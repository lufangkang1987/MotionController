#include "MotionController.h"
#include <QMessageBox>
#include "ParaSettings.h"

MotionController::MotionController(QWidget* parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);
    updateTimer = new QTimer(this);

    m_axisList[0] = { Axis::X_AXIS, ui.XStateLineEdit, ui.XLocateLineEdit, ui.XSpeedLineEdit };
    m_axisList[1] = { Axis::Y_MASTER, ui.YStateLineEdit, ui.YLocateLineEdit, ui.YSpeedLineEdit };
    m_axisList[2] = { Axis::Z_FRONT_AXIS, ui.ZQStateLineEdit, ui.ZQLocateLineEdit, ui.ZQSpeedLineEdit };
    m_axisList[3] = { Axis::Z_BACK_AXIS, ui.ZHStateLineEdit, ui.ZHLocateLineEdit, ui.ZHSpeedLineEdit };

    updateTimer->setInterval(100); // Update every 100 ms
    connect(updateTimer, &QTimer::timeout, this, &MotionController::updateAllAxisParameters);
    updateTimer->start();
}

MotionController::~MotionController()
{
    ZAux_Close(handle);
    handle = nullptr;
}

void MotionController::on_DisconnectBtn_clicked()
{
    ZAux_Close(handle);
    handle = nullptr;
    ui.ConnectBtn->setEnabled(true);
    ui.DisconnectBtn->setEnabled(false);
}

void MotionController::on_OriginBtn_clicked()
{
    // Start the origin process for all axes

}

void MotionController::on_StartInspectBtn_clicked()
{
    // Start the inspection process
}

void MotionController::on_StartPointBtn_clicked()
{
    // Set the start point
}

void MotionController::on_BackStartPointBtn_clicked()
{ 
    // Back to start point
}

void MotionController::on_EndBtn_clicked()
{
    // Set the end point
}


void MotionController::on_BackEndBtn_clicked()
{ 
    //Back to the end point
}

void MotionController::on_StepDoubleSpinBox_valueChanged(double arg1)
{
    ui.XdoubleSpinBox->setValue(arg1);
    ui.YdoubleSpinBox->setValue(arg1);
    ui.ZQdoubleSpinBox->setValue(arg1);
    ui.ZHdoubleSpinBox->setValue(arg1);

}

void MotionController::on_XParaBtn_clicked()
{
    parameterSettings(Axis::X_AXIS);
}

void MotionController::on_YParaBtn_clicked()
{
    parameterSettings(Axis::Y_MASTER);
}

void MotionController::on_ZQParaBtn_clicked()
{
    parameterSettings(Axis::Z_FRONT_AXIS);
}

void MotionController::on_ZHParaBtn_clicked()
{
    parameterSettings(Axis::Z_BACK_AXIS);
}

void MotionController::on_XClearBtn_clicked()
{
    clearParameters(Axis::X_AXIS);
}

void MotionController::on_YClearBtn_clicked()
{
    clearParameters(Axis::Y_MASTER);
}

void MotionController::on_ZQClearBtn_clicked()
{
    clearParameters(Axis::Z_FRONT_AXIS);
}

void MotionController::on_ZHClearBtn_clicked()
{
    clearParameters(Axis::Z_BACK_AXIS);
}

void MotionController::on_XNormalDirBtn_clicked()
{
    setNormalDirection(Axis::X_AXIS);
}

void MotionController::on_YNormalDirBtn_clicked()
{
    setNormalDirection(Axis::Y_MASTER);
}

void MotionController::on_ZQNormalDirBtn_clicked()
{
    setNormalDirection(Axis::Z_FRONT_AXIS);
}

void MotionController::on_ZHNormalDirBtn_clicked()
{
    setNormalDirection(Axis::Z_BACK_AXIS);
}

void MotionController::on_XReverseDirBtn_clicked()
{
    setReverseDirection(Axis::X_AXIS);
}

void MotionController::on_YReverseDirBtn_clicked()
{
    setReverseDirection(Axis::Y_MASTER);
}

void MotionController::on_ZQReverseDirBtn_clicked()
{
    setReverseDirection(Axis::Z_FRONT_AXIS);
}

void MotionController::on_ZHReverseDirBtn_clicked()
{
    setReverseDirection(Axis::Z_BACK_AXIS);
}

void MotionController::on_XStopBtn_clicked()
{
    stopAxis(Axis::X_AXIS);
}

void MotionController::on_YStopBtn_clicked()
{
    stopAxis(Axis::Y_MASTER);
}

void MotionController::on_ZQStopBtn_clicked()
{
    stopAxis(Axis::Z_FRONT_AXIS);
}

void MotionController::on_ZHStopBtn_clicked()
{
    stopAxis(Axis::Z_BACK_AXIS);
}

void MotionController::parameterSettings(const int axis)
{
    QString axisName;
    switch (axis)
    {
    case Axis::X_AXIS:
        // Open X axis parameter settings dialog
        axisName = "X Axis";
        break;
    case Axis::Y_MASTER:
        // Open Y axis parameter settings dialog
        axisName = "Y Axis";
        break;
    case Axis::Z_FRONT_AXIS:
        // Open Z Front axis parameter settings dialog
        axisName = "Z Front Axis";
        break;
    case Axis::Z_BACK_AXIS:
        // Open ZBack axis parameter settings dialog
        axisName = "Z Back Axis";
        break;
    default:
        break;
    }
    ParaSettings* paraSettings = new ParaSettings(axisName, nullptr);
    paraSettings->setAttribute(Qt::WA_DeleteOnClose); // Ensure the dialog is deleted when closed)
    paraSettings->show();
}

void MotionController::clearParameters(const int axis)
{
}

void MotionController::setNormalDirection(const int axis)
{}

void MotionController::setReverseDirection(const int axis)
{}

void MotionController::stopAxis(const int axis)
{}

void MotionController::updateSingleAxisParameters(const AxisConfig& config,bool isConnected)
{
    if (!isConnected) {
        config.stateLineEdit->setText("未连接");
        config.locateLineEdit->setText("0.00");
        config.speedLineEdit->setText("0.00");
        return;
    }
    float speed = 0.0f, position = 0.0f;
    ZAux_Direct_GetDpos(handle, config.axisNo, &position);
    config.locateLineEdit->setText(QString::number(position, 'f', 2));

    ZAux_Direct_GetVpSpeed(handle, config.axisNo, &speed);
    config.speedLineEdit->setText(QString::number(speed, 'f', 2));

    int idle = 0;
    int ret = ZAux_Direct_GetIfIdle(handle, config.axisNo, &idle);
    if (ret == ERR_OK)
    {
        if (idle == -1)
        {
            config.stateLineEdit->setText("停止");
        }
        else
        {
            config.stateLineEdit->setText("运行");
        }
    }
}

void MotionController::updateAllAxisParameters()
{
    bool isConnected = (handle != nullptr);
    for (int i = 0; i < 4; ++i)
    {
        updateSingleAxisParameters(m_axisList[i], isConnected);
    }
}

void MotionController::on_ConnectBtn_clicked()
{
    // Connect to the motion controller
    QString ipAddress = ui.IPComboBox->currentText();

    int ret = ZAux_OpenEth(ipAddress.toUtf8().data(), &handle);
    if (ret == ERR_OK) {
        // Connection successful
        ui.ConnectBtn->setEnabled(false);
        ui.DisconnectBtn->setEnabled(true);
    }
    else {
        // Connection failed
        QMessageBox::critical(this, "Connection Error", "Failed to connect to the motion controller.");
    }
}

