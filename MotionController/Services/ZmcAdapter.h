#pragma once

#include <QObject>
#include <QString>

#include "zmotion.h"
#include "zauxdll2.h"

// ============================================================================
// ZmcAdapter — 正运动控制 ZMC 库适配器层
// 封装所有 ZAux_* 库函数调用，对外暴露语义化方法
// ============================================================================

class ZmcAdapter : public QObject
{
    Q_OBJECT

public:
    explicit ZmcAdapter(QObject* parent = nullptr);
    ~ZmcAdapter();

    // ---- 连接管理 ----
    bool connect(const QString& ip);
    void disconnect();
    bool isConnected() const { return m_handle != nullptr; }

    // ---- 位置/速度读取 ----
    float getPosition(int axis);
    float getCurrentSpeed(int axis);       // VpSpeed（实际运行速度）
    int   getIdleState(int axis);          // -1=停止, 0=运动中

    // ---- 运动控制 ----
    void moveAbsolute(int axis, float pos); // 绝对定位
    void moveRelative(int axis, float dist);// 相对运动（寸动）
    void moveVelocity(int axis, int dir);   // 连续速度运动 (1=正, 0=反)
    void moveMultiAbs(int count, const int* axes, const float* positions); // 多轴联动
    void cancelAxis(int axis, int mode = 2); // 减速停止

    // ---- 轴参数设置 ----
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

    // ---- 位置清零 ----
    void zeroPosition(int axis);

    // ---- 原点回零 ----
    void setDatumIn(int axis, int io);
    void setFwdIn(int axis, int io);
    void setRevIn(int axis, int io);
    void trigger();                         // 使 IN 口配置生效
    void startDatum(int axis, int mode);    // 启动回零
    uint32_t getHomeStatus(int axis);       // 0=未完成, 1=回零成功

    // ---- 参数回读 ----
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
