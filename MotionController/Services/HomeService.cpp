#include "HomeService.h"
#include "ZmcAdapter.h"
#include "../Types.h"           // Axis
#include "../../ParaSettings.h"   // AxisParams
#include <QSettings>
#include <QCoreApplication>
#include <QDebug>
#include <QStringList>

static QString configFilePath()
{
    return QCoreApplication::applicationDirPath() + "/config.ini";
}

HomeService::HomeService(ZmcAdapter* adapter, QObject* parent)
    : QObject(parent), m_adapter(adapter)
{
}

// ==================== Start homing ====================

void HomeService::startHome(const AxisParams* cachedParams)
{
    if (!m_adapter || !m_adapter->isConnected())
    {
        qWarning() << "[HOME] start rejected: controller is not connected";
        return;
    }

    if (m_state != HOME_IDLE)
    {
        qWarning() << "[HOME] start rejected: home is already running";
        return;
    }

    m_state = HOME_IDLE;  // Keep IDLE until all axes are configured, then enter RUNNING.

    // 1. Read homing config.
    QSettings settings(configFilePath(), QSettings::IniFormat);
    int datumMode = settings.value("Home/Mode", 4).toInt();
    qInfo() << "[HOME] start requested. config=" << configFilePath()
        << "mode=" << datumMode
        << "timeoutMs=" << kTimeoutMs;

    // 2. Write parameters to all four axes and start homing.
    int axes[] = { Axis::X_AXIS, Axis::Y_MASTER, Axis::Z_FRONT_AXIS, Axis::Z_BACK_AXIS };

    for (int axis : axes)
    {
        const AxisParams& p = cachedParams[axis];

        m_adapter->setAxisType(axis, 65);
        m_adapter->setUnits(axis, p.m_units);
        m_adapter->setLowSpeed(axis, p.m_lspeed);
        m_adapter->setSpeed(axis, p.m_speed);
        m_adapter->setAccel(axis, p.m_acc);
        m_adapter->setDecel(axis, p.m_dec);
        m_adapter->setSramp(axis, p.m_sramp);
        m_adapter->setCreep(axis, 10.0f);
        m_adapter->setHomeWait(axis, 1000);

        // Log configured axis parameters and home IO mapping.
        qDebug() << "[HOME] axis" << axis
            << "units=" << m_adapter->getParamUnits(axis)
            << "lspeed=" << m_adapter->getParamLowSpeed(axis)
            << "speed=" << m_adapter->getParamSpeed(axis)
            << "acc=" << m_adapter->getParamAccel(axis)
            << "creep=" << m_adapter->getParamCreep(axis)
            << "DatumIn=" << m_adapter->getDatumIn(axis)
            << "FwdIn=" << m_adapter->getFwdIn(axis)
            << "RevIn=" << m_adapter->getRevIn(axis)
            << "mode=" << datumMode;

        // IO mapping.
        int datumIn = settings.value(QString("Home/DatumIn%1").arg(axis), -1).toInt();
        int fwdIn   = settings.value(QString("Home/FwdIn%1").arg(axis),   -1).toInt();
        int revIn   = settings.value(QString("Home/RevIn%1").arg(axis),   -1).toInt();

        qInfo() << "[HOME] configure axis" << axis
            << "units=" << p.m_units
            << "lspeed=" << p.m_lspeed
            << "speed=" << p.m_speed
            << "acc=" << p.m_acc
            << "dec=" << p.m_dec
            << "sramp=" << p.m_sramp
            << "datumIn=" << datumIn
            << "fwdIn=" << fwdIn
            << "revIn=" << revIn;

        if (fwdIn   >= 0) m_adapter->setFwdIn(axis, fwdIn);
        if (revIn   >= 0) m_adapter->setRevIn(axis, revIn);
        if (datumIn >= 0) m_adapter->setDatumIn(axis, datumIn);

        m_adapter->zeroPosition(axis);
        m_adapter->trigger();
        m_adapter->startDatum(axis, datumMode);
    }

    // 3. Enter asynchronous wait state.
    m_state = HOME_RUNNING;
    m_elapsedMs = 0;
    qInfo() << "[HOME] running";
    emit homingStarted();
}

// ==================== Stop homing ====================

void HomeService::stopHome()
{
    if (m_state != HOME_RUNNING) return;

    qWarning() << "[HOME] stop requested by user";

    int axes[] = { Axis::X_AXIS, Axis::Y_MASTER, Axis::Z_FRONT_AXIS, Axis::Z_BACK_AXIS };
    for (int axis : axes)
        m_adapter->cancelAxis(axis, 2);

    m_state = HOME_IDLE;
    emit homingStopped();
}

// ==================== 100ms state machine tick ====================

void HomeService::tick()
{
    if (m_state != HOME_RUNNING) return;

    m_elapsedMs += 100;

    // Timeout check.
    if (m_elapsedMs > kTimeoutMs)
    {
        int axes[] = { Axis::X_AXIS, Axis::Y_MASTER, Axis::Z_FRONT_AXIS, Axis::Z_BACK_AXIS };
        QStringList unfinishedAxes;

        for (int axis : axes)
        {
            uint32_t homestatus = m_adapter->getHomeStatus(axis);
            int idle = m_adapter->getIdleState(axis);
            float dpos = m_adapter->getPosition(axis);
            float mspeed = m_adapter->getCurrentSpeed(axis);

            qCritical() << "[HOME] timeout snapshot"
                << "axis" << axis
                << "homestatus=" << homestatus
                << "idle=" << idle
                << "dpos=" << dpos
                << "mspeed=" << mspeed
                << "DatumIn=" << m_adapter->getDatumIn(axis)
                << "FwdIn=" << m_adapter->getFwdIn(axis)
                << "RevIn=" << m_adapter->getRevIn(axis);

            if (homestatus != 1)
                unfinishedAxes << QString::number(axis);
        }

        qCritical() << "[HOME] timeout. unfinished axes:" << unfinishedAxes.join(",");

        for (int axis : axes)
            m_adapter->cancelAxis(axis, 2);

        m_state = HOME_IDLE;
        emit homeFailed("Home timeout after 60 seconds. Check: 1. limit/home switch wiring; 2. homing direction and IO mapping; 3. controller alarm or axis motion state.");
        return;
    }

    // Check whether all four axes have completed homing.
    int axes[] = { Axis::X_AXIS, Axis::Y_MASTER, Axis::Z_FRONT_AXIS, Axis::Z_BACK_AXIS };
    bool allDone = true;

    for (int axis : axes)
    {
        uint32_t homestatus = m_adapter->getHomeStatus(axis);
        int idle = m_adapter->getIdleState(axis);
        float dpos = m_adapter->getPosition(axis);
        float mspeed = m_adapter->getCurrentSpeed(axis);

        qDebug() << "[HOME] tick" << m_elapsedMs
            << "axis" << axis
            << "homestatus=" << homestatus
            << "idle=" << idle
            << "dpos=" << dpos
            << "mspeed=" << mspeed;

        if (homestatus != 1)
        {
            allDone = false;
            break;
        }
    }

    if (allDone)
    {
        // Homing completed. Zero all axis positions.
        for (int axis : axes)
            m_adapter->zeroPosition(axis);

        m_state = HOME_IDLE;
        qInfo() << "[HOME] completed";
        emit homeCompleted();
    }
}
