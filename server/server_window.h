#ifndef SERVER_WINDOW_H
#define SERVER_WINDOW_H

#include <QMainWindow>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QMap>
#include <QTableWidget>
#include <QTextEdit>
#include <QSpinBox>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QShowEvent>
#include "protocol.h"

class ServerWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit ServerWindow(QWidget *parent = nullptr);
    ~ServerWindow();

protected:
    void showEvent(QShowEvent *event) override;

private slots:
    void onStartStopServer();
    void onNewConnection();
    void onReadyRead();
    void onClientDisconnected();
    void onWakeUpTimer();
    void onManualWake();
    void onManualAddWakeConfig();
    void onClearSingleConfig();
    void onClearWakeConfigs();
    void onClearLogs();
    void onAutoStartToggled(bool checked);
    void onAlwaysOnTopToggled(bool checked);

private:
    void setupUI();
    void setupWindowProperties();
    void startServer();
    void stopServer();
    void processMessage(const NetworkMessage &msg, QTcpSocket *client);
    void updateWakeConfigTable();
    void sendWakeOnLanPacket(const QString &macAddress);
    void logMessage(const QString &message);
    bool setServerAutoStart(bool enable);
    bool createSystemdService();
    bool removeSystemdService();
    QString getSystemdServicePath();

#ifdef Q_OS_WIN
    bool createWindowsAutoStart();
    bool removeWindowsAutoStart();
#endif

    // 配置持久化
    void saveWakeConfigs();
    void loadWakeConfigs();
    QString getConfigFilePath();

    // 添加平台检测方法
    QString getSystemArchitecture();
    bool isX86Architecture();
    bool isFTArchitecture();
    QString getPlatformName();

    QTcpServer *tcpServer;
    int serverPort;
    bool isRunning;
    QTimer *wakeUpTimer;

    // UI components
    QLabel *statusLabel;
    QSpinBox *portSpinBox;
    QPushButton *startStopButton;
    QCheckBox *autoStartCheckBox;
    QCheckBox *alwaysOnTopCheckBox;
    QTableWidget *wakeConfigTable;
    QTextEdit *logTextEdit;
    QLineEdit *manualMacEdit;
    QSpinBox *manualIntervalSpinBox;
    QPushButton *manualAddButton;
    QPushButton *manualWakeButton;
    QPushButton *clearWakeConfigsButton;
    QPushButton *clearLogsButton;

    // Data storage
    QMap<QTcpSocket*, QString> connectedClients;
    QMap<QString, WakeOnLanConfig> wakeConfigs;
    QMap<QString, QDateTime> lastWakeTime;
};

#endif // SERVER_WINDOW_H
