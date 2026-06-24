#pragma once

#include <QWidget>
#include "ui_ParaSettings.h"

class ParaSettings : public QWidget
{
    Q_OBJECT

public:
    ParaSettings(const QString& title, QWidget* parent = nullptr);
    ~ParaSettings();

private:
    Ui::ParaSettingsClass ui;
    enum DIR
    {
        NORMAL = 0,
        REVERSE = 1
    };
    int m_units = 1;
    int m_lspeed = 0;
    int m_speed = 100;
    int m_acc = 3000;
    int m_dec = 3000;
    int m_sramp = 10;

private:
    void updateParameters();
};

