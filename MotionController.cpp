#include "MotionController.h"
#include <QMessageBox>
#include <QSettings>
#include <QCoreApplication>
#include <QApplication>
#include <QEvent>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QKeyEvent>
#include <cmath>
#include "ParaSettings.h"

MotionController::MotionController(QWidget* parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);
    qApp->installEventFilter(this);

    updateTimer = new QTimer(this);

    m_axisList[0] = { Axis::X_AXIS, ui.XStateLineEdit, ui.XLocateLineEdit, ui.XSpeedLineEdit };
    m_axisList[1] = { Axis::Y_MASTER, ui.YStateLineEdit, ui.YLocateLineEdit, ui.YSpeedLineEdit };
    m_axisList[2] = { Axis::Z_FRONT_AXIS, ui.ZQStateLineEdit, ui.ZQLocateLineEdit, ui.ZQSpeedLineEdit };
    m_axisList[3] = { Axis::Z_BACK_AXIS, ui.ZHStateLineEdit, ui.ZHLocateLineEdit, ui.ZHSpeedLineEdit };

    updateTimer->setInterval(100); // Update every 100 ms
    connect(updateTimer, &QTimer::timeout, this, &MotionController::updateAllAxisParameters);
    updateTimer->start();

    loadConfig(); // 启动时加载配置
}

MotionController::~MotionController()
{
    qApp->removeEventFilter(this);
    saveConfig(); // 退出时保存配置
    ZAux_Close(handle);
    handle = nullptr;
}

void MotionController::on_DisconnectBtn_clicked()
{
    // 先停回零和扫描
    m_isHoming = false;
    m_scanState = SCAN_IDLE;

    ZAux_Close(handle);
    handle = nullptr;
    ui.ConnectBtn->setEnabled(true);
    ui.DisconnectBtn->setEnabled(false);
    ui.IPComboBox->setEnabled(true);
    ui.OriginBtn->setText("原点回零");
    ui.StartInspectBtn->setText("开始检测");

    // timer 继续跑，updateAllAxisParameters 里会检测到 handle==nullptr 显示"未连接"
}

void MotionController::on_OriginBtn_clicked()
{
    if (!handle)
    {
        QMessageBox::warning(this, "提示", "请先连接控制器");
        return;
    }

    // 正在回零中 → 点击 = 停止回零
    if (m_isHoming)
    {
        int axes[] = {Axis::X_AXIS, Axis::Y_MASTER, Axis::Z_FRONT_AXIS, Axis::Z_BACK_AXIS};
        for (int axis : axes)
            ZAux_Direct_Single_Cancel(handle, axis, 2);
        m_isHoming = false;
        ui.OriginBtn->setText("原点回零");
        return;
    }

    // 检查各轴是否空闲
    int axes[] = {Axis::X_AXIS, Axis::Y_MASTER, Axis::Z_FRONT_AXIS, Axis::Z_BACK_AXIS};
    for (int axis : axes)
    {
        int idle = 0;
        ZAux_Direct_GetIfIdle(handle, axis, &idle);
        if (idle != -1)  // 0=运动中, -1=停止
        {
            QMessageBox::warning(this, "提示",
                QString("轴 %1 正在运动中，请先停止").arg(axis));
            return;
        }
    }

    // ---------- 从配置文件读取回零参数 ----------
    QSettings settings(configFilePath(), QSettings::IniFormat);
    // 回零模式：1=正向回零 2=反向回零 3=正向回零+反向退出 4=反向回零+正向退出
    int datumMode = settings.value("Home/Mode", 3).toInt();

    // ---------- 发起全部轴回零 ----------
    m_isHoming = true;
    ui.OriginBtn->setText("停止回零");

    for (int axis : axes)
    {
        // 脉冲当量、速度、加减速度（复用轴参数）
        ZAux_Direct_SetUnits(handle, axis,   m_axisParams[axis].m_units);
        ZAux_Direct_SetLspeed(handle, axis,  m_axisParams[axis].m_lspeed);
        ZAux_Direct_SetSpeed(handle, axis,   m_axisParams[axis].m_speed);
        ZAux_Direct_SetAccel(handle, axis,   m_axisParams[axis].m_acc);
        ZAux_Direct_SetDecel(handle, axis,   m_axisParams[axis].m_dec);
        ZAux_Direct_SetCreep(handle, axis, 10.0f);

        // 原点/限位输入口 — 从配置文件读取，默认 -1 表示不启用
        // 在 config.ini 里填实际接线编号，例如：
        //   [Home]
        //   DatumIn0=0   ← X轴原点传感器接 IN0
        //   FwdIn0=4     ← X轴正限位接 IN4
        //   RevIn0=8     ← X轴负限位接 IN8
        int datumIn = settings.value(QString("Home/DatumIn%1").arg(axis), -1).toInt();
        int fwdIn   = settings.value(QString("Home/FwdIn%1").arg(axis),   -1).toInt();
        int revIn   = settings.value(QString("Home/RevIn%1").arg(axis),   -1).toInt();

        if (datumIn >= 0) ZAux_Direct_SetDatumIn(handle, axis, datumIn);
        if (fwdIn   >= 0) ZAux_Direct_SetFwdIn(handle, axis, fwdIn);
        if (revIn   >= 0) ZAux_Direct_SetRevIn(handle, axis, revIn);

        ZAux_Direct_Single_Datum(handle, axis, datumMode);
    }
}

