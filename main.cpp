#include "MotionController.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    MotionController window;
    window.show();
    return app.exec();
}
