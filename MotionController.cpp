#include "MotionController.h"
#include "MotionController/Services/ZmcAdapter.h"
#include "MotionController/Services/AxisService.h"
#include "MotionController/Services/HomeService.h"
#include "MotionController/Services/ScanService.h"

#include <QDebug>

#include <QMessageBox>
#include <QSettings>
#include <QCoreApplication>
#include <QApplication>
#include <QCloseEvent>
#include <QKeyEvent>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QTimer>

// ============================================================================
// 构造 / 析构
// ============================================================================

MotionController::MotionController(QWidget* parent)
	: QMainWindow(parent)
{
	ui.setupUi(this);

	// ---- 初始化适配器层 ----
	m_adapter = new ZmcAdapter(this);

	// ---- 初始化服务层（构造函数注入） ----
	m_axisService = new AxisService(m_adapter, this);
	m_homeService = new HomeService(m_adapter, this);
	m_scanService = new ScanService(m_adapter, this);

	// ---- 安装事件过滤器（键盘快捷键） ----
	qApp->installEventFilter(this);

	// ---- 定时器（100ms 周期） ----
	updateTimer = new QTimer(this);
	updateTimer->setInterval(100);
	connect(updateTimer, &QTimer::timeout, this, &MotionController::updateAllAxisParameters);
	updateTimer->start();

	// ---- 绑定 AxisConfig ----
	m_axisList[0] = { Axis::X_AXIS,       ui.XStateLineEdit,   ui.XLocateLineEdit,   ui.XSpeedLineEdit };
	m_axisList[1] = { Axis::Y_MASTER,     ui.YStateLineEdit,   ui.YLocateLineEdit,   ui.YSpeedLineEdit };
	m_axisList[2] = { Axis::Z_FRONT_AXIS, ui.ZQStateLineEdit,  ui.ZQLocateLineEdit,  ui.ZQSpeedLineEdit };
	m_axisList[3] = { Axis::Z_BACK_AXIS,  ui.ZHStateLineEdit,  ui.ZHLocateLineEdit,  ui.ZHSpeedLineEdit };

	// ---- 绑定 HomeService 信号 ----
	connect(m_homeService, &HomeService::homingStarted, this, &MotionController::onHomeStarted);
	connect(m_homeService, &HomeService::homingStopped, this, &MotionController::onHomeStopped);
	connect(m_homeService, &HomeService::homeCompleted, this, &MotionController::onHomeCompleted);
	connect(m_homeService, &HomeService::homeFailed, this, &MotionController::onHomeFailed);

	// ---- 绑定 ScanService 信号 ----
	connect(m_scanService, &ScanService::scanStarted, this, &MotionController::onScanStarted);
	connect(m_scanService, &ScanService::scanStopped, this, &MotionController::onScanStopped);
	connect(m_scanService, &ScanService::scanCompleted, this, &MotionController::onScanCompleted);

	// ---- 加载上次配置 ----
	loadConfig();
}

MotionController::~MotionController()
{
	qApp->removeEventFilter(this);
	m_adapter->setOutput(0, false);
	m_adapter->setOutput(1, false);
	m_adapter->setOutput(2, false);
	m_lastLedState = LED_OFF;
	saveConfig();
}

void MotionController::closeEvent(QCloseEvent* event)
{
	// 停止正在运行的服务
	if (m_homeService->isHoming())  m_homeService->stopHome();
	if (m_scanService->isRunning()) m_scanService->stopScan();

	// 关闭所有输出口（OP0=报警, OP1=回零, OP2=正常）
	m_adapter->setOutput(0, false);
	m_adapter->setOutput(1, false);
	m_adapter->setOutput(2, false);
	m_lastLedState = LED_OFF;

	saveConfig();
	event->accept();
}

// ============================================================================
// 连接管理
// ============================================================================

void MotionController::on_ConnectBtn_clicked()
{
	QString ip = ui.IPComboBox->currentText();

	if (!m_adapter->connect(ip))
	{
		QMessageBox::critical(this, "Connection Error", "Failed to connect to the motion controller.");
		return;
	}

	ui.ConnectBtn->setEnabled(false);
	ui.DisconnectBtn->setEnabled(true);
	ui.IPComboBox->setEnabled(false);

	QSettings settings(configFilePath(), QSettings::IniFormat);
	settings.setValue("Connection/IP", ip);

	// 连接成功后下发全部参数
	m_axisService->applyParametersFromConfig(m_axisParams);
}

