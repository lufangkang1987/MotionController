#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_MotionController.h"

class MotionController : public QMainWindow
{
    Q_OBJECT

public:
    MotionController(QWidget *parent = nullptr);
    ~MotionController();

private:
    Ui::MotionControllerClass ui;
};

