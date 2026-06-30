#pragma once

#include <QWidget>
#include <QSettings>
#include "ui_ParaSettings.h"

struct AxisParams
{
    float m_units = 1000.0f;
    float m_lspeed = 0.0f;
    float m_speed = 20.0f;
    float m_acc = 2000.0f;
    float m_dec = 2000.0f;
    float m_sramp = 10.0f;
    int dir = 0; // 0: normal, 1: reverse
};

// config.ini 文件路径（exe 同目录）
QString configFilePath();

class ParaSettings : public QWidget
{
    Q_OBJECT

public:
    ParaSettings(int axis, const QString& title, QWidget* parent = nullptr);
    ~ParaSettings();

signals:
    void parametersChanged(int Axis, const AxisParams& params);

private slots:
    void on_pushButton_clicked(); // OK button
    void on_pushButton_2_clicked(); // Cancel button

private:
    Ui::ParaSettingsClass ui;
    int m_axis;
    AxisParams m_params;
    void updateParameters();
    void readFromUI();
    void loadFromConfig();
    void saveToConfig();
};

