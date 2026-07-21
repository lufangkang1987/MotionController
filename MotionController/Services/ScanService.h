#pragma once

#include <QObject>
#include "../Types.h"            // ScanState, Axis

class ZmcAdapter;
struct AxisParams;

// ============================================================================
// ScanService - scan state machine service layer.
// Extracts raster/continuous scan logic and remaining-time estimation from MotionController.
// ============================================================================

class ScanService : public QObject
{
    Q_OBJECT

public:
    explicit ScanService(ZmcAdapter* adapter, QObject* parent = nullptr);

    bool isRunning() const { return !m_paused && m_scanState != SCAN_IDLE && m_scanState != SCAN_DONE; }
    bool isPaused() const { return m_paused; }
    bool hasActiveScan() const { return m_paused || isRunning(); }
    ScanState state() const { return m_scanState; }

    // ---- Start, pause, resume, and stop ----
    void startScan(bool isContinuous,                       // Continuous scan vs raster scan.
                   const float* startPoint,                 // [4]: X/Y/ZQ/ZH
                   const float* endPoint,
                   float stepGap,                           // Raster step gap.
                   int   totalLines,
                   const AxisParams* cachedParams);         // Used for remaining-time estimation.
    void pauseScan();
    void resumeScan();
    void stopScan();

    // Drive the state machine every 100ms.
    void tick();

    // ---- Remaining time ----
    float getRemainingTime() const { return m_remainingTime; }

signals:
    void scanCompleted();
    void scanStarted();
    void scanPaused();
    void scanResumed();
    void scanStopped();

private:
    ZmcAdapter* m_adapter;

    ScanState m_scanState = SCAN_IDLE;

    // Scan parameters.
    float m_scanStartX = 0, m_scanEndX = 0;
    float m_scanStartY = 0, m_scanEndY = 0;
    float m_stepGap = 1;
    int   m_totalLines = 1;
    int   m_currentLine = 0;
    bool  m_forward = true;
    bool  m_isContinuous = false;
    bool  m_paused = false;

    float m_remainingTime = 0;

    const AxisParams* m_params = nullptr;  // Borrowed pointer, not owned.

    // ---- Internal helpers ----
    float estimateMoveTime(float distance, float speed, float acc, float dec);
    void  updateRemainingTime();
    void  resumeCurrentMove();
};
