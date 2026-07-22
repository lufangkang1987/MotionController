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
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QTimer>

// ============================================================================
// Construction and destruction
// ============================================================================

MotionController::MotionController(QWidget* parent)
	: QMainWindow(parent)
{
	ui.setupUi(this);

	// ---- Initialize adapter layer ----
	m_adapter = new ZmcAdapter(this);

	// ---- Initialize service layer ----
	m_axisService = new AxisService(m_adapter, this);
	m_homeService = new HomeService(m_adapter, this);
	m_scanService = new ScanService(m_adapter, this);

	// ---- Install keyboard event filter ----
	qApp->installEventFilter(this);

	// ---- Start 100ms update timer ----
	updateTimer = new QTimer(this);
	updateTimer->setInterval(100);
	connect(updateTimer, &QTimer::timeout, this, &MotionController::updateAllAxisParameters);
	updateTimer->start();

	// ---- Bind axis status UI fields ----
	m_axisList[0] = { Axis::X_AXIS,       ui.XStateLineEdit,   ui.XLocateLineEdit,   ui.XSpeedLineEdit };
	m_axisList[1] = { Axis::Y_MASTER,     ui.YStateLineEdit,   ui.YLocateLineEdit,   ui.YSpeedLineEdit };
	m_axisList[2] = { Axis::Z_FRONT_AXIS, ui.ZQStateLineEdit,  ui.ZQLocateLineEdit,  ui.ZQSpeedLineEdit };
	m_axisList[3] = { Axis::Z_BACK_AXIS,  ui.ZHStateLineEdit,  ui.ZHLocateLineEdit,  ui.ZHSpeedLineEdit };

	// ---- Connect HomeService signals ----
	connect(m_homeService, &HomeService::homingStarted, this, &MotionController::onHomeStarted);
	connect(m_homeService, &HomeService::homingStopped, this, &MotionController::onHomeStopped);
	connect(m_homeService, &HomeService::homeCompleted, this, &MotionController::onHomeCompleted);
	connect(m_homeService, &HomeService::homeFailed, this, &MotionController::onHomeFailed);

	// ---- Connect ScanService signals ----
	connect(m_scanService, &ScanService::scanStarted, this, &MotionController::onScanStarted);
	connect(m_scanService, &ScanService::scanPaused, this, &MotionController::onScanPaused);
	connect(m_scanService, &ScanService::scanResumed, this, &MotionController::onScanResumed);
	connect(m_scanService, &ScanService::scanStopped, this, &MotionController::onScanStopped);
	connect(m_scanService, &ScanService::scanCompleted, this, &MotionController::onScanCompleted);

	// ---- Ensure config exists, then load settings ----
	ensureConfigFileExists();
	loadConfig();
}

MotionController::~MotionController()
{
	qApp->removeEventFilter(this);
	m_adapter->setOutput(m_alarmLedOp, false);
	m_adapter->setOutput(m_homeLedOp, false);
	m_adapter->setOutput(m_normalLedOp, false);
	m_adapter->setOutput(m_buzzerOp, false);
	m_lastLedState = LED_OFF;
	m_scanLedOverride = LED_OFF;
	saveConfig();
}

void MotionController::closeEvent(QCloseEvent* event)
{
	// Stop active services before closing.
	if (m_homeService->isHoming())  m_homeService->stopHome();
	if (m_scanService->hasActiveScan()) m_scanService->stopScan();

	// Turn off all outputs before closing.
	m_adapter->setOutput(m_alarmLedOp, false);
	m_adapter->setOutput(m_homeLedOp, false);
	m_adapter->setOutput(m_normalLedOp, false);
	m_adapter->setOutput(m_buzzerOp, false);
	m_lastLedState = LED_OFF;
	m_scanLedOverride = LED_OFF;

	saveConfig();
	event->accept();
}

// ============================================================================
// Connection management
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

	// Apply all cached axis parameters after connecting.
	m_axisService->applyParametersFromConfig(m_axisParams);
}

