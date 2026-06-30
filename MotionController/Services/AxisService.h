#pragma once

#include <QObject>
#include <QString>

class ZmcAdapter;
struct AxisParams;

// ============================================================================
// AxisService — 单轴运动操作服务层
// 封装正转/反转(连续+寸动)/停止/清零/参数下发/状态读取
// ============================================================================

class AxisService : public QObject
{
    Q_OBJECT

public:
    explicit AxisService(ZmcAdapter* adapter, QObject* parent = nullptr);

    // ---- 运动操作 (已封装"连续"复选框 + 定长"逻辑) ----
    void movePositive(int axis, bool isContinuous, float stepValue);
    void moveNegative(int axis, bool isContinuous, float stepValue);
    void stop(int axis);
    void clearPosition(int axis);

    // ---- 参数管理 ----
    void applyParameters(int axis, const AxisParams& params);
    void applyParametersFromConfig(AxisParams* outCache);  // 从 INI 批量读参并下发，同时回写缓存

    // ---- 状态读取 (含 dir 方向反转) ----
    struct AxisStatus {
        float position = 0;
        float speed = 0;
        int   idle = -1;  // -1=停止, 0=运动中
        QString stateText; // "停止"/"正转"/"反转"/"运行"/"未连接"
    };
    AxisStatus getStatus(int axis, const AxisParams& params);

private:
    ZmcAdapter* m_adapter;  // 构造函数注入
};
