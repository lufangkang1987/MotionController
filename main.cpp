#include "MotionController.h"
#include <QtWidgets/QApplication>
#include <cstdlib>
#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QMessageLogContext>
#include <QMutex>
#include <QMutexLocker>
#include <QTextStream>

static QFile g_logFile;
static QMutex g_logMutex;

static const char* logLevelName(QtMsgType type)
{
    switch (type)
    {
    case QtDebugMsg:    return "DEBUG";
    case QtInfoMsg:     return "INFO";
    case QtWarningMsg:  return "WARN";
    case QtCriticalMsg: return "ERROR";
    case QtFatalMsg:    return "FATAL";
    }
    return "UNKNOWN";
}

static void messageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg)
{
    QMutexLocker locker(&g_logMutex);

    QTextStream out(&g_logFile);
    out << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz")
        << " [" << logLevelName(type) << "] "
        << msg;

    if (context.file)
        out << " (" << context.file << ":" << context.line << ")";

    out << "\n";
    out.flush();

    if (type == QtFatalMsg)
        abort();
}

static void initFileLogger()
{
    QDir logDir(QCoreApplication::applicationDirPath() + "/logs");
    if (!logDir.exists())
        logDir.mkpath(".");

    const QString fileName = QDate::currentDate().toString("yyyyMMdd") + ".log";
    g_logFile.setFileName(logDir.filePath(fileName));
    if (!g_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        return;

    qInstallMessageHandler(messageHandler);
    qInfo() << "Application started. Log file:" << g_logFile.fileName();
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    initFileLogger();
    app.setWindowIcon(QIcon(":/resources/logo.ico"));
    MotionController window;
    window.show();
    return app.exec();
}
