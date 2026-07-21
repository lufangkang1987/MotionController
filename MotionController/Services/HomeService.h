#pragma once

#include <QObject>

class ZmcAdapter;
struct AxisParams;

// ============================================================================
// HomeService - asynchronous homing state machine.
// Replaces the old blocking while/msleep loop with timer-driven tick processing.
// ============================================================================

class HomeService : public QObject
{
    Q_OBJECT

public:
    explicit HomeService(ZmcAdapter* adapter, QObject* parent = nullptr);

    bool isHoming() const { return m_state != HOME_IDLE; }

    // Start four-axis homing. Axis parameters are supplied for speed and limit settings.
    void startHome(const AxisParams* cachedParams);
    // User-requested homing stop.
    void stopHome();
    // Called every 100ms by the UI timer.
    void tick();

signals:
    void homingStarted();                                   // Homing started.
    void homingStopped();                                   // Homing stopped by user.
    void homeCompleted();                                   // Homing completed successfully.
    void homeFailed(const QString& reason);                 // Homing failed.

private:
    enum State { HOME_IDLE, HOME_RUNNING };

    ZmcAdapter* m_adapter;
    State       m_state = HOME_IDLE;
    int         m_elapsedMs = 0;    // Elapsed homing time in milliseconds.
    static constexpr int kTimeoutMs = 60000;

    void configureAllAxes(const AxisParams* cachedParams);
};
