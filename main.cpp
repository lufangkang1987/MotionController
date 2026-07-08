#include "MotionController.h"
#include <QtWidgets/QApplication>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/resources/logo.ico"));
    MotionController window;
    window.show();
    return app.exec();
}
