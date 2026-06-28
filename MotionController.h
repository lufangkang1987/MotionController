#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_MotionController.h"
#include "zmotion.h"
#include "zauxdll2.h"
#include <QTimer>
#include "ParaSettings.h"

// Enum to represent different axis
enum Axis
{
    X_AXIS = 0,
    Y_MASTER = 1,
    Y_SLAVE = 2,
    Z_FRONT_AXIS = 3,
    Z_BACK_AXIS = 4
};

// Structure to hold axis configuration
struct AxisConfig
{
    int axisNo;
    QLineEdit* stateLineEdit;
    QLineEdit* locateLineEdit;
    QLineEdit* speedLineEdit;
};

// 扫描状态机
enum ScanState
{
    SCAN_IDLE,           // 空闲
    SCAN_MOVE_TO_START,  // 移动到起点（栅格扫查）
    SCAN_FORWARD,        // X正向扫查中（栅格扫查）
    SCAN_REVERSE,        // X反向扫查中（栅格扫查）
    SCAN_STEP,           // Y步进中（栅格扫查）
    SCAN_CONTINUOUS,     // 连续扫查中（X/Y联动斜线）
    SCAN_DONE            // 扫描完成
};

class MotionController : public QMainWindow
{
    Q_OBJECT

public:
    MotionController(QWidget *parent = nullptr);
    ~MotionController();

private slots:
    void on_ConnectBtn_clicked();
    void on_DisconnectBtn_clicked();

    void on_OriginBtn_clicked();
    void on_StartInspectBtn_clicked();

    void on_StartPointBtn_clicked();
    void on_BackStartPointBtn_clicked();
    void on_EndBtn_clicked();
    void on_BackEndBtn_clicked();

    void on_StepDoubleSpinBox_valueChanged(double arg1);

    // Parameter settings slots for each axis
    void on_XParaBtn_clicked();
    void on_YParaBtn_clicked();
    void on_ZQParaBtn_clicked();
    void on_ZHParaBtn_clicked();

    // Clear parameters slots for each axis
    void on_XClearBtn_clicked();
    void on_YClearBtn_clicked();
    void on_ZQClearBtn_clicked();
    void on_ZHClearBtn_clicked();

    // Normal direction slots for each axis (按钮名必须与UI一致)
    void on_XNormalBtn_clicked();
    void on_YNormalBtn_clicked();
    void on_ZQNormalBtn_clicked();
    void on_ZHNormalBtn_clicked();

    // Reverse direction slots for each axis
    void on_XReverseBtn_clicked();
    void on_YReverseBtn_clicked();
    void on_ZQReverseBtn_clicked();
    void on_ZHReverseBtn_clicked();

    // Stop axis slots for each axis
    void on_XStopBtn_clicked();
    void on_YStopBtn_clicked();
    void on_ZQStopBtn_clicked();
    void on_ZHStopBtn_clicked();

    // Apply axis parameters
    void applyAxisParameters(int axis, const AxisParams& params);


private:
    Ui::MotionControllerClass ui;
    ZMC_HANDLE handle = nullptr;
    QTimer* updateTimer = nullptr;
    AxisConfig m_axisList[4];
    AxisParams m_axisParams[5]; // 缓存各轴参数（以轴号为索引），用于状态显示换算
    float m_startPoint[4] = {0, 0, 0, 0};  // 扫查起点位置 [X, Y, Z前, Z后]
    float m_endPoint[4]   = {0, 0, 0, 0};  // 扫查终点位置 [X, Y, Z前, Z后]

    // 回零状态
    bool  m_isHoming = false;    // 是否正在回零

    // 扫描状态机
    ScanState m_scanState = SCAN_IDLE;
    float m_scanStartX = 0;      // 扫查起点 X
    float m_scanEndX   = 0;      // 扫查终点 X
    float m_scanStartY = 0;      // 步进起点 Y
    float m_scanEndY   = 0;      // 步进终点 Y
    float m_stepGap    = 0;      // 步进间隔（Y每步移动量）
    int   m_totalLines = 0;      // 总扫查线数
    int   m_currentLine = 0;     // 当前线号
    bool  m_forward = true;      // 扫查方向 true=正向

private:
    void parameterSettings(const int axis);
    void clearParameters(const int axis);
    void setNormalDirection(const int axis);
    void setReverseDirection(const int axis);
    void stopAxis(const int axis);

    void loadConfig();
    void saveConfig();
    void applyAllAxisParams();

    void startScan();
    void stopScan();
    void scanStateMachine();
    float estimateMoveTime(float distance, float speed, float acc, float dec);
    float estimateRemainingTime();
    QString formatRemainingTime(float seconds);

private:
    void updateSingleAxisParameters(const AxisConfig& config, bool isConnected);
    void updateAllAxisParameters();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    bool handleMotionKey(QKeyEvent* event);
};





