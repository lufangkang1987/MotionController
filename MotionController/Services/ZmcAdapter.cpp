#include "ZmcAdapter.h"
#include <QDebug>

ZmcAdapter::ZmcAdapter(QObject* parent)
    : QObject(parent)
{
}

ZmcAdapter::~ZmcAdapter()
{
    disconnect();
}

// ==================== Connection management ====================

bool ZmcAdapter::connect(const QString& ip)
{
    int ret = ZAux_OpenEth(ip.toUtf8().data(), &m_handle);
    if (ret == ERR_OK)
        qInfo() << "[ZMC] connected:" << ip;
    else
        qWarning() << "[ZMC] connect failed:" << ip << "ret=" << ret;
    return (ret == ERR_OK);
}

void ZmcAdapter::disconnect()
{
    if (m_handle)
    {
        qInfo() << "[ZMC] disconnected";
        ZAux_Close(m_handle);
        m_handle = nullptr;
    }
}

// ==================== Status reads ====================

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

// ==================== Motion control ====================

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
    int ret = ZAux_Direct_Single_Cancel(m_handle, axis, mode);
    if (ret != 0)
        qWarning() << "[ZMC] cancelAxis failed: axis=" << axis << "mode=" << mode << "ret=" << ret;
}

// ==================== Parameter writes ====================

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

// ==================== Position control ====================

void ZmcAdapter::zeroPosition(int axis)
{
    if (!m_handle) return;
    ZAux_Direct_SetDpos(m_handle, axis, 0);
    ZAux_Direct_SetMpos(m_handle, axis, 0);
}

// ==================== Output ports ====================

void ZmcAdapter::setOutput(int port, bool on)
{
	if (!m_handle) return;
	int ret1 = ZAux_Direct_SetOp(m_handle, port, on ? 1 : 0);
	if (ret1 != 0)
	{
        // SetOp may fail on some controllers, so fall back to DirectCommand Basic output assignment.
		char buf[64] = {};
		QString cmd = QString("OP(%1,%2)").arg(port).arg(on ? 1 : 0);
		int ret2 = ZAux_DirectCommand(m_handle, cmd.toUtf8().data(), buf, sizeof(buf));
		if (ret2 != 0)
			qWarning() << "setOutput failed: port=" << port << "on=" << on
			           << "SetOp ret=" << ret1 << "DirectCommand ret=" << ret2;
	}
}

bool ZmcAdapter::getOutput(int port)
{
	if (!m_handle) return false;
	uint32_t val = 0;
	ZAux_Direct_GetOp(m_handle, port, &val);
	return val != 0;
}

// ==================== Axis alarm ====================

bool ZmcAdapter::isAxisAlarm(int axis)
{
	if (!m_handle) return false;

    // Prefer ZMC_GetAxisStates and read m_AlarmState from the returned structure.
	struct_AxisStates states = {};
	/*int ret = ZMC_GetAxisStates(m_handle, axis, &states);
	if (ret == 0)
	{
        bool alarm = (states.m_AlarmState != 0);
		if (alarm)
			qDebug() << "[ALARM] axis" << axis << "m_AlarmState=" << states.m_AlarmState
				<< "HomeState=" << states.m_HomeState
				<< "ElDec=" << states.m_ElDecState << "ElPlus=" << states.m_ElPlusState;
		return alarm;
	}*/

    // Fall back to ZAux_Direct_GetAxisStatus and check the AXISSTATUS register.
    // AXISSTATUS: bit0=stopped, bit1=moving, bit2=homing, bit3=waiting.
    //             bit4=positive limit, bit5=negative limit, bit6=soft positive limit, bit7=soft negative limit.
    //             bit8+ indicates following error, alarm, or other real fault states.
	int status = 0;
	int ret = ZAux_Direct_GetAxisStatus(m_handle, axis, &status);
	if (ret == 0)
	{
        bool alarm = (status & 0xFFFFFF00) != 0;  // Treat bit8+ as real alarm/fault states.
		if (alarm)
			qDebug() << "[ALARM] axis" << axis << "AXISSTATUS=0x" << Qt::hex << status
				<< "alarm bits=0x" << (status & 0xFFFFFF00);
		return alarm;
	}

	qWarning() << "isAxisAlarm failed: axis=" << axis << "ret=" << ret;
	return false;
}

// ==================== Homing control ====================

void ZmcAdapter::setDatumIn(int axis, int io)
{
    if (!m_handle) return;
    int ret = ZAux_Direct_SetDatumIn(m_handle, axis, io);
    if (ret != 0)
        qWarning() << "[ZMC] setDatumIn failed: axis=" << axis << "io=" << io << "ret=" << ret;
}

void ZmcAdapter::setFwdIn(int axis, int io)
{
    if (!m_handle) return;
    int ret = ZAux_Direct_SetFwdIn(m_handle, axis, io);
    if (ret != 0)
        qWarning() << "[ZMC] setFwdIn failed: axis=" << axis << "io=" << io << "ret=" << ret;
}

void ZmcAdapter::setRevIn(int axis, int io)
{
    if (!m_handle) return;
    int ret = ZAux_Direct_SetRevIn(m_handle, axis, io);
    if (ret != 0)
        qWarning() << "[ZMC] setRevIn failed: axis=" << axis << "io=" << io << "ret=" << ret;
}

void ZmcAdapter::trigger()
{
    if (!m_handle) return;
    int ret = ZAux_Trigger(m_handle);
    if (ret != 0)
        qWarning() << "[ZMC] trigger failed: ret=" << ret;
}

void ZmcAdapter::startDatum(int axis, int mode)
{
    if (!m_handle) return;
    int ret = ZAux_Direct_Single_Datum(m_handle, axis, mode);
    if (ret == 0)
        qInfo() << "[ZMC] startDatum: axis=" << axis << "mode=" << mode;
    else
        qWarning() << "[ZMC] startDatum failed: axis=" << axis << "mode=" << mode << "ret=" << ret;
}

uint32_t ZmcAdapter::getHomeStatus(int axis)
{
    if (!m_handle) return 0;
    uint32_t status = 0;
    int ret = ZAux_Direct_GetHomeStatus(m_handle, axis, &status);
    if (ret != 0)
        qWarning() << "[ZMC] getHomeStatus failed: axis=" << axis << "ret=" << ret;
    return status;
}

// ==================== Parameter reads ====================

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
