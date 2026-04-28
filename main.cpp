#include "mainwindow.h"
#include <QApplication>
#include <QCoreApplication>
#include<QSettings>
#include <QFontDatabase>
#include <QString>
#include"Globals.h"
#include<QDir>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QStandardPaths>

static QFile g_logFile;

static void messageHandler(QtMsgType type, const QMessageLogContext &, const QString &msg)
{
    if (!g_logFile.isOpen()) return;
    QTextStream out(&g_logFile);
    out << QDateTime::currentDateTime().toString("hh:mm:ss.zzz") << " ";
    switch (type) {
        case QtDebugMsg:    out << "[D] "; break;
        case QtWarningMsg:  out << "[W] "; break;
        case QtCriticalMsg: out << "[C] "; break;
        case QtFatalMsg:    out << "[F] "; break;
        default:            out << "[I] "; break;
    }
    out << msg << "\n";
    out.flush();
}

namespace {
double g_orbMax = 8.0; // Default orb value
}

// Add a getter/setter function
double getOrbMax() {
    return g_orbMax;
}

void setOrbMax(double value) {
    g_orbMax = value;
}

QString g_astroFontFamily;


int main(int argc, char *argv[])
{

    QApplication a(argc, argv);

    // Log to Documents/Asteria/debug.log
    QString logPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                      + "/Asteria/debug.log";
    QDir().mkpath(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/Asteria");
    g_logFile.setFileName(logPath);
    g_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
    qInstallMessageHandler(messageHandler);

    // Load custom astronomical font
    int fontId = QFontDatabase::addApplicationFont(":/resources/AstromoonySans.ttf");
    if (fontId == -1) {
        qWarning() << "Failed to load Astromoony font";
    } else {
        g_astroFontFamily = QFontDatabase::applicationFontFamilies(fontId).at(0);
    }

    //QCoreApplication::setOrganizationName("Alamahant");

#ifdef FLATHUB_BUILD
    QCoreApplication::setOrganizationName("");

#else
    QCoreApplication::setOrganizationName("Alamahant");

#endif

    QCoreApplication::setApplicationName("Asteria");
    QDir().mkpath(AsteriaGlobals::appDir);
    QCoreApplication::setApplicationVersion("2.1.5");


    MainWindow w;
    w.show();
    return a.exec();
}
