#pragma once

#include <QObject>
#include <QString>

class ZmcAdapter;
struct AxisParams;

// ============================================================================
// AxisService - service layer for single-axis motion operations.
// Wraps forward/reverse motion, jogging, stop, clear position, parameter writes, and status reads.
// ============================================================================

class AxisService : public QObject
{
    Q_OBJECT

public:
    explicit AxisService(ZmcAdapter* adapter, QObject* parent = nullptr);

    // ---- Motion operations. Wraps continuous mode and fixed-distance jog logic. ----
    void movePositive(int axis, bool isContinuous, float stepValue);
    void moveNegative(int axis, bool isContinuous, float stepValue);
    void stop(int axis);
    void clearPosition(int axis);

    // ---- Parameter writes ----
    void applyParameters(int axis, const AxisParams& params);
    void applyParametersFromConfig(AxisParams* outCache);  // Load INI parameters into cache and write them if connected.

    // ---- Status reads, including direction reversal from config. ----
    struct AxisStatus {
        float position = 0;
        float speed = 0;
        int   idle = -1;  // -1=stopped, 0=moving.
        QString stateText; // Disconnected / Stopped / Moving + / Moving - / Idle / Unknown.
    };
    AxisStatus getStatus(int axis, const AxisParams& params);

private:
    ZmcAdapter* m_adapter;  // Motion controller adapter.
};
