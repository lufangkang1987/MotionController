#include "AxisService.h"
#include "ZmcAdapter.h"
#include "../Types.h"           // Axis, AxisConfig
#include "../../ParaSettings.h"   // AxisParams
#include <QSettings>
#include <QCoreApplication>

static QString configFilePath()
{
    return QCoreApplication::applicationDirPath() + "/config.ini";
}

AxisService::AxisService(ZmcAdapter* adapter, QObject* parent)
    : QObject(parent), m_adapter(adapter)
{
}

// ==================== 运动操作 ====================

void AxisService::movePositive(int axis, bool isContinuous, float stepValue)
{
    if (!m_adapter || !m_adapter->isConnected()) return;

    if (isContinuous)
        m_adapter->moveVelocity(axis, 1);       // 正向连续点动
    else
        m_adapter->moveRelative(axis, stepValue); // 正向寸动
}

void AxisService::moveNegative(int axis, bool isContinuous, float stepValue)
{
    if (!m_adapter || !m_adapter->isConnected()) return;

    if (isContinuous)
        m_adapter->moveVelocity(axis, 0);        // 反向连续点动
    else
        m_adapter->moveRelative(axis, -stepValue); // 反向寸动
}

void AxisService::stop(int axis)
{
    if (!m_adapter || !m_adapter->isConnected()) return;
    m_adapter->cancelAxis(axis, 2);
}

void AxisService::clearPosition(int axis)
{
    if (!m_adapter || !m_adapter->isConnected()) return;
    m_adapter->zeroPosition(axis);
}

// ==================== 参数管理 ====================

void AxisService::applyParameters(int axis, const AxisParams& params)
{
    if (!m_adapter || !m_adapter->isConnected()) return;

    m_adapter->setAxisType(axis, 65);
    m_adapter->setUnits(axis, params.m_units);
    m_adapter->setLowSpeed(axis, params.m_lspeed);
    m_adapter->setSpeed(axis, params.m_speed);
    m_adapter->setAccel(axis, params.m_acc);
    m_adapter->setDecel(axis, params.m_dec);
    m_adapter->setSramp(axis, params.m_sramp);
    m_adapter->setInvertStep(axis, params.dir);
}

void AxisService::applyParametersFromConfig(AxisParams* outCache)
{
    int axes[] = { Axis::X_AXIS, Axis::Y_MASTER, Axis::Z_FRONT_AXIS, Axis::Z_BACK_AXIS };
    QSettings settings(configFilePath(), QSettings::IniFormat);

    for (int axis : axes)
    {
        QString group = QString("Axis%1").arg(axis);
        settings.beginGroup(group);
        AxisParams params;
        params.m_units  = settings.value("units",  1.0f).toFloat();
        params.m_lspeed = settings.value("lspeed", 0.0f).toFloat();
        params.m_speed  = settings.value("speed",  100.0f).toFloat();
        params.m_acc    = settings.value("acc",    3000.0f).toFloat();
        params.m_dec    = settings.value("dec",    3000.0f).toFloat();
        params.m_sramp  = settings.value("sramp",  10.0f).toFloat();
        params.dir      = settings.value("dir",    0).toInt();
        settings.endGroup();

        // 回写到调用方缓存（未连接时仅缓存不下发）
        if (outCache)
            outCache[axis] = params;

        applyParameters(axis, params);
    }
}

// ==================== 状态读取 ====================

AxisService::AxisStatus AxisService::getStatus(int axis, const AxisParams& params)
{
    AxisStatus s;

    if (!m_adapter || !m_adapter->isConnected())
    {
        s.stateText = "未连接";
        return s;
    }

    s.position = m_adapter->getPosition(axis);
    s.speed    = m_adapter->getCurrentSpeed(axis);
    s.idle     = m_adapter->getIdleState(axis);

    // 方向反转显示
    if (params.dir == 1)
    {
        s.position = -s.position;
        s.speed    = -s.speed;
    }

    // 状态文本
    if (s.idle == -1)
        s.stateText = "停止";
    else if (s.idle == 0)
    {
        if (s.speed > 0.001f)
            s.stateText = "正转";
        else if (s.speed < -0.001f)
            s.stateText = "反转";
        else
            s.stateText = "运行";
    }
    else
        s.stateText = "未知";

    return s;
}
