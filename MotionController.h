#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_MotionController.h"
#include "zmotion.h"
#include "zauxdll2.h"
#include <QTimer>

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

    // Normal direction slots for each axis
    void on_XNormalDirBtn_clicked();
    void on_YNormalDirBtn_clicked();
    void on_ZQNormalDirBtn_clicked();
    void on_ZHNormalDirBtn_clicked();

    // Reverse direction slots for each axis
    void on_XReverseDirBtn_clicked();
    void on_YReverseDirBtn_clicked();
    void on_ZQReverseDirBtn_clicked();
    void on_ZHReverseDirBtn_clicked();

    // Stop axis slots for each axis
    void on_XStopBtn_clicked();
    void on_YStopBtn_clicked();
    void on_ZQStopBtn_clicked();
    void on_ZHStopBtn_clicked();


private:
    Ui::MotionControllerClass ui;
    ZMC_HANDLE handle = nullptr;
    QTimer* updateTimer = nullptr;
    AxisConfig m_axisList[4];

private:
    void parameterSettings(const int axis);
    void clearParameters(const int axis);
    void setNormalDirection(const int axis);
    void setReverseDirection(const int axis);
    void stopAxis(const int axis);

private:
    void updateSingleAxisParameters(const AxisConfig& config, bool isConnected);
    void updateAllAxisParameters();

};