void MotionController::on_DisconnectBtn_clicked()
{
	// 先停回零和扫描
	if (m_homeService->isHoming())  m_homeService->stopHome();
	if (m_scanService->isRunning()) m_scanService->stopScan();

	// 断开前关闭所有指示灯
	m_adapter->setOutput(0, false);
	m_adapter->setOutput(1, false);
	m_adapter->setOutput(2, false);
	m_lastLedState = LED_OFF;

	m_adapter->disconnect();

	ui.ConnectBtn->setEnabled(true);
	ui.DisconnectBtn->setEnabled(false);
	ui.IPComboBox->setEnabled(true);
	ui.OriginBtn->setText("原点回零");
	ui.StartInspectBtn->setText("开始检测");
}

// ============================================================================
// 原点回零（委托给 HomeService 异步状态机）
// ============================================================================

void MotionController::on_OriginBtn_clicked()
{
	if (!m_adapter->isConnected())
	{
		QMessageBox::warning(this, "提示", "请先连接控制器");
		return;
	}

	// 正在回零中 → 停止
	if (m_homeService->isHoming())
	{
		m_homeService->stopHome();
		return;
	}

	// 检查各轴是否空闲
	int axes[] = { Axis::X_AXIS, Axis::Y_MASTER, Axis::Z_FRONT_AXIS, Axis::Z_BACK_AXIS };
	for (int axis : axes)
	{
		int idle = m_adapter->getIdleState(axis);
		if (idle != -1)
		{
			QMessageBox::warning(this, "提示",
				QString("轴 %1 正在运动中，请先停止").arg(axis));
			return;
		}
	}

	// 先下发参数，再启动异步回零
	m_axisService->applyParametersFromConfig(m_axisParams);
	m_homeService->startHome(m_axisParams);
}

void MotionController::onHomeStarted()
{
	ui.OriginBtn->setText("停止回零");
}

void MotionController::onHomeStopped()
{
	ui.OriginBtn->setText("原点回零");
}

void MotionController::onHomeCompleted()
{
	ui.OriginBtn->setText("原点回零");
	QMessageBox::information(this, "提示", "所有轴回零完成，原点已置零");
}

void MotionController::onHomeFailed(const QString& reason)
{
	ui.OriginBtn->setText("原点回零");
	QMessageBox::critical(this, "回零失败", reason);
}

// ============================================================================
// 开始检测 / 停止检测（委托给 ScanService）
// ============================================================================

void MotionController::on_StartInspectBtn_clicked()
{
	if (!m_adapter->isConnected())
	{
		QMessageBox::warning(this, "提示", "请先连接控制器");
		return;
	}

	if (m_scanService->isRunning())
	{
		m_scanService->stopScan();
		return;
	}

	// 读取 UI 参数
	bool isContinuous = (ui.ScanMethodComboBox->currentText() == "连续扫查");

	if (m_startPoint[0] == 0 && m_endPoint[0] == 0 &&
		m_startPoint[1] == 0 && m_endPoint[1] == 0)
	{
		QMessageBox::warning(this, "提示", "请先设置起点和终点");
		return;
	}

	float stepGap = ui.StepGapDoubleSpinBox->value();
	if (!isContinuous && stepGap <= 0)
	{
		QMessageBox::warning(this, "提示", "步进间隔必须大于0");
		return;
	}

	// 栅格扫查：计算总步数
	int totalLines = 1;
	if (!isContinuous)
	{
		float stepLength = qAbs(m_endPoint[1] - m_startPoint[1]);
		totalLines = (int)(stepLength / stepGap) + 1;
		if (totalLines < 1) totalLines = 1;
	}

	m_scanService->startScan(isContinuous, m_startPoint, m_endPoint,
		stepGap, totalLines, m_axisParams);
}

void MotionController::onScanStarted()
{
	ui.StartInspectBtn->setText("停止检测");
}

void MotionController::onScanStopped()
{
	ui.StartInspectBtn->setText("开始检测");
	ui.LeftTime->setText("");
}

void MotionController::onScanCompleted()
{
	ui.StartInspectBtn->setText("开始检测");
	ui.LeftTime->setText("扫描完成");
}

// ============================================================================
// 扫查计划
// ============================================================================