void MotionController::on_DisconnectBtn_clicked()
{
	// Stop homing and scanning before disconnecting.
	if (m_homeService->isHoming())  m_homeService->stopHome();
	if (m_scanService->hasActiveScan()) m_scanService->stopScan();

	// Turn off all outputs before disconnecting.
	m_adapter->setOutput(m_alarmLedOp, false);
	m_adapter->setOutput(m_homeLedOp, false);
	m_adapter->setOutput(m_normalLedOp, false);
	m_adapter->setOutput(m_buzzerOp, false);
	m_lastLedState = LED_OFF;
	m_scanLedOverride = LED_OFF;

	m_adapter->disconnect();

	ui.ConnectBtn->setEnabled(true);
	ui.DisconnectBtn->setEnabled(false);
	ui.IPComboBox->setEnabled(true);
	ui.OriginBtn->setText(QStringLiteral("原点回零"));
	ui.StartInspectBtn->setText(QStringLiteral("开始检测"));
	ui.StopInspectBtn->setText(QStringLiteral("停止检测"));
	m_scanResetPending = false;
}

// ============================================================================
// Home operation, delegated to HomeService.
// ============================================================================

void MotionController::on_OriginBtn_clicked()
{
	if (!m_adapter->isConnected())
	{
		QMessageBox::warning(this, "Warning", "Connect controller first.");
		return;
	}

	// Stop homing if it is already running.
	if (m_homeService->isHoming())
	{
		m_homeService->stopHome();
		return;
	}

	// Check that every axis is idle before homing.
	int axes[] = { Axis::X_AXIS, Axis::Y_MASTER, Axis::Z_FRONT_AXIS, Axis::Z_BACK_AXIS };
	for (int axis : axes)
	{
		int idle = m_adapter->getIdleState(axis);
		if (idle != -1)
		{
			QMessageBox::warning(this, "Warning",
				QString("Axis %1 is moving. Stop it first.").arg(axis));
			return;
		}
	}

	// Apply parameters first, then start asynchronous homing.
	m_axisService->applyParametersFromConfig(m_axisParams);
	m_homeService->startHome(m_axisParams);
}

void MotionController::onHomeStarted()
{
	ui.OriginBtn->setText(QStringLiteral("原点回零"));
}

void MotionController::onHomeStopped()
{
	ui.OriginBtn->setText(QStringLiteral("原点回零"));
}

void MotionController::onHomeCompleted()
{
	ui.OriginBtn->setText(QStringLiteral("原点回零"));
	QMessageBox::information(this, "Info", "Home completed.");
}

void MotionController::onHomeFailed(const QString& reason)
{
	ui.OriginBtn->setText(QStringLiteral("原点回零"));
	QMessageBox::critical(this, "Home Failed", reason);
}

// ============================================================================
// Scan start, pause, resume, and stop operations.
// ============================================================================

