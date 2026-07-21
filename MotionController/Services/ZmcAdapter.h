#pragma once

#include <QObject>
#include <QString>

#include "zmotion.h"
#include "zauxdll2.h"

// ============================================================================
// ZmcAdapter - adapter layer for the ZMC motion-control library.
// Wraps ZAux_* calls and exposes semantic controller operations.
// ============================================================================

class ZmcAdapter : public QObject
{
    Q_OBJECT

public:
    explicit ZmcAdapter(QObject* parent = nullptr);
    ~ZmcAdapter();

    // ---- Connection management ----
    bool connect(const QString& ip);
    void disconnect();
    bool isConnected() const { return m_handle != nullptr; }

    // ---- Status reads ----
    float getPosition(int axis);
    float getCurrentSpeed(int axis);       // Current measured axis speed.
    int   getIdleState(int axis);          // -1=stopped, 0=moving.

    // ---- Motion control ----
    void moveAbsolute(int axis, float pos); // Absolute move.
    void moveRelative(int axis, float dist);// Relative move.
    void moveVelocity(int axis, int dir);   // Velocity move, 1=positive, 0=negative.
    void moveMultiAbs(int count, const int* axes, const float* positions); // Multi-axis absolute move.
    void cancelAxis(int axis, int mode = 2); // Cancel motion.

    // ---- Parameter writes ----
    void setAxisType(int axis, int type);
    void setUnits(int axis, float val);
    void setLowSpeed(int axis, float val);
    void setSpeed(int axis, float val);
    void setAccel(int axis, float val);
    void setDecel(int axis, float val);
    void setSramp(int axis, float val);
    void setCreep(int axis, float val);
    void setHomeWait(int axis, int ms);
    void setInvertStep(int axis, int invert);

    // ---- Position control ----
    void zeroPosition(int axis);

    // ---- Output ports ----
    void setOutput(int port, bool on);      // Set output port state.
    bool getOutput(int port);               // Read output port state.

    // ---- Axis alarm ----
    bool isAxisAlarm(int axis);             // Return whether the axis has a real alarm.

    // ---- Homing control ----
    void setDatumIn(int axis, int io);
    void setFwdIn(int axis, int io);
    void setRevIn(int axis, int io);
    void trigger();                         // Trigger pending IN configuration changes.
    void startDatum(int axis, int mode);    // Start homing.
    uint32_t getHomeStatus(int axis);       // 0=not complete, 1=homing complete.

    // ---- Parameter reads ----
    float getParamUnits(int axis);
    float getParamSpeed(int axis);
    float getParamAccel(int axis);
    float getParamLowSpeed(int axis);
    float getParamCreep(int axis);
    int   getDatumIn(int axis);
    int   getFwdIn(int axis);
    int   getRevIn(int axis);

private:
    ZMC_HANDLE m_handle = nullptr;
};
