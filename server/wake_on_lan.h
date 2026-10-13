#ifndef WAKE_ON_LAN_H
#define WAKE_ON_LAN_H

#include <QUdpSocket>
#include <QString>
#include <QSysInfo>

class WakeOnLan
{
public:
    WakeOnLan();
    bool sendMagicPacket(const QString &macAddress);
    bool sendMagicPacket(const QString &macAddress, const QString &broadcastAddress, quint16 port = 9);

    // 新增平台信息方法
    QString getPlatformInfo();

private:
    QByteArray createMagicPacket(const QString &macAddress);
    QString getSystemArchitecture();
    bool isX86Architecture();
    QUdpSocket udpSocket;
};

#endif // WAKE_ON_LAN_H