void MotionController::on_StartPointBtn_clicked()
{
	if (!m_adapter->isConnected())
	{
		QMessageBox::warning(this, "提示", "请先连接控制器");
		return;
	}
	auto ret = QMessageBox::warning(this, "提示", "是否设置当前点为起点？", QMessageBox::StandardButton::Ok | QMessageBox::StandardButton::No, QMessageBox::StandardButton::No);
	if (ret != QMessageBox::StandardButton::Ok)
		return;
	int axes[4] = { Axis::X_AXIS, Axis::Y_MASTER, Axis::Z_FRONT_AXIS, Axis::Z_BACK_AXIS };
	const char* labels[4] = { "X", "Y", "ZQ", "ZH" };
	QString text;
	for (int i = 0; i < 4; i++)
	{
		m_startPoint[i] = m_adapter->getPosition(axes[i]);
		if (i > 0) text += " ";
		text += QString("%1:%2").arg(labels[i]).arg(m_startPoint[i], 0, 'f', 2);
	}
	ui.lineEdit_4->setText(text);
	updateScanRegionLength();
}

void MotionController::on_EndBtn_clicked()
{
	if (!m_adapter->isConnected())
	{
		QMessageBox::warning(this, "提示", "请先连接控制器");
		return;
	}
	auto ret = QMessageBox::warning(this, "提示", "是否设置当前点为终点？", QMessageBox::StandardButton::Ok, QMessageBox::StandardButton::No);
	if (ret != QMessageBox::StandardButton::Ok)
		return;
	int axes[4] = { Axis::X_AXIS, Axis::Y_MASTER, Axis::Z_FRONT_AXIS, Axis::Z_BACK_AXIS };
	const char* labels[4] = { "X", "Y", "ZQ", "ZH" };
	QString text;
	for (int i = 0; i < 4; i++)
	{
		m_endPoint[i] = m_adapter->getPosition(axes[i]);
		if (i > 0) text += " ";
		text += QString("%1:%2").arg(labels[i]).arg(m_endPoint[i], 0, 'f', 2);
	}
	ui.lineEdit_5->setText(text);
	updateScanRegionLength();
}

void MotionController::on_BackStartPointBtn_clicked()
{
	if (!m_adapter->isConnected())
	{
		QMessageBox::warning(this, "提示", "请先连接控制器");
		return;
	}
	int axes[4] = { Axis::X_AXIS, Axis::Y_MASTER, Axis::Z_FRONT_AXIS, Axis::Z_BACK_AXIS };
	for (int i = 0; i < 4; i++)
		m_adapter->moveAbsolute(axes[i], m_startPoint[i]);
}

void MotionController::on_BackEndBtn_clicked()
{
	if (!m_adapter->isConnected())
	{
		QMessageBox::warning(this, "提示", "请先连接控制器");
		return;
	}
	int axes[4] = { Axis::X_AXIS, Axis::Y_MASTER, Axis::Z_FRONT_AXIS, Axis::Z_BACK_AXIS };
	for (int i = 0; i < 4; i++)
		m_adapter->moveAbsolute(axes[i], m_endPoint[i]);
}

void MotionController::on_GainRegionRadioBtn_clicked()
{
	updateScanRegionLength();
}

void MotionController::on_StepDoubleSpinBox_valueChanged(double arg1)
{
	ui.XdoubleSpinBox->setValue(arg1);
	ui.YdoubleSpinBox->setValue(arg1);
	ui.ZQdoubleSpinBox->setValue(arg1);
	ui.ZHdoubleSpinBox->setValue(arg1);
}

void MotionController::updateScanRegionLength()
{
	if (!ui.GainRegionRadioBtn->isChecked()) return;
	ui.ScanLengthDoubleSpinBox->setValue(qAbs(m_endPoint[0] - m_startPoint[0]));
	ui.StepLengthDoubleSpinBox->setValue(qAbs(m_endPoint[1] - m_startPoint[1]));
}

// ============================================================================
// 轴参数设置（ParaSettings 弹窗）
// ============================================================================

void MotionController::on_XParaBtn_clicked()
{
	if (!m_adapter->isConnected()) { QMessageBox::warning(this, "提示", "请先连接控制器"); return; }
	ParaSettings* dlg = new ParaSettings(Axis::X_AXIS, "X Axis", nullptr);
	dlg->setAttribute(Qt::WA_DeleteOnClose);
	connect(dlg, &ParaSettings::parametersChanged, this, &MotionController::applyAxisParameters);
	dlg->show();
}

