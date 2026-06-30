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
// MotionController — UI 层（纯信号槽 + 定时器 + 配置持久化）
// 运动控制逻辑委托给 AxisService / HomeService / ScanService
// 硬件通信委托给 ZmcAdapter
// ============================================================================

class MotionController : public QMainWindow
{
    Q_OBJECT

public:
    MotionController(QWidget* parent = nullptr);
    ~MotionController();

private slots:
    // ---- 连接 ----
    void on_ConnectBtn_clicked();
    void on_DisconnectBtn_clicked();

    // ---- 检测操作 ----
    void on_OriginBtn_clicked();
    void on_StartInspectBtn_clicked();

    // ---- 扫查计划 ----
    void on_StartPointBtn_clicked();
    void on_BackStartPointBtn_clicked();
    void on_EndBtn_clicked();
    void on_BackEndBtn_clicked();
    void on_GainRegionRadioBtn_clicked();
    void on_StepDoubleSpinBox_valueChanged(double arg1);

    // ---- 四轴参数按钮 ----
    void on_XParaBtn_clicked();
    void on_YParaBtn_clicked();
    void on_ZQParaBtn_clicked();
    void on_ZHParaBtn_clicked();

    // ---- 四轴清零 ----
    void on_XClearBtn_clicked();
    void on_YClearBtn_clicked();
    void on_ZQClearBtn_clicked();
    void on_ZHClearBtn_clicked();

    // ---- 四轴正转 ----
    void on_XNormalBtn_clicked();
    void on_YNormalBtn_clicked();
    void on_ZQNormalBtn_clicked();
    void on_ZHNormalBtn_clicked();

    // ---- 四轴反转 ----
    void on_XReverseBtn_clicked();
    void on_YReverseBtn_clicked();
    void on_ZQReverseBtn_clicked();
    void on_ZHReverseBtn_clicked();

    // ---- 四轴停止 ----
    void on_XStopBtn_clicked();
    void on_YStopBtn_clicked();
    void on_ZQStopBtn_clicked();
    void on_ZHStopBtn_clicked();

    // ---- 参数变更回调 ----
    void applyAxisParameters(int axis, const AxisParams& params);

    // ---- HomeService 回调 ----
    void onHomeStarted();
    void onHomeStopped();
    void onHomeCompleted();
    void onHomeFailed(const QString& reason);

    // ---- ScanService 回调 ----
    void onScanStarted();
    void onScanStopped();
    void onScanCompleted();

private:
    Ui::MotionControllerClass ui;

    // ---- 适配器 & 服务（构造函数注入） ----
    ZmcAdapter*  m_adapter     = nullptr;
    AxisService* m_axisService = nullptr;
    HomeService* m_homeService = nullptr;
    ScanService* m_scanService = nullptr;

    QTimer* updateTimer = nullptr;
    AxisConfig m_axisList[4];
    AxisParams m_axisParams[5];   // 缓存各轴参数

    float m_startPoint[4] = { 0, 0, 0, 0 };
    float m_endPoint[4]   = { 0, 0, 0, 0 };

    // ---- 配置持久化 ----
    void loadConfig();
    void saveConfig();

    // ---- 定时器驱动 ----
    void updateAllAxisParameters();

    // ---- 扫查计划辅助 ----
    void updateScanRegionLength();

    // ---- 键盘 ----
    bool handleMotionKey(QKeyEvent* event);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
};
