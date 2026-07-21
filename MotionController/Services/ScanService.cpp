#include "ScanService.h"
#include "ZmcAdapter.h"
#include "../Types.h"           // ScanState, Axis
#include "../../ParaSettings.h"   // AxisParams
#include <cmath>

ScanService::ScanService(ZmcAdapter* adapter, QObject* parent)
    : QObject(parent), m_adapter(adapter)
{
}

// ==================== Start scan ====================

void ScanService::startScan(bool isContinuous,
                             const float* startPoint,
                             const float* endPoint,
                             float stepGap,
                             int totalLines,
                             const AxisParams* cachedParams)
{
    if (!m_adapter || !m_adapter->isConnected()) return;
    if (hasActiveScan()) return;

    m_params       = cachedParams;
    m_isContinuous = isContinuous;
    m_scanStartX   = startPoint[0];
    m_scanEndX     = endPoint[0];
    m_scanStartY   = startPoint[1];
    m_scanEndY     = endPoint[1];
    m_stepGap      = stepGap;
    m_totalLines   = totalLines;
    m_currentLine  = 0;
    m_forward      = true;
    m_paused       = false;
    m_remainingTime = 0;

    // Move to the saved scan start point first.
    m_scanState = SCAN_MOVE_TO_START;
    m_adapter->moveAbsolute(Axis::X_AXIS, m_scanStartX);
    m_adapter->moveAbsolute(Axis::Y_MASTER, m_scanStartY);

    emit scanStarted();
}

void ScanService::pauseScan()
{
    if (!isRunning()) return;

    m_adapter->cancelAxis(Axis::X_AXIS, 2);
    m_adapter->cancelAxis(Axis::Y_MASTER, 2);
    m_paused = true;
    emit scanPaused();
}

void ScanService::resumeScan()
{
    if (!m_paused) return;

    m_paused = false;
    resumeCurrentMove();
    emit scanResumed();
}

void ScanService::stopScan()
{
    if (!hasActiveScan()) return;

    m_scanState = SCAN_IDLE;
    m_paused = false;
    m_adapter->cancelAxis(Axis::X_AXIS, 2);
    m_adapter->cancelAxis(Axis::Y_MASTER, 2);
    emit scanStopped();
}

// ==================== State machine tick ====================

void ScanService::tick()
{
    if (!isRunning()) return;

    int xIdle = m_adapter->getIdleState(Axis::X_AXIS);
    int yIdle = m_adapter->getIdleState(Axis::Y_MASTER);
    bool xStop = (xIdle == -1);
    bool yStop = (yIdle == -1);

    switch (m_scanState)
    {
    case SCAN_MOVE_TO_START:
        if (xStop && yStop)
        {
            if (m_isContinuous)
            {
                // Continuous scan: move X/Y together to the end point.
                int axisList[2] = { Axis::X_AXIS, Axis::Y_MASTER };
                float posList[2] = { m_scanEndX, m_scanEndY };
                m_adapter->moveMultiAbs(2, axisList, posList);
                m_scanState = SCAN_CONTINUOUS;
            }
            else
            {
                // Raster scan: start the first line in the positive X direction.
                m_currentLine = 0;
                m_forward = true;
                m_scanState = SCAN_FORWARD;
                m_adapter->moveAbsolute(Axis::X_AXIS, m_scanEndX);
            }
        }
        break;

    case SCAN_CONTINUOUS:
        if (xStop && yStop)
        {
            m_scanState = SCAN_IDLE;
            emit scanCompleted();
        }
        break;

    case SCAN_FORWARD:
        if (xStop)
        {
            m_currentLine++;
            if (m_currentLine >= m_totalLines)
            {
                m_scanState = SCAN_IDLE;
                emit scanCompleted();
                return;
            }
            m_scanState = SCAN_STEP;
            float ratio = (m_totalLines > 1) ? (float)m_currentLine / (m_totalLines - 1) : 1.0f;
            float yTarget = m_scanStartY + (m_scanEndY - m_scanStartY) * ratio;
            m_adapter->moveAbsolute(Axis::Y_MASTER, yTarget);
        }
        break;

    case SCAN_REVERSE:
        if (xStop)
        {
            m_currentLine++;
            if (m_currentLine >= m_totalLines)
            {
                m_scanState = SCAN_IDLE;
                emit scanCompleted();
                return;
            }
            m_scanState = SCAN_STEP;
            float ratio = (m_totalLines > 1) ? (float)m_currentLine / (m_totalLines - 1) : 1.0f;
            float yTarget = m_scanStartY + (m_scanEndY - m_scanStartY) * ratio;
            m_adapter->moveAbsolute(Axis::Y_MASTER, yTarget);
        }
        break;

    case SCAN_STEP:
        if (yStop)
        {
            m_forward = !m_forward;
            if (m_forward)
            {
                m_scanState = SCAN_FORWARD;
                m_adapter->moveAbsolute(Axis::X_AXIS, m_scanEndX);
            }
            else
            {
                m_scanState = SCAN_REVERSE;
                m_adapter->moveAbsolute(Axis::X_AXIS, m_scanStartX);
            }
        }
        break;

    default:
        break;
    }

    // Update remaining time estimate.
    updateRemainingTime();
}