void MotionController::on_YParaBtn_clicked()
{
	if (!m_adapter->isConnected()) { QMessageBox::warning(this, "提示", "请先连接控制器"); return; }
	ParaSettings* dlg = new ParaSettings(Axis::Y_MASTER, "Y Axis", nullptr);
	dlg->setAttribute(Qt::WA_DeleteOnClose);
	connect(dlg, &ParaSettings::parametersChanged, this, &MotionController::applyAxisParameters);
	dlg->show();
}

void MotionController::on_ZQParaBtn_clicked()
{
	if (!m_adapter->isConnected()) { QMessageBox::warning(this, "提示", "请先连接控制器"); return; }
	ParaSettings* dlg = new ParaSettings(Axis::Z_FRONT_AXIS, "Z Front Axis", nullptr);
	dlg->setAttribute(Qt::WA_DeleteOnClose);
	connect(dlg, &ParaSettings::parametersChanged, this, &MotionController::applyAxisParameters);
	dlg->show();
}

void MotionController::on_ZHParaBtn_clicked()
{
	if (!m_adapter->isConnected()) { QMessageBox::warning(this, "提示", "请先连接控制器"); return; }
	ParaSettings* dlg = new ParaSettings(Axis::Z_BACK_AXIS, "Z Back Axis", nullptr);
	dlg->setAttribute(Qt::WA_DeleteOnClose);
	connect(dlg, &ParaSettings::parametersChanged, this, &MotionController::applyAxisParameters);
	dlg->show();
}

void MotionController::applyAxisParameters(int axis, const AxisParams& params)
{
	m_axisParams[axis] = params;
	m_axisService->applyParameters(axis, params);
}

// ============================================================================
// 四轴运动控制（委托给 AxisService）
// ============================================================================

// ---- 清零 ----
void MotionController::on_XClearBtn_clicked()
{
	if (!m_adapter->isConnected()) { QMessageBox::warning(this, "提示", "请先连接控制器"); return; }
	m_axisService->clearPosition(Axis::X_AXIS);
}
void MotionController::on_YClearBtn_clicked()
{
	if (!m_adapter->isConnected()) { QMessageBox::warning(this, "提示", "请先连接控制器"); return; }
	m_axisService->clearPosition(Axis::Y_MASTER);
}
void MotionController::on_ZQClearBtn_clicked()
{
	if (!m_adapter->isConnected()) { QMessageBox::warning(this, "提示", "请先连接控制器"); return; }
	m_axisService->clearPosition(Axis::Z_FRONT_AXIS);
}
void MotionController::on_ZHClearBtn_clicked()
{
	if (!m_adapter->isConnected()) { QMessageBox::warning(this, "提示", "请先连接控制器"); return; }
	m_axisService->clearPosition(Axis::Z_BACK_AXIS);
}

// ---- 正转 ----
void MotionController::on_XNormalBtn_clicked()
{
	if (!m_adapter->isConnected()) { QMessageBox::warning(this, "提示", "请先连接控制器"); return; }
	m_axisService->movePositive(Axis::X_AXIS, ui.checkBox_6->isChecked(), ui.XdoubleSpinBox->value());
}
void MotionController::on_YNormalBtn_clicked()
{
	if (!m_adapter->isConnected()) { QMessageBox::warning(this, "提示", "请先连接控制器"); return; }
	m_axisService->movePositive(Axis::Y_MASTER, ui.checkBox_7->isChecked(), ui.YdoubleSpinBox->value());
}
void MotionController::on_ZQNormalBtn_clicked()
{
	if (!m_adapter->isConnected()) { QMessageBox::warning(this, "提示", "请先连接控制器"); return; }
	m_axisService->movePositive(Axis::Z_FRONT_AXIS, ui.checkBox_8->isChecked(), ui.ZQdoubleSpinBox->value());
}
void MotionController::on_ZHNormalBtn_clicked()
{
	if (!m_adapter->isConnected()) { QMessageBox::warning(this, "提示", "请先连接控制器"); return; }
	m_axisService->movePositive(Axis::Z_BACK_AXIS, ui.checkBox_10->isChecked(), ui.ZHdoubleSpinBox->value());
}