void MotionController::on_StartInspectBtn_clicked()
{
	if (!m_adapter->isConnected())
	{
		QMessageBox::warning(this, "Warning", "Connect controller first.");
		return;
	}

	if (m_scanService->isPaused())
	{
		auto ret = QMessageBox::question(this, "Confirm Resume Scan",
			"Scan is paused. Resume from the paused position?",
			QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
		if (ret != QMessageBox::Yes)
			return;

		m_scanService->resumeScan();
		return;
	}

	if (m_scanService->isRunning())
	{
		auto ret = QMessageBox::warning(this, "Confirm Pause Scan",
			"Scan is running. Pause the current scan?",
			QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
		if (ret != QMessageBox::Yes)
			return;

		m_scanService->pauseScan();
		return;
	}

	bool isContinuous = (ui.ScanMethodComboBox->currentIndex() == 1);

	if (m_startPoint[0] == 0 && m_endPoint[0] == 0 &&
		m_startPoint[1] == 0 && m_endPoint[1] == 0)
	{
		QMessageBox::warning(this, "Warning", "Set start and end points first.");
		return;
	}

	float stepGap = ui.StepGapDoubleSpinBox->value();
	if (!isContinuous && stepGap <= 0)
	{
		QMessageBox::warning(this, "Warning", "Step gap must be greater than 0.");
		return;
	}

	int totalLines = 1;
	if (!isContinuous)
	{
		float stepLength = qAbs(m_endPoint[1] - m_startPoint[1]);
		totalLines = (int)(stepLength / stepGap) + 1;
		if (totalLines < 1) totalLines = 1;
	}

	auto ret = QMessageBox::question(this, "Confirm Start Scan",
		"Start this scan? Yellow light will turn on. After pause, you can resume or use Back To Start to reset the scan.",
		QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
	if (ret != QMessageBox::Yes)
		return;

	m_scanService->startScan(isContinuous, m_startPoint, m_endPoint,
		stepGap, totalLines, m_axisParams);
}

void MotionController::on_StopInspectBtn_clicked()
{
	if (!m_adapter->isConnected())
	{
		QMessageBox::warning(this, "Warning", "Connect controller first.");
		return;
	}

	if (m_scanResetPending)
	{
		auto ret = QMessageBox::question(this, "Confirm Reset Scan",
			"Move back to the saved start point and return scan to standby?",
			QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
		if (ret != QMessageBox::Yes)
			return;

		int axes[4] = { Axis::X_AXIS, Axis::Y_MASTER, Axis::Z_FRONT_AXIS, Axis::Z_BACK_AXIS };
		for (int i = 0; i < 4; i++)
			m_adapter->moveAbsolute(axes[i], m_startPoint[i]);

		m_scanResetPending = false;
		m_scanLedOverride = LED_ALARM;
		ui.StartInspectBtn->setText(QStringLiteral("开始检测"));
		ui.StopInspectBtn->setText(QStringLiteral("停止检测"));
		ui.LeftTime->setText("");
		qInfo() << "[SCAN] reset requested: move back to start point and enter standby";
		return;
	}

	if (!m_scanService->hasActiveScan())
	{
		QMessageBox::information(this, "Info", "No active scan to stop.");
		return;
	}

	auto ret = QMessageBox::warning(this, "Confirm Stop Scan",
		"Stop the current scan? The stop button will become Reset.",
		QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
	if (ret != QMessageBox::Yes)
		return;

	m_scanResetPending = true;
	m_scanService->stopScan();
	ui.StartInspectBtn->setText(QStringLiteral("开始检测"));
	ui.StopInspectBtn->setText(QStringLiteral("重置"));
	ui.LeftTime->setText("");
	qWarning() << "[SCAN] stop requested: reset is pending";
}

void MotionController::onScanStarted()
{
	m_scanLedOverride = LED_OFF;
	m_scanResetPending = false;
	qInfo() << "[SCAN] started: yellow light on";
	ui.StartInspectBtn->setText(QStringLiteral("暂停检测"));
	ui.StopInspectBtn->setText(QStringLiteral("停止检测"));
}

void MotionController::onScanPaused()
{
	m_scanLedOverride = LED_ALARM;
	qWarning() << "[SCAN] paused: red light on";
	ui.StartInspectBtn->setText(QStringLiteral("恢复检测"));
	ui.StopInspectBtn->setText(QStringLiteral("停止检测"));
}

void MotionController::onScanResumed()
{
	m_scanLedOverride = LED_OFF;
	qInfo() << "[SCAN] resumed: yellow light on";
	ui.StartInspectBtn->setText(QStringLiteral("暂停检测"));
	ui.StopInspectBtn->setText(QStringLiteral("停止检测"));
}

void MotionController::onScanStopped()
{
	m_scanLedOverride = LED_ALARM;
	qWarning() << "[SCAN] stopped: red light on";
	ui.StartInspectBtn->setText(QStringLiteral("开始检测"));
	ui.StopInspectBtn->setText(m_scanResetPending ? QStringLiteral("重置") : QStringLiteral("停止检测"));
	ui.LeftTime->setText("");
}

void MotionController::onScanCompleted()
{
	m_scanLedOverride = LED_NORMAL;
	qInfo() << "[SCAN] completed: green light on, buzzer pulse"
		<< "buzzerOp=" << m_buzzerOp
		<< "durationMs=" << m_buzzerDurationMs;

	if (m_adapter->isConnected() && m_buzzerOp >= 0 && m_buzzerDurationMs > 0)
	{
		m_adapter->setOutput(m_buzzerOp, true);
		QTimer::singleShot(m_buzzerDurationMs, this, [this]() {
			if (m_adapter && m_adapter->isConnected())
				m_adapter->setOutput(m_buzzerOp, false);
		});
	}

	ui.StartInspectBtn->setText(QStringLiteral("开始检测"));
	ui.StopInspectBtn->setText(QStringLiteral("停止检测"));
	m_scanResetPending = false;
	ui.LeftTime->setText(QStringLiteral("检测完成"));
}

// ============================================================================
// Scan point controls.
// ============================================================================

void MotionController::on_StartPointBtn_clicked()
{
	if (!m_adapter->isConnected())
	{
		QMessageBox::warning(this, "Warning", "Connect controller first.");
		return;
	}
	auto ret = QMessageBox::warning(this, "Confirm Start Point", "Save current position as start point?", QMessageBox::StandardButton::Ok | QMessageBox::StandardButton::No, QMessageBox::StandardButton::No);
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
		QMessageBox::warning(this, "Warning", "Connect controller first.");
		return;
	}
	auto ret = QMessageBox::warning(this, "Confirm End Point", "Save current position as end point?", QMessageBox::StandardButton::Ok | QMessageBox::StandardButton::No, QMessageBox::StandardButton::No);
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
		QMessageBox::warning(this, "Warning", "Connect controller first.");
		return;
	}

	if (m_scanService->isPaused())
	{
		auto ret = QMessageBox::warning(this, "Confirm Back To Start",
			"Scan is paused. Moving back to start will stop and reset this scan. Continue?",
			QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
		if (ret != QMessageBox::Yes)
			return;

		m_scanService->stopScan();
		m_scanLedOverride = LED_ALARM;
	}
	else
	{
		auto ret = QMessageBox::question(this, "Confirm Back To Start",
			"Move to the saved start point?",
			QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
		if (ret != QMessageBox::Yes)
			return;
	}

	int axes[4] = { Axis::X_AXIS, Axis::Y_MASTER, Axis::Z_FRONT_AXIS, Axis::Z_BACK_AXIS };
	for (int i = 0; i < 4; i++)
		m_adapter->moveAbsolute(axes[i], m_startPoint[i]);

	qInfo() << "[SCAN] move back to start point";
}
void MotionController::on_BackEndBtn_clicked()
{
	if (!m_adapter->isConnected())
	{
		QMessageBox::warning(this, "Warning", "Connect controller first.");
		return;
	}
	auto ret = QMessageBox::question(this, "Confirm Back To End",
		"Move to the saved end point?",
		QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
	if (ret != QMessageBox::Yes)
		return;

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
// Axis parameter dialogs.
// ============================================================================

void MotionController::on_XParaBtn_clicked()
{
	if (!m_adapter->isConnected()) { QMessageBox::warning(this, "Warning", "Connect controller first."); return; }
	ParaSettings* dlg = new ParaSettings(Axis::X_AXIS, "X Axis", nullptr);
	dlg->setAttribute(Qt::WA_DeleteOnClose);
	connect(dlg, &ParaSettings::parametersChanged, this, &MotionController::applyAxisParameters);
	dlg->show();
}

void MotionController::on_YParaBtn_clicked()
{
	if (!m_adapter->isConnected()) { QMessageBox::warning(this, "Warning", "Connect controller first."); return; }
	ParaSettings* dlg = new ParaSettings(Axis::Y_MASTER, "Y Axis", nullptr);
	dlg->setAttribute(Qt::WA_DeleteOnClose);
	connect(dlg, &ParaSettings::parametersChanged, this, &MotionController::applyAxisParameters);
	dlg->show();
}

void MotionController::on_ZQParaBtn_clicked()
{
	if (!m_adapter->isConnected()) { QMessageBox::warning(this, "Warning", "Connect controller first."); return; }
	ParaSettings* dlg = new ParaSettings(Axis::Z_FRONT_AXIS, "Z Front Axis", nullptr);
	dlg->setAttribute(Qt::WA_DeleteOnClose);
	connect(dlg, &ParaSettings::parametersChanged, this, &MotionController::applyAxisParameters);
	dlg->show();
}

void MotionController::on_ZHParaBtn_clicked()
{
	if (!m_adapter->isConnected()) { QMessageBox::warning(this, "Warning", "Connect controller first."); return; }
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
// Manual axis commands delegated to AxisService.
// ============================================================================

// ---- Clear axis positions ----
void MotionController::on_XClearBtn_clicked()
{
	if (!m_adapter->isConnected()) { QMessageBox::warning(this, "Warning", "Connect controller first."); return; }
	m_axisService->clearPosition(Axis::X_AXIS);
}
void MotionController::on_YClearBtn_clicked()
{
	if (!m_adapter->isConnected()) { QMessageBox::warning(this, "Warning", "Connect controller first."); return; }
	m_axisService->clearPosition(Axis::Y_MASTER);
}
void MotionController::on_ZQClearBtn_clicked()
{
	if (!m_adapter->isConnected()) { QMessageBox::warning(this, "Warning", "Connect controller first."); return; }
	m_axisService->clearPosition(Axis::Z_FRONT_AXIS);
}
void MotionController::on_ZHClearBtn_clicked()
{
	if (!m_adapter->isConnected()) { QMessageBox::warning(this, "Warning", "Connect controller first."); return; }
	m_axisService->clearPosition(Axis::Z_BACK_AXIS);
}

// ---- Move axes in positive direction ----
void MotionController::on_XNormalBtn_clicked()
{
	if (!m_adapter->isConnected()) { QMessageBox::warning(this, "Warning", "Connect controller first."); return; }
	m_axisService->movePositive(Axis::X_AXIS, ui.checkBox_6->isChecked(), ui.XdoubleSpinBox->value());
}
void MotionController::on_YNormalBtn_clicked()
{
	if (!m_adapter->isConnected()) { QMessageBox::warning(this, "Warning", "Connect controller first."); return; }
	m_axisService->movePositive(Axis::Y_MASTER, ui.checkBox_7->isChecked(), ui.YdoubleSpinBox->value());
}
void MotionController::on_ZQNormalBtn_clicked()
{
	if (!m_adapter->isConnected()) { QMessageBox::warning(this, "Warning", "Connect controller first."); return; }
	m_axisService->movePositive(Axis::Z_FRONT_AXIS, ui.checkBox_8->isChecked(), ui.ZQdoubleSpinBox->value());
}
void MotionController::on_ZHNormalBtn_clicked()
{
	if (!m_adapter->isConnected()) { QMessageBox::warning(this, "Warning", "Connect controller first."); return; }
	m_axisService->movePositive(Axis::Z_BACK_AXIS, ui.checkBox_10->isChecked(), ui.ZHdoubleSpinBox->value());
}

// ---- Move axes in negative direction ----
void MotionController::on_XReverseBtn_clicked()
{
	if (!m_adapter->isConnected()) { QMessageBox::warning(this, "Warning", "Connect controller first."); return; }
	m_axisService->moveNegative(Axis::X_AXIS, ui.checkBox_6->isChecked(), ui.XdoubleSpinBox->value());
}
void MotionController::on_YReverseBtn_clicked()
{
	if (!m_adapter->isConnected()) { QMessageBox::warning(this, "Warning", "Connect controller first."); return; }
	m_axisService->moveNegative(Axis::Y_MASTER, ui.checkBox_7->isChecked(), ui.YdoubleSpinBox->value());
}
void MotionController::on_ZQReverseBtn_clicked()
{
	if (!m_adapter->isConnected()) { QMessageBox::warning(this, "Warning", "Connect controller first."); return; }
	m_axisService->moveNegative(Axis::Z_FRONT_AXIS, ui.checkBox_8->isChecked(), ui.ZQdoubleSpinBox->value());
}
void MotionController::on_ZHReverseBtn_clicked()
{
	if (!m_adapter->isConnected()) { QMessageBox::warning(this, "Warning", "Connect controller first."); return; }
	m_axisService->moveNegative(Axis::Z_BACK_AXIS, ui.checkBox_10->isChecked(), ui.ZHdoubleSpinBox->value());
}

// ---- Stop axes ----
void MotionController::on_XStopBtn_clicked()
{
	if (!m_adapter->isConnected()) { QMessageBox::warning(this, "Warning", "Connect controller first."); return; }
	m_axisService->stop(Axis::X_AXIS);
}
void MotionController::on_YStopBtn_clicked()
{
	if (!m_adapter->isConnected()) { QMessageBox::warning(this, "Warning", "Connect controller first."); return; }
	m_axisService->stop(Axis::Y_MASTER);
}
void MotionController::on_ZQStopBtn_clicked()
{
	if (!m_adapter->isConnected()) { QMessageBox::warning(this, "Warning", "Connect controller first."); return; }
	m_axisService->stop(Axis::Z_FRONT_AXIS);
}
void MotionController::on_ZHStopBtn_clicked()
{
	if (!m_adapter->isConnected()) { QMessageBox::warning(this, "Warning", "Connect controller first."); return; }
	m_axisService->stop(Axis::Z_BACK_AXIS);
}

// ============================================================================
// 100ms update loop: refresh axis status, drive services, and update LEDs.
// ============================================================================

void MotionController::updateAllAxisParameters()
{
	bool isConnected = m_adapter->isConnected();

	// 1. Refresh connection and axis status UI.
	for (int i = 0; i < 4; ++i)
	{
		const AxisConfig& cfg = m_axisList[i];

		if (!isConnected)
		{
			cfg.stateLineEdit->setText(QStringLiteral("未连接"));
			cfg.locateLineEdit->setText("0.00");
			cfg.speedLineEdit->setText("0.00");
			continue;
		}

		AxisService::AxisStatus s = m_axisService->getStatus(cfg.axisNo, m_axisParams[cfg.axisNo]);
		cfg.stateLineEdit->setText(s.stateText);
		cfg.locateLineEdit->setText(QString::number(s.position, 'f', 2));
		cfg.speedLineEdit->setText(QString::number(s.speed, 'f', 2));
	}

	// 2. Drive HomeService state machine.
	m_homeService->tick();

	// 3. Drive ScanService state machine and remaining time display.
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
                ui.LeftTime->setText(QString("%1:%2:%3")
					.arg(hours)
					.arg(mins, 2, 10, QChar('0'))
					.arg(secs, 2, 10, QChar('0')));
			else
                ui.LeftTime->setText(QString("%1:%2")
					.arg(mins, 2, 10, QChar('0'))
					.arg(secs, 2, 10, QChar('0')));
		}
	}

	// 4. Update LEDs from axis and workflow state.
	LedState newLed = LED_OFF;

	if (isConnected)
	{
		// Check whether any axis is in alarm.
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

		// Priority: alarm > homing/scanning > scan override > normal.
		if (hasAlarm)
		{
			newLed = LED_ALARM;
		}
		else if (m_homeService->isHoming())
		{
			newLed = LED_HOME;
		}
		else if (m_scanService->isRunning())
		{
			newLed = LED_HOME;
		}
		else if (m_scanLedOverride != LED_OFF)
		{
			newLed = m_scanLedOverride;
		}
		else
		{
			newLed = LED_NORMAL;
		}

		// Diagnostic log while homing.
		if (m_homeService->isHoming())
			qDebug() << "[LED-DIAG] isHoming=true hasAlarm=" << hasAlarm
				<< "-> newLed=" << (hasAlarm ? "ALARM" : "HOME");
	}

	// Write output ports only when the LED state changes.
	if (newLed != m_lastLedState)
	{
		const char* ledNames[] = { "OFF", "ALARM", "HOME", "NORMAL" };
		qDebug() << "LED state change:" << ledNames[m_lastLedState] << "->" << ledNames[newLed]
			<< "| redOp" << m_alarmLedOp << "=" << (newLed == LED_ALARM)
			<< "yellowOp" << m_homeLedOp << "=" << (newLed == LED_HOME)
			<< "greenOp" << m_normalLedOp << "=" << (newLed == LED_NORMAL);
		m_adapter->setOutput(m_alarmLedOp, newLed == LED_ALARM);
		m_adapter->setOutput(m_homeLedOp, newLed == LED_HOME);
		m_adapter->setOutput(m_normalLedOp, newLed == LED_NORMAL);
		m_lastLedState = newLed;
	}
}

// ============================================================================
// Config persistence.
// ============================================================================

void MotionController::ensureConfigFileExists()
{
	const QString path = configFilePath();
	if (QFileInfo::exists(path))
		return;

	QFile file(path);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
		return;

	QTextStream out(&file);
	out << "[Home]\n";
	out << "Mode=4\n";
	out << "FwdIn0=1\n";
	out << "RevIn0=0\n";
	out << "DatumIn0=2\n";
	out << "FwdIn1=4\n";
	out << "RevIn1=3\n";
	out << "DatumIn1=5\n";
	out << "FwdIn3=7\n";
	out << "RevIn3=6\n";
	out << "DatumIn3=8\n";
	out << "FwdIn4=10\n";
	out << "RevIn4=9\n";
	out << "DatumIn4=11\n";
	out << "\n";
	out << "[Axis0]\n";
	out << "units=1000\n";
	out << "lspeed=0\n";
	out << "speed=20\n";
	out << "dec=2000\n";
	out << "sramp=10\n";
	out << "dir=0\n";
	out << "acc=2000\n";
	out << "\n";
	out << "[Scan]\n";
	out << "Step=10\n";
	out << "\n";
	out << "[Led]\n";
	out << "RedOp=0\n";
	out << "YellowOp=1\n";
	out << "GreenOp=2\n";
	out << "BuzzerOp=3\n";
	out << "BuzzerDurationMs=1000\n";
	out << "\n";
	out << "[Axis1]\n";
	out << "units=1000\n";
	out << "lspeed=0\n";
	out << "speed=20\n";
	out << "dec=2000\n";
	out << "sramp=10\n";
	out << "dir=0\n";
	out << "acc=2000\n";
	out << "\n";
	out << "[Axis3]\n";
	out << "units=1000\n";
	out << "lspeed=0\n";
	out << "speed=20\n";
	out << "dec=2000\n";
	out << "sramp=10\n";
	out << "dir=0\n";
	out << "acc=2000\n";
	out << "\n";
	out << "[Axis4]\n";
	out << "units=1000\n";
	out << "lspeed=0\n";
	out << "speed=20\n";
	out << "dec=2000\n";
	out << "sramp=10\n";
	out << "dir=0\n";
	out << "acc=2000\n";
	out << "\n";
	out << "[Connection]\n";
	out << "IP=192.168.0.11\n";
	qInfo() << "[CONFIG] default config.ini created:" << path;
}

void MotionController::loadConfig()
{
	QSettings settings(configFilePath(), QSettings::IniFormat);

	QString lastIP = settings.value("Connection/IP", "192.168.0.11").toString();
	ui.IPComboBox->setCurrentText(lastIP);

	double step = settings.value("Scan/Step", 10.0).toDouble();
	ui.StepDoubleSpinBox->setValue(step);

	m_alarmLedOp = settings.value("Led/RedOp", settings.value("Led/AlarmOp", 0)).toInt();
	m_homeLedOp = settings.value("Led/YellowOp", settings.value("Led/HomeOp", 1)).toInt();
	m_normalLedOp = settings.value("Led/GreenOp", settings.value("Led/NormalOp", 2)).toInt();
	m_buzzerOp = settings.value("Led/BuzzerOp", 3).toInt();
	m_buzzerDurationMs = settings.value("Led/BuzzerDurationMs", 1000).toInt();
	settings.setValue("Led/RedOp", m_alarmLedOp);
	settings.setValue("Led/YellowOp", m_homeLedOp);
	settings.setValue("Led/GreenOp", m_normalLedOp);
	settings.setValue("Led/BuzzerOp", m_buzzerOp);
	settings.setValue("Led/BuzzerDurationMs", m_buzzerDurationMs);
	settings.sync();
	qInfo() << "[LED] config"
		<< "redOp=" << m_alarmLedOp
		<< "yellowOp=" << m_homeLedOp
		<< "greenOp=" << m_normalLedOp
		<< "buzzerOp=" << m_buzzerOp
		<< "buzzerDurationMs=" << m_buzzerDurationMs;

	// Preload cached axis parameters from config.
	m_axisService->applyParametersFromConfig(m_axisParams);
}

void MotionController::saveConfig()
{
	QSettings settings(configFilePath(), QSettings::IniFormat);
	settings.setValue("Connection/IP", ui.IPComboBox->currentText());
	settings.setValue("Scan/Step", ui.StepDoubleSpinBox->value());
	settings.setValue("Led/RedOp", m_alarmLedOp);
	settings.setValue("Led/YellowOp", m_homeLedOp);
	settings.setValue("Led/GreenOp", m_normalLedOp);
	settings.setValue("Led/BuzzerOp", m_buzzerOp);
	settings.setValue("Led/BuzzerDurationMs", m_buzzerDurationMs);
}

// ============================================================================
// Keyboard shortcuts, handled through eventFilter only.
// ============================================================================

bool MotionController::eventFilter(QObject* watched, QEvent* event)
{
	// Only handle events from widgets in this window.
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

	// Consume motion keys, and ignore autorepeat or disconnected execution.
	if (!m_adapter->isConnected() || event->isAutoRepeat())
		return true;

	// Keyboard jog mode uses the step distance from the UI.
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
