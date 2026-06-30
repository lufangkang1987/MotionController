#include "HomeService.h"
#include "ZmcAdapter.h"
#include "../Types.h"           // Axis
#include "../../ParaSettings.h"   // AxisParams
#include <QSettings>
#include <QCoreApplication>
#include <QDebug>

static QString configFilePath()
{
    return QCoreApplication::applicationDirPath() + "/config.ini";
}

HomeService::HomeService(ZmcAdapter* adapter, QObject* parent)
    : QObject(parent), m_adapter(adapter)
{
}

// ==================== 发起回零 ====================

void HomeService::startHome(const AxisParams* cachedParams)
{
    if (!m_adapter || !m_adapter->isConnected()) return;
    if (m_state != HOME_IDLE) return;

    m_state = HOME_IDLE;  // configure 阶段不算 RUNNING，防止 tick 提前介入

    // 1. 读取配置
    QSettings settings(configFilePath(), QSettings::IniFormat);
    int datumMode = settings.value("Home/Mode", 4).toInt();

    // 2. 对四个轴下发参数并启动回零
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

        // 诊断日志
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

        // IO 口配置
        int datumIn = settings.value(QString("Home/DatumIn%1").arg(axis), -1).toInt();
        int fwdIn   = settings.value(QString("Home/FwdIn%1").arg(axis),   -1).toInt();
        int revIn   = settings.value(QString("Home/RevIn%1").arg(axis),   -1).toInt();

        if (fwdIn   >= 0) m_adapter->setFwdIn(axis, fwdIn);
        if (revIn   >= 0) m_adapter->setRevIn(axis, revIn);
        if (datumIn >= 0) m_adapter->setDatumIn(axis, datumIn);

        m_adapter->zeroPosition(axis);
        m_adapter->trigger();
        m_adapter->startDatum(axis, datumMode);
    }

    // 3. 进入异步等待状态
    m_state = HOME_RUNNING;
    m_elapsedMs = 0;
    emit homingStarted();
}

// ==================== 停止回零 ====================

void HomeService::stopHome()
{
    if (m_state != HOME_RUNNING) return;

    int axes[] = { Axis::X_AXIS, Axis::Y_MASTER, Axis::Z_FRONT_AXIS, Axis::Z_BACK_AXIS };
    for (int axis : axes)
        m_adapter->cancelAxis(axis, 2);

    m_state = HOME_IDLE;
    emit homingStopped();
}

// ==================== 每 100ms 状态机推进 ====================

void HomeService::tick()
{
    if (m_state != HOME_RUNNING) return;

    m_elapsedMs += 100;

    // 超时检查
    if (m_elapsedMs > kTimeoutMs)
    {
        int axes[] = { Axis::X_AXIS, Axis::Y_MASTER, Axis::Z_FRONT_AXIS, Axis::Z_BACK_AXIS };
        for (int axis : axes)
            m_adapter->cancelAxis(axis, 2);

        m_state = HOME_IDLE;
        emit homeFailed("回零超时（60秒），请检查：\n1. 原点传感器接线\n2. 回零方向是否正确\n3. 限位是否触发");
        return;
    }

    // 检查四个轴是否全部完成
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
        // 全部完成 → 位置归零
        for (int axis : axes)
            m_adapter->zeroPosition(axis);

        m_state = HOME_IDLE;
        emit homeCompleted();
    }
}