// ---- 反转 ----
void MotionController::on_XReverseBtn_clicked()
{
	if (!m_adapter->isConnected()) { QMessageBox::warning(this, "提示", "请先连接控制器"); return; }
	m_axisService->moveNegative(Axis::X_AXIS, ui.checkBox_6->isChecked(), ui.XdoubleSpinBox->value());
}
void MotionController::on_YReverseBtn_clicked()
{
	if (!m_adapter->isConnected()) { QMessageBox::warning(this, "提示", "请先连接控制器"); return; }
	m_axisService->moveNegative(Axis::Y_MASTER, ui.checkBox_7->isChecked(), ui.YdoubleSpinBox->value());
}
void MotionController::on_ZQReverseBtn_clicked()
{
	if (!m_adapter->isConnected()) { QMessageBox::warning(this, "提示", "请先连接控制器"); return; }
	m_axisService->moveNegative(Axis::Z_FRONT_AXIS, ui.checkBox_8->isChecked(), ui.ZQdoubleSpinBox->value());
}
void MotionController::on_ZHReverseBtn_clicked()
{
	if (!m_adapter->isConnected()) { QMessageBox::warning(this, "提示", "请先连接控制器"); return; }
	m_axisService->moveNegative(Axis::Z_BACK_AXIS, ui.checkBox_10->isChecked(), ui.ZHdoubleSpinBox->value());
}

// ---- 停止 ----
void MotionController::on_XStopBtn_clicked()
{
	if (!m_adapter->isConnected()) { QMessageBox::warning(this, "提示", "请先连接控制器"); return; }
	m_axisService->stop(Axis::X_AXIS);
}
void MotionController::on_YStopBtn_clicked()
{
	if (!m_adapter->isConnected()) { QMessageBox::warning(this, "提示", "请先连接控制器"); return; }
	m_axisService->stop(Axis::Y_MASTER);
}
void MotionController::on_ZQStopBtn_clicked()
{
	if (!m_adapter->isConnected()) { QMessageBox::warning(this, "提示", "请先连接控制器"); return; }
	m_axisService->stop(Axis::Z_FRONT_AXIS);
}
void MotionController::on_ZHStopBtn_clicked()
{
	if (!m_adapter->isConnected()) { QMessageBox::warning(this, "提示", "请先连接控制器"); return; }
	m_axisService->stop(Axis::Z_BACK_AXIS);
}

// ============================================================================
// 100ms 定时器 — 状态轮询 + 服务驱动
// ============================================================================

void MotionController::updateAllAxisParameters()
{
	bool isConnected = m_adapter->isConnected();

	// 1. 刷新四轴 UI 状态
	for (int i = 0; i < 4; ++i)
	{
		const AxisConfig& cfg = m_axisList[i];

		if (!isConnected)
		{
			cfg.stateLineEdit->setText("未连接");
			cfg.locateLineEdit->setText("0.00");
			cfg.speedLineEdit->setText("0.00");
			continue;
		}

		AxisService::AxisStatus s = m_axisService->getStatus(cfg.axisNo, m_axisParams[cfg.axisNo]);
		cfg.stateLineEdit->setText(s.stateText);
		cfg.locateLineEdit->setText(QString::number(s.position, 'f', 2));
		cfg.speedLineEdit->setText(QString::number(s.speed, 'f', 2));
	}

	// 2. 驱动 HomeService 异步状态机
	m_homeService->tick();

	// 3. 驱动 ScanService 状态机 + 更新剩余时间
	m_scanService->tick();

	if (m_scanService->isRunning())
	{
		float remaining = m_scanService->getRemainingTime();
		if (remaining <= 0)
			ui.LeftTime->setText("0:00");
		else
		{
			int totalSec = (int)(remaining + 0.5f);
			int hours = totalSec / 3600;
			int mins = (totalSec % 3600) / 60;
			int secs = totalSec % 60;
			if (hours > 0)
				ui.LeftTime->setText(QString("剩余 %1:%2:%3")
					.arg(hours)
					.arg(mins, 2, 10, QChar('0'))
					.arg(secs, 2, 10, QChar('0')));
			else
				ui.LeftTime->setText(QString("剩余 %1:%2")
					.arg(mins, 2, 10, QChar('0'))
					.arg(secs, 2, 10, QChar('0')));
		}
	}

	// 4. 根据轴状态控制指示灯（OP0=报警, OP1=回零, OP2=正常）
	LedState newLed = LED_OFF;

	if (isConnected)
	{
		// 检查四轴是否报警
		int axes[] = { Axis::X_AXIS, Axis::Y_MASTER, Axis::Z_FRONT_AXIS, Axis::Z_BACK_AXIS };
		bool hasAlarm = false;
		for (int axis : axes)
		{
			if (m_adapter->isAxisAlarm(axis))
			{
				hasAlarm = true;
				break;
			}
		}

		// 优先级：报警 > 回零 > 正常
		if (hasAlarm)
		{
			newLed = LED_ALARM;
		}
		else if (m_homeService->isHoming())
		{
			newLed = LED_HOME;
		}
		else
		{
			newLed = LED_NORMAL;
		}

		// 回零期间诊断日志
		if (m_homeService->isHoming())
			qDebug() << "[LED-DIAG] isHoming=true hasAlarm=" << hasAlarm
				<< "-> newLed=" << (hasAlarm ? "ALARM" : "HOME");
	}

	// 仅在状态变化时写输出口，减少总线通信
	if (newLed != m_lastLedState)
	{
		const char* ledNames[] = { "OFF", "ALARM", "HOME", "NORMAL" };
		qDebug() << "LED state change:" << ledNames[m_lastLedState] << "->" << ledNames[newLed]
			<< "| OP0=" << (newLed == LED_ALARM)
			<< "OP1=" << (newLed == LED_HOME)
			<< "OP2=" << (newLed == LED_NORMAL);
		m_adapter->setOutput(0, newLed == LED_ALARM);
		m_adapter->setOutput(1, newLed == LED_HOME);
		m_adapter->setOutput(2, newLed == LED_NORMAL);
		m_lastLedState = newLed;
	}
}

