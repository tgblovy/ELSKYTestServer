#include <QApplication>
#include <QSysInfo>
#include "server_window.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 输出平台信息
    qDebug() << "Application starting on platform:" << QSysInfo::prettyProductName();
    qDebug() << "CPU Architecture:" << QSysInfo::currentCpuArchitecture();
    qDebug() << "Build ABI:" << QSysInfo::buildAbi();

    ServerWindow w;
    w.show();

    return a.exec();
}
