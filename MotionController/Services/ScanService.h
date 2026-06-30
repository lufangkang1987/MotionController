#pragma once

#include <QObject>
#include "../Types.h"            // ScanState, Axis

class ZmcAdapter;
struct AxisParams;

// ============================================================================
// ScanService — 扫描状态机服务层
// 从 MotionController 抽离栅格扫查/连续扫查 + 剩余时间估算
// ============================================================================

class ScanService : public QObject
{
    Q_OBJECT

public:
    explicit ScanService(ZmcAdapter* adapter, QObject* parent = nullptr);

    bool isRunning() const { return m_scanState != SCAN_IDLE && m_scanState != SCAN_DONE; }
    ScanState state() const { return m_scanState; }

    // ---- 启动/停止 ----
    void startScan(bool isContinuous,                       // 连续扫查 vs 栅格扫查
                   const float* startPoint,                 // [4]: X/Y/ZQ/ZH
                   const float* endPoint,
                   float stepGap,                           // 步进间隔
                   int   totalLines,
                   const AxisParams* cachedParams);         // 供剩余时间估算
    void stopScan();

    // 每 100ms 驱动状态机
    void tick();

    // ---- 剩余时间 ----
    float getRemainingTime() const { return m_remainingTime; }

signals:
    void scanCompleted();
    void scanStarted();
    void scanStopped();

private:
    ZmcAdapter* m_adapter;

    ScanState m_scanState = SCAN_IDLE;

    // 扫描参数
    float m_scanStartX = 0, m_scanEndX = 0;
    float m_scanStartY = 0, m_scanEndY = 0;
    float m_stepGap = 1;
    int   m_totalLines = 1;
    int   m_currentLine = 0;
    bool  m_forward = true;
    bool  m_isContinuous = false;

    float m_remainingTime = 0;

    const AxisParams* m_params = nullptr;  // 不持有，仅引用

    // ---- 内部辅助 ----
    float estimateMoveTime(float distance, float speed, float acc, float dec);
    void  updateRemainingTime();
};