// ============================================================================
// 配置持久化
// ============================================================================

void MotionController::loadConfig()
{
	QSettings settings(configFilePath(), QSettings::IniFormat);

	QString lastIP = settings.value("Connection/IP", "192.168.0.11").toString();
	ui.IPComboBox->setCurrentText(lastIP);

	double step = settings.value("Scan/Step", 10.0).toDouble();
	ui.StepDoubleSpinBox->setValue(step);

	// 预加载各轴参数到缓存（未连接时只缓存不下发）
	m_axisService->applyParametersFromConfig(m_axisParams);
}

void MotionController::saveConfig()
{
	QSettings settings(configFilePath(), QSettings::IniFormat);
	settings.setValue("Connection/IP", ui.IPComboBox->currentText());
	settings.setValue("Scan/Step", ui.StepDoubleSpinBox->value());
}

// ============================================================================
// 键盘快捷键（简化：仅 eventFilter，移除 keyPressEvent 冗余）
// ============================================================================

bool MotionController::eventFilter(QObject* watched, QEvent* event)
{
	// 只处理本窗口内的事件
	if (event->type() == QEvent::KeyPress && watched->isWidgetType())
	{
		QWidget* widget = static_cast<QWidget*>(watched);
		if (widget->window() == this)
		{
			if (handleMotionKey(static_cast<QKeyEvent*>(event)))
				return true;
		}
	}

	return QMainWindow::eventFilter(watched, event);
}

bool MotionController::handleMotionKey(QKeyEvent* event)
{
	int   axis = -1;
	float dir = 1.0f;

	switch (event->key())
	{
	case Qt::Key_Up:       axis = Axis::Y_MASTER;      dir = 1; break;
	case Qt::Key_Down:     axis = Axis::Y_MASTER;      dir = -1; break;
	case Qt::Key_Left:     axis = Axis::X_AXIS;        dir = -1; break;
	case Qt::Key_Right:    axis = Axis::X_AXIS;        dir = 1; break;
	case Qt::Key_Home:     axis = Axis::Z_FRONT_AXIS;  dir = 1; break;
	case Qt::Key_End:      axis = Axis::Z_FRONT_AXIS;  dir = -1; break;
	case Qt::Key_PageUp:   axis = Axis::Z_BACK_AXIS;   dir = 1; break;
	case Qt::Key_PageDown: axis = Axis::Z_BACK_AXIS;   dir = -1; break;
	default:
		return false;
	}

	event->accept();

	// 总是消费运动按键，同时阻止自动重复和未连接时执行
	if (!m_adapter->isConnected() || event->isAutoRepeat())
		return true;

	// 键盘始终寸动模式
	QDoubleSpinBox* stepSpin = nullptr;
	switch (axis)
	{
	case Axis::X_AXIS:        stepSpin = ui.XdoubleSpinBox;  break;
	case Axis::Y_MASTER:      stepSpin = ui.YdoubleSpinBox;  break;
	case Axis::Z_FRONT_AXIS:  stepSpin = ui.ZQdoubleSpinBox; break;
	case Axis::Z_BACK_AXIS:   stepSpin = ui.ZHdoubleSpinBox; break;
	}

	if (stepSpin)
		m_adapter->moveRelative(axis, stepSpin->value() * dir);

	return true;
}
