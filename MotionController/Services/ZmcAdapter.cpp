#include "ZmcAdapter.h"

ZmcAdapter::ZmcAdapter(QObject* parent)
    : QObject(parent)
{
}

ZmcAdapter::~ZmcAdapter()
{
    disconnect();
}

// ==================== 连接管理 ====================

bool ZmcAdapter::connect(const QString& ip)
{
    int ret = ZAux_OpenEth(ip.toUtf8().data(), &m_handle);
    return (ret == ERR_OK);
}

void ZmcAdapter::disconnect()
{
    if (m_handle)
    {
        ZAux_Close(m_handle);
        m_handle = nullptr;
    }
}

// ==================== 位置/速度读取 ====================

float ZmcAdapter::getPosition(int axis)
{
    if (!m_handle) return 0;
    float pos = 0;
    ZAux_Direct_GetDpos(m_handle, axis, &pos);
    return pos;
}

float ZmcAdapter::getCurrentSpeed(int axis)
{
    if (!m_handle) return 0;
    float speed = 0;
    ZAux_Direct_GetVpSpeed(m_handle, axis, &speed);
    return speed;
}

int ZmcAdapter::getIdleState(int axis)
{
    if (!m_handle) return -1;
    int idle = -1;
    ZAux_Direct_GetIfIdle(m_handle, axis, &idle);
    return idle;
}

// ==================== 运动控制 ====================

void ZmcAdapter::moveAbsolute(int axis, float pos)
{
    if (!m_handle) return;
    ZAux_Direct_Single_MoveAbs(m_handle, axis, pos);
}

void ZmcAdapter::moveRelative(int axis, float dist)
{
    if (!m_handle) return;
    ZAux_Direct_Single_Move(m_handle, axis, dist);
}

void ZmcAdapter::moveVelocity(int axis, int dir)
{
    if (!m_handle) return;
    ZAux_Direct_Single_Vmove(m_handle, axis, dir);
}

void ZmcAdapter::moveMultiAbs(int count, const int* axes, const float* positions)
{
    if (!m_handle) return;
    ZAux_Direct_MoveAbs(m_handle, count, const_cast<int*>(axes), const_cast<float*>(positions));
}

void ZmcAdapter::cancelAxis(int axis, int mode)
{
    if (!m_handle) return;
    ZAux_Direct_Single_Cancel(m_handle, axis, mode);
}

// ==================== 轴参数设置 ====================

void ZmcAdapter::setAxisType(int axis, int type)
{
    if (!m_handle) return;
    ZAux_Direct_SetAtype(m_handle, axis, type);
}

void ZmcAdapter::setUnits(int axis, float val)
{
    if (!m_handle) return;
    ZAux_Direct_SetUnits(m_handle, axis, val);
}

void ZmcAdapter::setLowSpeed(int axis, float val)
{
    if (!m_handle) return;
    ZAux_Direct_SetLspeed(m_handle, axis, val);
}

void ZmcAdapter::setSpeed(int axis, float val)
{
    if (!m_handle) return;
    ZAux_Direct_SetSpeed(m_handle, axis, val);
}

void ZmcAdapter::setAccel(int axis, float val)
{
    if (!m_handle) return;
    ZAux_Direct_SetAccel(m_handle, axis, val);
}

void ZmcAdapter::setDecel(int axis, float val)
{
    if (!m_handle) return;
    ZAux_Direct_SetDecel(m_handle, axis, val);
}

void ZmcAdapter::setSramp(int axis, float val)
{
    if (!m_handle) return;
    ZAux_Direct_SetSramp(m_handle, axis, val);
}

void ZmcAdapter::setCreep(int axis, float val)
{
    if (!m_handle) return;
    ZAux_Direct_SetCreep(m_handle, axis, val);
}

void ZmcAdapter::setHomeWait(int axis, int ms)
{
    if (!m_handle) return;
    ZAux_Direct_SetHomeWait(m_handle, axis, ms);
}

void ZmcAdapter::setInvertStep(int axis, int invert)
{
    if (!m_handle) return;
    ZAux_Direct_SetInvertStep(m_handle, axis, invert);
}

// ==================== 位置清零 ====================

void ZmcAdapter::zeroPosition(int axis)
{
    if (!m_handle) return;
    ZAux_Direct_SetDpos(m_handle, axis, 0);
    ZAux_Direct_SetMpos(m_handle, axis, 0);
}

// ==================== 原点回零 ====================

void ZmcAdapter::setDatumIn(int axis, int io)
{
    if (!m_handle) return;
    ZAux_Direct_SetDatumIn(m_handle, axis, io);
}

void ZmcAdapter::setFwdIn(int axis, int io)
{
    if (!m_handle) return;
    ZAux_Direct_SetFwdIn(m_handle, axis, io);
}

void ZmcAdapter::setRevIn(int axis, int io)
{
    if (!m_handle) return;
    ZAux_Direct_SetRevIn(m_handle, axis, io);
}

void ZmcAdapter::trigger()
{
    if (!m_handle) return;
    ZAux_Trigger(m_handle);
}

void ZmcAdapter::startDatum(int axis, int mode)
{
    if (!m_handle) return;
    ZAux_Direct_Single_Datum(m_handle, axis, mode);
}

uint32_t ZmcAdapter::getHomeStatus(int axis)
{
    if (!m_handle) return 0;
    uint32_t status = 0;
    ZAux_Direct_GetHomeStatus(m_handle, axis, &status);
    return status;
}

// ==================== 参数回读 ====================

float ZmcAdapter::getParamUnits(int axis)
{
    if (!m_handle) return 0;
    float v = 0;
    ZAux_Direct_GetUnits(m_handle, axis, &v);
    return v;
}

float ZmcAdapter::getParamSpeed(int axis)
{
    if (!m_handle) return 0;
    float v = 0;
    ZAux_Direct_GetSpeed(m_handle, axis, &v);
    return v;
}

float ZmcAdapter::getParamAccel(int axis)
{
    if (!m_handle) return 0;
    float v = 0;
    ZAux_Direct_GetAccel(m_handle, axis, &v);
    return v;
}

float ZmcAdapter::getParamLowSpeed(int axis)
{
    if (!m_handle) return 0;
    float v = 0;
    ZAux_Direct_GetLspeed(m_handle, axis, &v);
    return v;
}

float ZmcAdapter::getParamCreep(int axis)
{
    if (!m_handle) return 0;
    float v = 0;
    ZAux_Direct_GetCreep(m_handle, axis, &v);
    return v;
}

int ZmcAdapter::getDatumIn(int axis)
{
    if (!m_handle) return -1;
    int v = -1;
    ZAux_Direct_GetDatumIn(m_handle, axis, &v);
    return v;
}

int ZmcAdapter::getFwdIn(int axis)
{
    if (!m_handle) return -1;
    int v = -1;
    ZAux_Direct_GetFwdIn(m_handle, axis, &v);
    return v;
}

int ZmcAdapter::getRevIn(int axis)
{
    if (!m_handle) return -1;
    int v = -1;
    ZAux_Direct_GetRevIn(m_handle, axis, &v);
    return v;
}