void MotionController::on_StartInspectBtn_clicked()
{
    if (!handle)
    {
        QMessageBox::warning(this, "提示", "请先连接控制器");
        return;
    }

    if (m_scanState != SCAN_IDLE)
    {
        // 扫描中点击 = 停止扫描
        stopScan();
        return;
    }

    startScan();
}

void MotionController::on_StartPointBtn_clicked()
{
    if (!handle)
    {
        QMessageBox::warning(this, "提示", "请先连接控制器");
        return;
    }
    int axes[4] = {Axis::X_AXIS, Axis::Y_MASTER, Axis::Z_FRONT_AXIS, Axis::Z_BACK_AXIS};
    const char* labels[4] = {"X", "Y", "ZQ", "ZH"};
    QString text;
    for (int i = 0; i < 4; i++)
    {
        ZAux_Direct_GetDpos(handle, axes[i], &m_startPoint[i]);
        if (i > 0) text += " ";
        text += QString("%1:%2").arg(labels[i]).arg(m_startPoint[i], 0, 'f', 2);
    }
    ui.lineEdit_4->setText(text);
}

void MotionController::on_BackStartPointBtn_clicked()
{ 
    if (!handle)
    {
        QMessageBox::warning(this, "提示", "请先连接控制器");
        return;
    }
    int axes[4] = {Axis::X_AXIS, Axis::Y_MASTER, Axis::Z_FRONT_AXIS, Axis::Z_BACK_AXIS};
    for (int i = 0; i < 4; i++)
    {
        ZAux_Direct_Single_MoveAbs(handle, axes[i], m_startPoint[i]);
    }
}

void MotionController::on_EndBtn_clicked()
{
    if (!handle)
    {
        QMessageBox::warning(this, "提示", "请先连接控制器");
        return;
    }
    int axes[4] = {Axis::X_AXIS, Axis::Y_MASTER, Axis::Z_FRONT_AXIS, Axis::Z_BACK_AXIS};
    const char* labels[4] = {"X", "Y", "ZQ", "ZH"};
    QString text;
    for (int i = 0; i < 4; i++)
    {
        ZAux_Direct_GetDpos(handle, axes[i], &m_endPoint[i]);
        if (i > 0) text += " ";
        text += QString("%1:%2").arg(labels[i]).arg(m_endPoint[i], 0, 'f', 2);
    }
    ui.lineEdit_5->setText(text);
}


