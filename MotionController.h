#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_MotionController.h"
#include "MotionController/Types.h"           // Axis, ScanState, AxisConfig
#include "ParaSettings.h"    // AxisParams

class ZmcAdapter;
class AxisService;
class HomeService;
class ScanService;
class QTimer;

// ============================================================================
// MotionController - UI layer for signal wiring, timers, and config persistence.
// Motion-control logic is delegated to AxisService, HomeService, and ScanService.
// Hardware communication is delegated to ZmcAdapter.
// ============================================================================

class MotionController : public QMainWindow
{
    Q_OBJECT

public:
    MotionController(QWidget* parent = nullptr);
    ~MotionController();

private slots:
    // ---- Connection ----
    void on_ConnectBtn_clicked();
    void on_DisconnectBtn_clicked();

    // ---- Inspection operations ----
    void on_OriginBtn_clicked();
    void on_StartInspectBtn_clicked();
    void on_StopInspectBtn_clicked();

    // ---- Scan plan ----
    void on_StartPointBtn_clicked();
    void on_BackStartPointBtn_clicked();
    void on_EndBtn_clicked();
    void on_BackEndBtn_clicked();
    void on_GainRegionRadioBtn_clicked();
    void on_StepDoubleSpinBox_valueChanged(double arg1);

    // ---- Four-axis parameter buttons ----
    void on_XParaBtn_clicked();
    void on_YParaBtn_clicked();
    void on_ZQParaBtn_clicked();
    void on_ZHParaBtn_clicked();

    // ---- Four-axis position clear ----
    void on_XClearBtn_clicked();
    void on_YClearBtn_clicked();
    void on_ZQClearBtn_clicked();
    void on_ZHClearBtn_clicked();

    // ---- Four-axis positive motion ----
    void on_XNormalBtn_clicked();
    void on_YNormalBtn_clicked();
    void on_ZQNormalBtn_clicked();
    void on_ZHNormalBtn_clicked();

    // ---- Four-axis negative motion ----
    void on_XReverseBtn_clicked();
    void on_YReverseBtn_clicked();
    void on_ZQReverseBtn_clicked();
    void on_ZHReverseBtn_clicked();

    // ---- Four-axis stop ----
    void on_XStopBtn_clicked();
    void on_YStopBtn_clicked();
    void on_ZQStopBtn_clicked();
    void on_ZHStopBtn_clicked();

    // ---- Parameter change callback ----
    void applyAxisParameters(int axis, const AxisParams& params);

    // ---- HomeService callbacks ----
    void onHomeStarted();
    void onHomeStopped();
    void onHomeCompleted();
    void onHomeFailed(const QString& reason);

    // ---- ScanService callbacks ----
    void onScanStarted();
    void onScanPaused();
    void onScanResumed();
    void onScanStopped();
    void onScanCompleted();

private:
    Ui::MotionControllerClass ui;

    // ---- Adapter and services, injected through constructors. ----
    ZmcAdapter*  m_adapter     = nullptr;
    AxisService* m_axisService = nullptr;
    HomeService* m_homeService = nullptr;
    ScanService* m_scanService = nullptr;

    QTimer* updateTimer = nullptr;
    AxisConfig m_axisList[4];
    AxisParams m_axisParams[5];   // Cached parameters for each axis.

    float m_startPoint[4] = { 0, 0, 0, 0 };
    float m_endPoint[4]   = { 0, 0, 0, 0 };

    // ---- Cached LED state. Avoids writing the same OP values every 100ms. ----
    // LED output ports are loaded from config.ini [Led].
    enum LedState { LED_OFF, LED_ALARM, LED_HOME, LED_NORMAL };
    LedState m_lastLedState = LED_OFF;
    LedState m_scanLedOverride = LED_OFF;
    bool m_scanResetPending = false;
    int m_alarmLedOp = 0;
    int m_homeLedOp = 1;
    int m_normalLedOp = 2;
    int m_buzzerOp = 3;
    int m_buzzerDurationMs = 1000;

    // ---- Config persistence ----
    void ensureConfigFileExists();
    void loadConfig();
    void saveConfig();

    // ---- Timer-driven updates ----
    void updateAllAxisParameters();

    // ---- Scan plan helpers ----
    void updateScanRegionLength();

    // ---- Keyboard ----
    bool handleMotionKey(QKeyEvent* event);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
};
