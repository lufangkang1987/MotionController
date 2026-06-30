#pragma once

#include <QObject>

class ZmcAdapter;
struct AxisParams;

// ============================================================================
// HomeService — 异步回零状态机
// 替代原 while+msleep 同步阻塞循环，改为 tick() 驱动
// ============================================================================

class HomeService : public QObject
{
    Q_OBJECT

public:
    explicit HomeService(ZmcAdapter* adapter, QObject* parent = nullptr);

    bool isHoming() const { return m_state != HOME_IDLE; }

    // 发起四轴回零（需传入参数缓存用于下发速度等配置）
    void startHome(const AxisParams* cachedParams);
    // 用户中止回零
    void stopHome();
    // 每 100ms 调用，驱动状态机
    void tick();

signals:
    void homingStarted();                                   // 回零已启动
    void homingStopped();                                   // 用户中止
    void homeCompleted();                                   // 全部轴回零成功
    void homeFailed(const QString& reason);                 // 超时/异常

private:
    enum State { HOME_IDLE, HOME_RUNNING };

    ZmcAdapter* m_adapter;
    State       m_state = HOME_IDLE;
    int         m_elapsedMs = 0;    // 已等待毫秒数
    static constexpr int kTimeoutMs = 60000;

    void configureAllAxes(const AxisParams* cachedParams);
};
