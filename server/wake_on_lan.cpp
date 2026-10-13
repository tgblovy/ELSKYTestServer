#include "wake_on_lan.h"
#include <QNetworkInterface>
#include <QRegularExpression>

// 新增平台检测方法
QString WakeOnLan::getSystemArchitecture() {
    return QSysInfo::currentCpuArchitecture();
}

bool WakeOnLan::isX86Architecture() {
    QString arch = getSystemArchitecture().toLower();
    return arch.contains("x86") || arch.contains("amd64") ||
           arch.contains("i386") || arch.contains("i686");
}

QString WakeOnLan::getPlatformInfo() {
    if (isX86Architecture()) {
        return "X86";
    } else {
        return "ARM";
    }
}

WakeOnLan::WakeOnLan() {}

QByteArray WakeOnLan::createMagicPacket(const QString &macAddress) {
    QByteArray packet;

    // 移除MAC地址中的分隔符
    QString cleanMac = macAddress;
    cleanMac.remove(QRegularExpression("[-: ]"));

    if (cleanMac.length() != 12) {
        return QByteArray();
    }

    // 6字节的FF前缀
    for (int i = 0; i < 6; i++) {
        packet.append(0xFF);
    }

    // 重复16次的MAC地址
    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 6; j++) {
            bool ok;
            quint8 byte = cleanMac.mid(j * 2, 2).toUShort(&ok, 16);
            if (ok) {
                packet.append(byte);
            }
        }
    }

    return packet;
}

bool WakeOnLan::sendMagicPacket(const QString &macAddress) {
    // 使用默认广播地址
    return sendMagicPacket(macAddress, "255.255.255.255", 9);
}

bool WakeOnLan::sendMagicPacket(const QString &macAddress, const QString &broadcastAddress, quint16 port) {
    QByteArray packet = createMagicPacket(macAddress);
    if (packet.isEmpty()) {
        return false;
    }

    qint64 result = udpSocket.writeDatagram(packet, QHostAddress(broadcastAddress), port);

    // 可选：记录平台信息用于调试
    // qDebug() << "Wake-on-LAN packet sent from platform:" << getPlatformInfo();

    return (result == packet.size());
}