void ScanService::resumeCurrentMove()
{
    switch (m_scanState)
    {
    case SCAN_MOVE_TO_START:
        m_adapter->moveAbsolute(Axis::X_AXIS, m_scanStartX);
        m_adapter->moveAbsolute(Axis::Y_MASTER, m_scanStartY);
        break;

    case SCAN_CONTINUOUS:
    {
        int axisList[2] = { Axis::X_AXIS, Axis::Y_MASTER };
        float posList[2] = { m_scanEndX, m_scanEndY };
        m_adapter->moveMultiAbs(2, axisList, posList);
        break;
    }

    case SCAN_FORWARD:
        m_adapter->moveAbsolute(Axis::X_AXIS, m_scanEndX);
        break;

    case SCAN_REVERSE:
        m_adapter->moveAbsolute(Axis::X_AXIS, m_scanStartX);
        break;

    case SCAN_STEP:
    {
        float ratio = (m_totalLines > 1) ? (float)m_currentLine / (m_totalLines - 1) : 1.0f;
        float yTarget = m_scanStartY + (m_scanEndY - m_scanStartY) * ratio;
        m_adapter->moveAbsolute(Axis::Y_MASTER, yTarget);
        break;
    }

    default:
        break;
    }
}

// ==================== Remaining time estimate ==================== 

float ScanService::estimateMoveTime(float distance, float speed, float acc, float dec)
{
    distance = qAbs(distance);
    if (distance < 0.001f || speed <= 0 || acc <= 0 || dec <= 0) return 0;

    float t_acc = speed / acc;
    float d_acc = 0.5f * acc * t_acc * t_acc;
    float t_dec = speed / dec;
    float d_dec = 0.5f * dec * t_dec * t_dec;

    if (distance >= d_acc + d_dec)
    {
        float d_cruise = distance - d_acc - d_dec;
        return t_acc + d_cruise / speed + t_dec;
    }
    else
    {
        float v_peak = sqrt(2.0f * distance * acc * dec / (acc + dec));
        return v_peak / acc + v_peak / dec;
    }
}

void ScanService::updateRemainingTime()
{
    if (!isRunning() || !m_params) return;

    float xDist = qAbs(m_scanEndX - m_scanStartX);
    const AxisParams& xP = m_params[Axis::X_AXIS];
    const AxisParams& yP = m_params[Axis::Y_MASTER];

    float xFullTime = estimateMoveTime(xDist, xP.m_speed, xP.m_acc, xP.m_dec);
    float yStepTime = estimateMoveTime(m_stepGap, yP.m_speed, yP.m_acc, yP.m_dec);

    int remainingLines = m_totalLines - m_currentLine;
    float remaining = 0;

    switch (m_scanState)
    {
    case SCAN_MOVE_TO_START:
    {
        float curX = m_adapter->getPosition(Axis::X_AXIS);
        float curY = m_adapter->getPosition(Axis::Y_MASTER);
        float xMoveTime = estimateMoveTime(m_scanStartX - curX, xP.m_speed, xP.m_acc, xP.m_dec);
        float yMoveTime = estimateMoveTime(m_scanStartY - curY, yP.m_speed, yP.m_acc, yP.m_dec);
        remaining = qMax(xMoveTime, yMoveTime)
            + m_totalLines * xFullTime
            + (m_totalLines - 1) * yStepTime;
        break;
    }

    case SCAN_FORWARD:
    case SCAN_REVERSE:
    {
        float curX = m_adapter->getPosition(Axis::X_AXIS);
        float targetX = m_forward ? m_scanEndX : m_scanStartX;
        float xRemTime = estimateMoveTime(qAbs(targetX - curX), xP.m_speed, xP.m_acc, xP.m_dec);
        remaining = xRemTime
            + (remainingLines - 1) * xFullTime
            + (remainingLines - 1) * yStepTime;
        break;
    }

    case SCAN_STEP:
    {
        float curY = m_adapter->getPosition(Axis::Y_MASTER);
        float ratio = (m_totalLines > 1) ? (float)m_currentLine / (m_totalLines - 1) : 1.0f;
        float yTarget = m_scanStartY + (m_scanEndY - m_scanStartY) * ratio;
        float yRemTime = estimateMoveTime(qAbs(yTarget - curY), yP.m_speed, yP.m_acc, yP.m_dec);
        remaining = yRemTime
            + (remainingLines - 1) * xFullTime
            + (remainingLines - 1) * yStepTime;
        break;
    }

    case SCAN_CONTINUOUS:
    {
        float curX = m_adapter->getPosition(Axis::X_AXIS);
        float curY = m_adapter->getPosition(Axis::Y_MASTER);
        float xRemTime = estimateMoveTime(qAbs(m_scanEndX - curX), xP.m_speed, xP.m_acc, xP.m_dec);
        float yRemTime = estimateMoveTime(qAbs(m_scanEndY - curY), yP.m_speed, yP.m_acc, yP.m_dec);
        remaining = qMax(xRemTime, yRemTime);
        break;
    }

    default:
        break;
    }

    m_remainingTime = remaining;
}