void MotionController::on_BackEndBtn_clicked()
{ 
    if (!handle)
    {
        QMessageBox::warning(this, "提示", "请先连接控制器");
        return;
    }
    int axes[4] = {Axis::X_AXIS, Axis::Y_MASTER, Axis::Z_FRONT_AXIS, Axis::Z_BACK_AXIS};
    for (int i = 0; i < 4; i++)
    {
        ZAux_Direct_Single_MoveAbs(handle, axes[i], m_endPoint[i]);
    }
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

void MotionController::on_XNormalBtn_clicked()
{
    setNormalDirection(Axis::X_AXIS);
}

void MotionController::on_YNormalBtn_clicked()
{
    setNormalDirection(Axis::Y_MASTER);
}

void MotionController::on_ZQNormalBtn_clicked()
{
    setNormalDirection(Axis::Z_FRONT_AXIS);
}

void MotionController::on_ZHNormalBtn_clicked()
{
    setNormalDirection(Axis::Z_BACK_AXIS);
}

void MotionController::on_XReverseBtn_clicked()
{
    setReverseDirection(Axis::X_AXIS);
}

void MotionController::on_YReverseBtn_clicked()
{
    setReverseDirection(Axis::Y_MASTER);
}

void MotionController::on_ZQReverseBtn_clicked()
{
    setReverseDirection(Axis::Z_FRONT_AXIS);
}

void MotionController::on_ZHReverseBtn_clicked()
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

void MotionController::applyAxisParameters(int axis, const AxisParams& params)
{
    // 更新参数缓存（无论是否连接都需要缓存，供状态显示使用）
    m_axisParams[axis] = params;

    if (!handle) return;
    ZAux_Direct_SetUnits(handle, axis, params.m_units);
    ZAux_Direct_SetLspeed(handle, axis, params.m_lspeed);
    ZAux_Direct_SetSpeed(handle, axis, params.m_speed);
    ZAux_Direct_SetAccel(handle, axis, params.m_acc);
    ZAux_Direct_SetDecel(handle, axis, params.m_dec);
    ZAux_Direct_SetSramp(handle, axis, params.m_sramp);

    ZAux_Direct_SetInvertStep(handle, axis, params.dir);
}

void MotionController::applyAllAxisParams()
{
    // 从配置文件读取所有轴参数，下发到控制器
    int axes[] = { Axis::X_AXIS, Axis::Y_MASTER, Axis::Z_FRONT_AXIS, Axis::Z_BACK_AXIS };
    QSettings settings(configFilePath(), QSettings::IniFormat);

    for (int axis : axes)
    {
        QString group = QString("Axis%1").arg(axis);
        settings.beginGroup(group);
        AxisParams params;
        params.m_units  = settings.value("units",  1.0f).toFloat();
        params.m_lspeed = settings.value("lspeed", 0.0f).toFloat();
        params.m_speed  = settings.value("speed",  100.0f).toFloat();
        params.m_acc    = settings.value("acc",    3000.0f).toFloat();
        params.m_dec    = settings.value("dec",    3000.0f).toFloat();
        params.m_sramp  = settings.value("sramp",  10.0f).toFloat();
        params.dir      = settings.value("dir",    0).toInt();
        settings.endGroup();

        applyAxisParameters(axis, params);
    }
}

void MotionController::loadConfig()
{
    QSettings settings(configFilePath(), QSettings::IniFormat);

    // 加载上次连接的 IP
    QString lastIP = settings.value("Connection/IP", "192.168.0.11").toString();
    ui.IPComboBox->setCurrentText(lastIP);

    // 加载定长值
    double step = settings.value("Scan/Step", 10.0).toDouble();
    ui.StepDoubleSpinBox->setValue(step);

    // 预加载各轴参数到缓存（未连接时只更新缓存，不下发控制器）
    applyAllAxisParams();
}

void MotionController::saveConfig()
{
    QSettings settings(configFilePath(), QSettings::IniFormat);

    // 保存当前 IP
    settings.setValue("Connection/IP", ui.IPComboBox->currentText());

    // 保存定长值
    settings.setValue("Scan/Step", ui.StepDoubleSpinBox->value());
}

void MotionController::parameterSettings(const int axis)
{
    if (!handle)
    {
        QMessageBox::warning(this, "提示", "请先连接控制器");
        return;
    }
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
    ParaSettings* paraSettings = new ParaSettings(axis, axisName, nullptr);
    paraSettings->setAttribute(Qt::WA_DeleteOnClose); // Ensure the dialog is deleted when closed)
    connect(paraSettings, &ParaSettings::parametersChanged, this, &MotionController::applyAxisParameters);
    paraSettings->show();
}

void MotionController::clearParameters(const int axis)
{
    if (!handle)
    {
        QMessageBox::warning(this, "提示", "请先连接控制器");
        return;
    }
    ZAux_Direct_SetDpos(handle, axis, 0);
}

void MotionController::setNormalDirection(const int axis)
{
    if (!handle)
    {
        QMessageBox::warning(this, "提示", "请先连接控制器");
        return;
    }

    // 根据轴号找对应的"连续"复选框和"定长"输入框
    QCheckBox* continueCb = nullptr;
    QDoubleSpinBox* stepSpin = nullptr;
    switch (axis)
    {
    case Axis::X_AXIS:        continueCb = ui.checkBox_6;  stepSpin = ui.XdoubleSpinBox;  break;
    case Axis::Y_MASTER:      continueCb = ui.checkBox_7;  stepSpin = ui.YdoubleSpinBox;  break;
    case Axis::Z_FRONT_AXIS:  continueCb = ui.checkBox_8;  stepSpin = ui.ZQdoubleSpinBox; break;
    case Axis::Z_BACK_AXIS:   continueCb = ui.checkBox_10; stepSpin = ui.ZHdoubleSpinBox; break;
    default: return;
    }

    if (continueCb && continueCb->isChecked())
    {
        // 勾选连续 → 连续点动（正方向）
        ZAux_Direct_Single_Vmove(handle, axis, 1);
    }
    else if (stepSpin)
    {
        // 未勾选 → 寸动（走定长距离）
        float step = stepSpin->value();
        ZAux_Direct_Single_Move(handle, axis, step);
    }
}

void MotionController::setReverseDirection(const int axis)
{
    if (!handle)
    {
        QMessageBox::warning(this, "提示", "请先连接控制器");
        return;
    }

    QCheckBox* continueCb = nullptr;
    QDoubleSpinBox* stepSpin = nullptr;
    switch (axis)
    {
    case Axis::X_AXIS:        continueCb = ui.checkBox_6;  stepSpin = ui.XdoubleSpinBox;  break;
    case Axis::Y_MASTER:      continueCb = ui.checkBox_7;  stepSpin = ui.YdoubleSpinBox;  break;
    case Axis::Z_FRONT_AXIS:  continueCb = ui.checkBox_8;  stepSpin = ui.ZQdoubleSpinBox; break;
    case Axis::Z_BACK_AXIS:   continueCb = ui.checkBox_10; stepSpin = ui.ZHdoubleSpinBox; break;
    default: return;
    }

    if (continueCb && continueCb->isChecked())
    {
        // 勾选连续 → 连续点动（负方向，0=反转）
        ZAux_Direct_Single_Vmove(handle, axis, 0);
    }
    else if (stepSpin)
    {
        // 未勾选 → 寸动（反方向走定长距离）
        float step = stepSpin->value();
        ZAux_Direct_Single_Move(handle, axis, -step);
    }
}

void MotionController::stopAxis(const int axis)
{
    if (!handle)
    {
        QMessageBox::warning(this, "提示", "请先连接控制器");
        return;
    }
    ZAux_Direct_Single_Cancel(handle, axis, 2);
}

void MotionController::updateSingleAxisParameters(const AxisConfig& config, bool isConnected)
{
    if (!isConnected) {
        config.stateLineEdit->setText("未连接");
        config.locateLineEdit->setText("0.00");
        config.speedLineEdit->setText("0.00");
        return;
    }

    int axisNo = config.axisNo;
    const AxisParams& params = m_axisParams[axisNo];

    float speed = 0.0f, position = 0.0f;
    ZAux_Direct_GetDpos(handle, axisNo, &position);
    ZAux_Direct_GetVpSpeed(handle, axisNo, &speed);

    // 根据方向参数调整显示：dir=1（反向）时位置和速度取反
    if (params.dir == 1)
    {
        position = -position;
        speed = -speed;
    }

    config.locateLineEdit->setText(QString::number(position, 'f', 2));
    config.speedLineEdit->setText(QString::number(speed, 'f', 2));

    // 状态判断：先查 idle，再根据速度方向区分正转/反转
    // GetIfIdle: 输出 idle=0 运动中, idle=-1 停止
    int idle = 0;
    int ret = ZAux_Direct_GetIfIdle(handle, axisNo, &idle);
    if (ret == ERR_OK)
    {
        if (idle == -1)
        {
            config.stateLineEdit->setText("停止");
        }
        else if (idle == 0)
        {
            // 运动中，根据速度方向判断
            if (speed > 0.001f)
                config.stateLineEdit->setText("正转");
            else if (speed < -0.001f)
                config.stateLineEdit->setText("反转");
            else
                config.stateLineEdit->setText("运行");
        }
        else
        {
            config.stateLineEdit->setText("未知");
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

    // ---------- 回零状态检查（每100ms）----------
    // GetIfIdle: 输出 idle=0 运动中, idle=-1 停止
    if (m_isHoming && handle)
    {
        int axes[] = {Axis::X_AXIS, Axis::Y_MASTER, Axis::Z_FRONT_AXIS, Axis::Z_BACK_AXIS};
        bool allIdle = true;
        for (int axis : axes)
        {
            int idle = 0;
            ZAux_Direct_GetIfIdle(handle, axis, &idle);
            if (idle != -1)  // 还在运动中
            {
                allIdle = false;
                break;
            }
        }

        if (allIdle)
        {
            // 所有轴回零完成
            m_isHoming = false;
            ui.OriginBtn->setText("原点回零");

            // 回零完成后 DPOS + MPOS 清零
            for (int axis : axes)
            {
                ZAux_Direct_SetDpos(handle, axis, 0);
                ZAux_Direct_SetMpos(handle, axis, 0);
            }

            QMessageBox::information(this, "提示", "所有轴回零完成，原点已置零");
        }
    }

    // 扫描状态机驱动（每100ms调用一次）
    if (m_scanState != SCAN_IDLE && m_scanState != SCAN_DONE)
    {
        scanStateMachine();
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
        ui.IPComboBox->setEnabled(false);

        // 保存 IP 到配置
        QSettings settings(configFilePath(), QSettings::IniFormat);
        settings.setValue("Connection/IP", ipAddress);

        // 连接成功后，把保存的轴参数全部下发到控制器
        applyAllAxisParams();
    }
    else {
        // Connection failed
        QMessageBox::critical(this, "Connection Error", "Failed to connect to the motion controller.");
    }
}

void MotionController::keyPressEvent(QKeyEvent* event)
{
    if (handleMotionKey(event))
        return;

    QMainWindow::keyPressEvent(event);
}

bool MotionController::eventFilter(QObject* watched, QEvent* event)
{
    bool isOwnObject = false;
    for (QObject* object = watched; object; object = object->parent())
    {
        if (object == this)
        {
            isOwnObject = true;
            break;
        }
    }

    if (isOwnObject && event->type() == QEvent::KeyPress)
    {
        if (handleMotionKey(static_cast<QKeyEvent*>(event)))
            return true;
    }

    return QMainWindow::eventFilter(watched, event);
}

bool MotionController::handleMotionKey(QKeyEvent* event)
{
    int axis = -1;
    float dir = 1.0f;

    switch (event->key())
    {
    case Qt::Key_Up:         axis = Axis::X_AXIS;        dir =  1; break;
    case Qt::Key_Down:       axis = Axis::X_AXIS;        dir = -1; break;
    case Qt::Key_Left:       axis = Axis::Y_MASTER;      dir =  1; break;
    case Qt::Key_Right:      axis = Axis::Y_MASTER;      dir = -1; break;
    case Qt::Key_Home:       axis = Axis::Z_FRONT_AXIS;  dir =  1; break;
    case Qt::Key_End:        axis = Axis::Z_FRONT_AXIS;  dir = -1; break;
    case Qt::Key_PageUp:     axis = Axis::Z_BACK_AXIS;   dir =  1; break;
    case Qt::Key_PageDown:   axis = Axis::Z_BACK_AXIS;   dir = -1; break;
    default:
        return false;
    }

    event->accept();

    // Always consume motion keys so focused controls cannot react to them too.
    if (!handle || event->isAutoRepeat())
        return true;

    // 读对应轴的定长值，寸动
    QDoubleSpinBox* stepSpin = nullptr;
    switch (axis)
    {
    case Axis::X_AXIS:        stepSpin = ui.XdoubleSpinBox;  break;
    case Axis::Y_MASTER:      stepSpin = ui.YdoubleSpinBox;  break;
    case Axis::Z_FRONT_AXIS:  stepSpin = ui.ZQdoubleSpinBox; break;
    case Axis::Z_BACK_AXIS:   stepSpin = ui.ZHdoubleSpinBox; break;
    }

    if (stepSpin)
    {
        float step = stepSpin->value() * dir;
        ZAux_Direct_Single_Move(handle, axis, step);
    }

    return true;
}

// ==================== 扫描状态机 ====================

void MotionController::startScan()
{
    // 判断扫查方式
    bool isContinuous = (ui.ScanMethodComboBox->currentText() == "连续扫查");

    // 读取起点终点
    m_scanStartX = m_startPoint[0];  // X
    m_scanEndX   = m_endPoint[0];
    m_scanStartY = m_startPoint[1];  // Y
    m_scanEndY   = m_endPoint[1];

    // 检查起点终点是否已设置
    if (m_scanStartX == 0 && m_scanEndX == 0 && m_scanStartY == 0 && m_scanEndY == 0)
    {
        QMessageBox::warning(this, "提示", "请先设置起点和终点");
        return;
    }

    // ---------- 连续扫查：X/Y联动走一条斜线 ----------
    if (isContinuous)
    {
        // 先移动到起点
        ZAux_Direct_Single_MoveAbs(handle, Axis::X_AXIS, m_scanStartX);
        ZAux_Direct_Single_MoveAbs(handle, Axis::Y_MASTER, m_scanStartY);

        // 等待起点到位后，在状态机里发起联动
        m_scanState = SCAN_MOVE_TO_START;
        ui.StartInspectBtn->setText("停止检测");
        return;
    }

    // ---------- 栅格扫查 ----------
    // 步进间隔（Y每步移动量）
    m_stepGap = ui.StepGapDoubleSpinBox->value();
    if (m_stepGap <= 0)
    {
        QMessageBox::warning(this, "提示", "步进间隔必须大于0");
        return;
    }

    // 计算总步数
    float stepLength = qAbs(m_scanEndY - m_scanStartY);
    m_totalLines = (int)(stepLength / m_stepGap) + 1;
    if (m_totalLines < 1) m_totalLines = 1;

    m_currentLine = 0;
    m_forward = true;

    // 判断模式：设置区域 vs 获取区域
    if (ui.GainRegionRadioBtn->isChecked())
    {
        // 获取区域：自动计算扫查长度和步进长度，填回UI
        float scanLength = qAbs(m_scanEndX - m_scanStartX);
        ui.ScanLengthDoubleSpinBox->setValue(scanLength);
        ui.StepLengthDoubleSpinBox->setValue(stepLength);
    }

    // 启动状态机：先移动到起点
    m_scanState = SCAN_MOVE_TO_START;
    ZAux_Direct_Single_MoveAbs(handle, Axis::X_AXIS, m_scanStartX);
    ZAux_Direct_Single_MoveAbs(handle, Axis::Y_MASTER, m_scanStartY);

    // 更新按钮文字
    ui.StartInspectBtn->setText("停止检测");
}

void MotionController::stopScan()
{
    m_scanState = SCAN_IDLE;
    // 停止所有轴
    ZAux_Direct_Single_Cancel(handle, Axis::X_AXIS, 2);
    ZAux_Direct_Single_Cancel(handle, Axis::Y_MASTER, 2);
    ui.StartInspectBtn->setText("开始检测");
    ui.LeftTime->setText("");
}

void MotionController::scanStateMachine()
{
    // 检查当前运动是否完成
    // GetIfIdle: 输出 idle=0 运动中, idle=-1 停止
    int xIdle = 0, yIdle = 0;
    ZAux_Direct_GetIfIdle(handle, Axis::X_AXIS, &xIdle);
    ZAux_Direct_GetIfIdle(handle, Axis::Y_MASTER, &yIdle);
    bool xStop = (xIdle == -1);
    bool yStop = (yIdle == -1);

    switch (m_scanState)
    {
    case SCAN_MOVE_TO_START:
        // 等待 X 和 Y 都到位
        if (xStop && yStop)
        {
            // 判断是连续扫查还是栅格扫查
            if (ui.ScanMethodComboBox->currentText() == "连续扫查")
            {
                // 连续扫查：X/Y 两轴联动插补，从起点走斜线到终点
                int axisList[2] = {Axis::X_AXIS, Axis::Y_MASTER};
                float posList[2] = {m_scanEndX, m_scanEndY};
                ZAux_Direct_MoveAbs(handle, 2, axisList, posList);
                m_scanState = SCAN_CONTINUOUS;
            }
            else
            {
                // 栅格扫查：开始第一线 X 正向
                m_currentLine = 0;
                m_forward = true;
                m_scanState = SCAN_FORWARD;
                ZAux_Direct_Single_MoveAbs(handle, Axis::X_AXIS, m_scanEndX);
            }
        }
        break;

    case SCAN_CONTINUOUS:
        // 连续扫查：等待 X/Y 都到位即完成
        if (xStop && yStop)
        {
            ui.StartInspectBtn->setText("开始检测");
            ui.LeftTime->setText("扫描完成");
            m_scanState = SCAN_IDLE;
        }
        break;

    case SCAN_FORWARD:
        // X 正向扫查完成
        if (xStop)
        {
            m_currentLine++;
            if (m_currentLine >= m_totalLines)
            {
                // 扫描完成
                ui.StartInspectBtn->setText("开始检测");
                ui.LeftTime->setText("扫描完成");
                m_scanState = SCAN_IDLE;
                return;
            }
            // Y 步进（按起点终点线性插值，自动处理方向）
            m_scanState = SCAN_STEP;
            float ratio = (m_totalLines > 1) ? (float)m_currentLine / (m_totalLines - 1) : 1.0f;
            float yTarget = m_scanStartY + (m_scanEndY - m_scanStartY) * ratio;
            ZAux_Direct_Single_MoveAbs(handle, Axis::Y_MASTER, yTarget);
        }
        break;

    case SCAN_REVERSE:
        // X 反向扫查完成
        if (xStop)
        {
            m_currentLine++;
            if (m_currentLine >= m_totalLines)
            {
                ui.StartInspectBtn->setText("开始检测");
                ui.LeftTime->setText("扫描完成");
                m_scanState = SCAN_IDLE;
                return;
            }
            // Y 步进
            m_scanState = SCAN_STEP;
            float ratio = (m_totalLines > 1) ? (float)m_currentLine / (m_totalLines - 1) : 1.0f;
            float yTarget = m_scanStartY + (m_scanEndY - m_scanStartY) * ratio;
            ZAux_Direct_Single_MoveAbs(handle, Axis::Y_MASTER, yTarget);
        }
        break;

    case SCAN_STEP:
        // Y 步进完成
        if (yStop)
        {
            // 反向扫查
            m_forward = !m_forward;
            if (m_forward)
            {
                m_scanState = SCAN_FORWARD;
                ZAux_Direct_Single_MoveAbs(handle, Axis::X_AXIS, m_scanEndX);
            }
            else
            {
                m_scanState = SCAN_REVERSE;
                ZAux_Direct_Single_MoveAbs(handle, Axis::X_AXIS, m_scanStartX);
            }
        }
        break;

    default:
        break;
    }

    // 更新剩余时间（方法1: 理论估算）
    if (m_scanState != SCAN_IDLE)
    {
        float remaining = estimateRemainingTime();
        ui.LeftTime->setText(formatRemainingTime(remaining));
    }
}

// 梯形/三角速度曲线估算单段运动时间
float MotionController::estimateMoveTime(float distance, float speed, float acc, float dec)
{
    distance = qAbs(distance);
    if (distance < 0.001f || speed <= 0 || acc <= 0 || dec <= 0) return 0;

    // 加速段
    float t_acc = speed / acc;
    float d_acc = 0.5f * acc * t_acc * t_acc;

    // 减速段
    float t_dec = speed / dec;
    float d_dec = 0.5f * dec * t_dec * t_dec;

    if (distance >= d_acc + d_dec)
    {
        // 梯形曲线：加速 + 匀速 + 减速
        float d_cruise = distance - d_acc - d_dec;
        return t_acc + d_cruise / speed + t_dec;
    }
    else
    {
        // 三角曲线：距离不够，达不到最大速度
        float v_peak = sqrt(2.0f * distance * acc * dec / (acc + dec));
        return v_peak / acc + v_peak / dec;
    }
}

float MotionController::estimateRemainingTime()
{
    if (m_scanState == SCAN_IDLE || m_scanState == SCAN_DONE) return 0;

    float xDist = qAbs(m_scanEndX - m_scanStartX);
    const AxisParams& xP = m_axisParams[Axis::X_AXIS];
    const AxisParams& yP = m_axisParams[Axis::Y_MASTER];

    float xFullTime = estimateMoveTime(xDist, xP.m_speed, xP.m_acc, xP.m_dec);
    float yStepTime = estimateMoveTime(m_stepGap, yP.m_speed, yP.m_acc, yP.m_dec);

    int remainingLines = m_totalLines - m_currentLine;
    float remaining = 0;

    switch (m_scanState)
    {
    case SCAN_MOVE_TO_START:
    {
        // 移动到起点：X/Y 并行，取较大值 + 全部扫查线
        float curX = 0, curY = 0;
        if (handle)
        {
            ZAux_Direct_GetDpos(handle, Axis::X_AXIS, &curX);
            ZAux_Direct_GetDpos(handle, Axis::Y_MASTER, &curY);
        }
        float xMoveTime = estimateMoveTime(m_scanStartX - curX, xP.m_speed, xP.m_acc, xP.m_dec);
        float yMoveTime = estimateMoveTime(m_scanStartY - curY, yP.m_speed, yP.m_acc, yP.m_dec);
        remaining = qMax(xMoveTime, yMoveTime)
                  + m_totalLines * xFullTime
                  + (m_totalLines - 1) * yStepTime;
        break;
    }

    case SCAN_FORWARD:
    case SCAN_REVERSE:
    {
        // 当前 X 扫查进行中：读当前位置算剩余距离
        float curX = 0;
        if (handle) ZAux_Direct_GetDpos(handle, Axis::X_AXIS, &curX);
        float targetX = m_forward ? m_scanEndX : m_scanStartX;
        float xRemaining = qAbs(targetX - curX);
        float xRemTime = estimateMoveTime(xRemaining, xP.m_speed, xP.m_acc, xP.m_dec);

        // 当前线剩余 + 后续线（每线 = 1次X扫查 + 1次Y步进，最后一线无Y步进）
        remaining = xRemTime
                  + (remainingLines - 1) * xFullTime
                  + (remainingLines - 1) * yStepTime;
        break;
    }

    case SCAN_STEP:
    {
        // 当前 Y 步进进行中：读当前位置算剩余距离
        float curY = 0;
        if (handle) ZAux_Direct_GetDpos(handle, Axis::Y_MASTER, &curY);
        float ratio = (m_totalLines > 1) ? (float)m_currentLine / (m_totalLines - 1) : 1.0f;
        float yTarget = m_scanStartY + (m_scanEndY - m_scanStartY) * ratio;
        float yRemaining = qAbs(yTarget - curY);
        float yRemTime = estimateMoveTime(yRemaining, yP.m_speed, yP.m_acc, yP.m_dec);

        // 当前Y步进剩余 + 后续线
        remaining = yRemTime
                  + (remainingLines - 1) * xFullTime
                  + (remainingLines - 1) * yStepTime;
        break;
    }

    case SCAN_CONTINUOUS:
    {
        // 连续扫查：X/Y联动中，读当前位置算剩余时间（取两轴较大值）
        float curX = 0, curY = 0;
        if (handle)
        {
            ZAux_Direct_GetDpos(handle, Axis::X_AXIS, &curX);
            ZAux_Direct_GetDpos(handle, Axis::Y_MASTER, &curY);
        }
        float xRemaining = qAbs(m_scanEndX - curX);
        float yRemaining = qAbs(m_scanEndY - curY);
        float xRemTime = estimateMoveTime(xRemaining, xP.m_speed, xP.m_acc, xP.m_dec);
        float yRemTime = estimateMoveTime(yRemaining, yP.m_speed, yP.m_acc, yP.m_dec);
        remaining = qMax(xRemTime, yRemTime);
        break;
    }

    default:
        break;
    }

    return remaining;
}

QString MotionController::formatRemainingTime(float seconds)
{
    if (seconds <= 0) return "0:00";

    int totalSec = (int)(seconds + 0.5f);
    int hours = totalSec / 3600;
    int mins  = (totalSec % 3600) / 60;
    int secs  = totalSec % 60;

    if (hours > 0)
        return QString("剩余 %1:%2:%3")
            .arg(hours)
            .arg(mins,  2, 10, QChar('0'))
            .arg(secs,  2, 10, QChar('0'));
    else
        return QString("剩余 %1:%2")
            .arg(mins, 2, 10, QChar('0'))
            .arg(secs, 2, 10, QChar('0'));
}
