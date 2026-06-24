#include "ParaSettings.h"

ParaSettings::ParaSettings(const QString& title, QWidget* parent)
    : QWidget(parent)
{
    ui.setupUi(this);
    setWindowTitle(title + "参数设置");
    updateParameters();
}

ParaSettings::~ParaSettings()
{}

void ParaSettings::updateParameters()
{
    ui.UnitsLineEdit->setText(QString::number(m_units));
    ui.LowSpeedLineEdit->setText(QString::number(m_lspeed));
    ui.SpeedLineEdit->setText(QString::number(m_speed));
    ui.AccLineEdit->setText(QString::number(m_acc));
    ui.DecLineEdit->setText(QString::number(m_dec));
    ui.SrampLineEdit->setText(QString::number(m_sramp));
}

