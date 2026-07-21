#include "ParaSettings.h"
#include <QCoreApplication>

QString configFilePath()
{
    return QCoreApplication::applicationDirPath() + "/config.ini";
}

ParaSettings::ParaSettings(int axis, const QString& title, QWidget* parent)
    : QWidget(parent), m_axis(axis)
{
    ui.setupUi(this);
    setWindowTitle(title + " Parameters");
    loadFromConfig();   // Load saved parameters from config.ini.
    updateParameters(); // Refresh UI from cached parameters.
}

ParaSettings::~ParaSettings()
{}


void ParaSettings::on_pushButton_clicked()
{
    readFromUI();
    saveToConfig();     // Save parameters to config.ini.
    emit parametersChanged(m_axis, m_params);
    close();
}

void ParaSettings::on_pushButton_2_clicked()
{
    close();
}

void ParaSettings::loadFromConfig()
{
    QSettings settings(configFilePath(), QSettings::IniFormat);
    QString group = QString("Axis%1").arg(m_axis);

    settings.beginGroup(group);
    m_params.m_units  = settings.value("units",  1000.0f).toFloat();
    m_params.m_lspeed = settings.value("lspeed", 0.0f).toFloat();
    m_params.m_speed  = settings.value("speed",  20.0f).toFloat();
    m_params.m_acc    = settings.value("acc",    2000.0f).toFloat();
    m_params.m_dec    = settings.value("dec",    2000.0f).toFloat();
    m_params.m_sramp  = settings.value("sramp",  10.0f).toFloat();
    m_params.dir      = settings.value("dir",    0).toInt();
    settings.endGroup();
}

void ParaSettings::saveToConfig()
{
    QSettings settings(configFilePath(), QSettings::IniFormat);
    QString group = QString("Axis%1").arg(m_axis);

    settings.beginGroup(group);
    settings.setValue("units",  m_params.m_units);
    settings.setValue("lspeed", m_params.m_lspeed);
    settings.setValue("speed",  m_params.m_speed);
    settings.setValue("acc",    m_params.m_acc);
    settings.setValue("dec",    m_params.m_dec);
    settings.setValue("sramp",  m_params.m_sramp);
    settings.setValue("dir",    m_params.dir);
    settings.endGroup();
}

void ParaSettings::updateParameters()
{
    ui.UnitsLineEdit->setText(QString::number(m_params.m_units, 'f', 1));
    ui.LowSpeedLineEdit->setText(QString::number(m_params.m_lspeed, 'f', 1));
    ui.SpeedLineEdit->setText(QString::number(m_params.m_speed, 'f', 1));
    ui.AccLineEdit->setText(QString::number(m_params.m_acc, 'f', 1));
    ui.DecLineEdit->setText(QString::number(m_params.m_dec, 'f', 1));
    ui.SrampLineEdit->setText(QString::number(m_params.m_sramp, 'f', 1));
    ui.DirCheckBox->setChecked(m_params.dir == 1);
}

void ParaSettings::readFromUI()
{
    // Read parameters from the UI and apply basic range protection.
    m_params.m_units  = qMax(0.0001f, ui.UnitsLineEdit->text().toFloat());
    m_params.m_lspeed = qMax(0.0f,    ui.LowSpeedLineEdit->text().toFloat());
    m_params.m_speed  = qMax(0.1f,    ui.SpeedLineEdit->text().toFloat());   // Minimum speed.
    m_params.m_acc    = qMax(1.0f,    ui.AccLineEdit->text().toFloat());
    m_params.m_dec    = qMax(1.0f,    ui.DecLineEdit->text().toFloat());
    m_params.m_sramp  = qMax(0.0f,    ui.SrampLineEdit->text().toFloat());
    m_params.dir      = ui.DirCheckBox->isChecked() ? 1 : 0;
}

