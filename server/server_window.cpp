#include "server_window.h"
#include "wake_on_lan.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QDateTime>
#include <QNetworkInterface>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QSplitter>
#include <QHeaderView>
#include <QScrollBar>
#include <QScrollArea>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QTextStream>
#include <QProcess>
#include <QApplication>
#include <QTimer>
#include <QSysInfo>
#include <QLabel>
#include <QSpinBox>
#include <QPushButton>
#include <QListWidget>
#include <QLineEdit>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextEdit>
#include <QShowEvent>
#include <QTextBlock>
#include <QSettings>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

// 平台检测方法实现 - 只在类外部实现一次
QString ServerWindow::getSystemArchitecture() {
    return QSysInfo::currentCpuArchitecture();
}

bool ServerWindow::isX86Architecture() {
    QString arch = getSystemArchitecture().toLower();
    return arch.contains("x86") || arch.contains("amd64") ||
           arch.contains("i386") || arch.contains("i686");
}

bool ServerWindow::isFTArchitecture() {
    QString arch = getSystemArchitecture().toLower();
    return arch.contains("aarch64") || arch.contains("arm64");
}

QString ServerWindow::getPlatformName() {
    if (isX86Architecture()) {
#ifdef Q_OS_WIN
        // Windows平台：通过注册表读取CPU厂商信息
        QString vendorId;
        QSettings reg("HKEY_LOCAL_MACHINE\\HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                       QSettings::NativeFormat);
        vendorId = reg.value("VendorIdentifier").toString().toLower();

        if (vendorId.contains("intel")) {
            return "Intel平台";
        } else if (vendorId.contains("amd")) {
            return "AMD平台";
        } else if (vendorId.contains("zhaoxin") || vendorId.contains("via") || vendorId.contains("centaur")) {
            return "兆芯平台";
        } else if (vendorId.contains("hygon")) {
            return "海光平台";
        }
        return "X86平台";
#else
        // Linux平台：读取 /proc/cpuinfo
        QProcess process;
        process.start("sh", QStringList() << "-c" << "cat /proc/cpuinfo | grep -i 'vendor_id' | head -1");
        if (process.waitForFinished(1000)) {
            QString output = process.readAllStandardOutput().toLower();
            if (output.contains("intel")) {
                return "Intel平台";
            } else if (output.contains("amd")) {
                return "AMD平台";
            } else if (output.contains("zhaoxin") || output.contains("via") || output.contains("centaur")) {
                return "兆芯平台";
            } else if (output.contains("hygon")) {
                return "海光平台";
            }
        }
        return "X86平台";
#endif
    } else if (isFTArchitecture()) {
        return "飞腾平台";
    } else {
        return getSystemArchitecture() + "平台";
    }
}

// 修正构造函数初始化顺序
ServerWindow::ServerWindow(QWidget *parent)
    : QMainWindow(parent),
      tcpServer(nullptr),
      serverPort(8888),
      isRunning(false),
      wakeUpTimer(new QTimer(this))
{
    setupUI();
    setupWindowProperties();

    connect(wakeUpTimer, SIGNAL(timeout()), this, SLOT(onWakeUpTimer()));

    // 加载持久化的唤醒配置
    loadWakeConfigs();
    updateWakeConfigTable();

    // 记录平台信息
    logMessage(QString("服务端启动 - 平台: %1, 架构: %2").arg(getPlatformName()).arg(getSystemArchitecture()));
}

// 修改 setupUI 方法，动态设置窗口标题
void ServerWindow::setupUI() {
    setWindowTitle(QString("研盛测试服务端V1.4- %1").arg(getPlatformName()));
    setMinimumSize(1100, 700);

    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setSpacing(8);

    // ========== 服务器状态组 ==========
    QGroupBox *statusGroup = new QGroupBox("服务器状态", this);
    QVBoxLayout *statusMainLayout = new QVBoxLayout(statusGroup);
    statusMainLayout->setSpacing(6);

    // 第一行：平台信息 + 端口 + 状态 + 勾选框 + 启动按钮
    QHBoxLayout *statusRow1Layout = new QHBoxLayout();

    QLabel *platformLabel = new QLabel(QString("运行平台: %1").arg(getPlatformName()), this);
    platformLabel->setStyleSheet("QLabel { color: #2c3e50; font-weight: bold; background-color: #e8f4fd; padding: 3px 8px; border-radius: 3px; }");

    statusLabel = new QLabel("服务器未启动", this);
    portSpinBox = new QSpinBox(this);
    portSpinBox->setRange(1000, 65535);
    portSpinBox->setValue(serverPort);
    portSpinBox->setFixedWidth(80);

    startStopButton = new QPushButton("启动服务器", this);

    autoStartCheckBox = new QCheckBox("开机启动", this);
    connect(autoStartCheckBox, SIGNAL(toggled(bool)), this, SLOT(onAutoStartToggled(bool)));

    alwaysOnTopCheckBox = new QCheckBox("窗口置顶", this);
    alwaysOnTopCheckBox->setChecked(true);
    connect(alwaysOnTopCheckBox, SIGNAL(toggled(bool)), this, SLOT(onAlwaysOnTopToggled(bool)));

#ifdef Q_OS_WIN
    {
        QSettings reg("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                       QSettings::NativeFormat);
        if (reg.contains("ELSKY_test_server")) {
            autoStartCheckBox->setChecked(true);
        }
    }
#endif

    connect(startStopButton, SIGNAL(clicked()), this, SLOT(onStartStopServer()));

    statusRow1Layout->addWidget(platformLabel);
    statusRow1Layout->addSpacing(10);
    statusRow1Layout->addWidget(new QLabel("端口:", this));
    statusRow1Layout->addWidget(portSpinBox);
    statusRow1Layout->addWidget(statusLabel);
    statusRow1Layout->addStretch();
    statusRow1Layout->addWidget(autoStartCheckBox);
    statusRow1Layout->addSpacing(15);
    statusRow1Layout->addWidget(alwaysOnTopCheckBox);
    statusRow1Layout->addSpacing(15);
    statusRow1Layout->addWidget(startStopButton);

    // 第二行：系统信息
    QHBoxLayout *statusRow2Layout = new QHBoxLayout();
    QLabel *archLabel = new QLabel(QString("系统架构: %1").arg(getSystemArchitecture()), this);
    archLabel->setStyleSheet("QLabel { color: #7f8c8d; font-size: 12px; }");
    QLabel *qtVersionLabel = new QLabel(QString("Qt版本: %1").arg(QT_VERSION_STR), this);
    qtVersionLabel->setStyleSheet("QLabel { color: #7f8c8d; font-size: 12px; }");
    statusRow2Layout->addWidget(archLabel);
    statusRow2Layout->addSpacing(20);
    statusRow2Layout->addWidget(qtVersionLabel);
    statusRow2Layout->addStretch();

    statusMainLayout->addLayout(statusRow1Layout);
    statusMainLayout->addLayout(statusRow2Layout);

    // ========== 手动添加唤醒配置 ==========
    QGroupBox *manualWakeGroup = new QGroupBox("手动添加唤醒配置", this);
    QHBoxLayout *macInputLayout = new QHBoxLayout(manualWakeGroup);

    manualMacEdit = new QLineEdit(this);
    manualMacEdit->setPlaceholderText("输入12位十六进制数字，如001122AABBCC");
    manualMacEdit->setMaxLength(17); // AA:BB:CC:DD:EE:FF = 17字符
    // 只允许输入十六进制字符，自动补冒号
    connect(manualMacEdit, &QLineEdit::textEdited, this, [this](const QString &text) {
        QString cleaned;
        for (const QChar &c : text) {
            if (c.isLetterOrNumber())
                cleaned += c.toUpper();
        }
        // 截取前12位
        cleaned = cleaned.left(12);

        // 每2位插入冒号
        QString formatted;
        for (int i = 0; i < cleaned.length(); ++i) {
            if (i > 0 && i % 2 == 0)
                formatted += ':';
            formatted += cleaned[i];
        }
        manualMacEdit->blockSignals(true);
        manualMacEdit->setText(formatted);
        manualMacEdit->setCursorPosition(formatted.length());
        manualMacEdit->blockSignals(false);
    });

    manualIntervalSpinBox = new QSpinBox(this);
    manualIntervalSpinBox->setRange(1, 86400);
    manualIntervalSpinBox->setValue(60);
    manualIntervalSpinBox->setSuffix(" 秒");
    manualIntervalSpinBox->setFixedWidth(120);

    manualAddButton = new QPushButton("添加唤醒配置", this);
    connect(manualAddButton, SIGNAL(clicked()), this, SLOT(onManualAddWakeConfig()));

    macInputLayout->addWidget(new QLabel("MAC地址:", this));
    macInputLayout->addWidget(manualMacEdit);
    macInputLayout->addSpacing(15);
    macInputLayout->addWidget(new QLabel("唤醒间隔:", this));
    macInputLayout->addWidget(manualIntervalSpinBox);
    macInputLayout->addSpacing(15);
    macInputLayout->addWidget(manualAddButton);
    macInputLayout->addStretch();

    // ========== 分割器：配置表格 + 日志 ==========
    QSplitter *mainSplitter = new QSplitter(Qt::Vertical, this);

    // --- 唤醒配置表格 ---
    QGroupBox *wakeConfigGroup = new QGroupBox("唤醒配置列表", this);
    QVBoxLayout *wakeConfigLayout = new QVBoxLayout(wakeConfigGroup);
    wakeConfigLayout->setContentsMargins(5, 10, 5, 5);

    wakeConfigTable = new QTableWidget(this);
    wakeConfigTable->setColumnCount(6);
    wakeConfigTable->setHorizontalHeaderLabels(
        QStringList() << "客户端ID" << "MAC地址" << "唤醒间隔(秒)"
                       << "最后唤醒时间" << "唤醒操作" << "清除操作");
    wakeConfigTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    wakeConfigTable->setSelectionMode(QAbstractItemView::SingleSelection);
    wakeConfigTable->setAlternatingRowColors(true);

    // 设置列宽和拉伸模式
    wakeConfigTable->setColumnWidth(0, 220);   // 客户端ID
    wakeConfigTable->setColumnWidth(1, 150);   // MAC地址: 完整显示 AA:BB:CC:DD:EE:FF
    wakeConfigTable->setColumnWidth(2, 100);   // 唤醒间隔
    wakeConfigTable->setColumnWidth(3, 190);   // 最后唤醒时间: 完整显示 yyyy-MM-dd hh:mm:ss
    wakeConfigTable->setColumnWidth(4, 90);    // 唤醒操作
    wakeConfigTable->setColumnWidth(5, 70);    // 清除操作

    wakeConfigTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    wakeConfigTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    wakeConfigTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    wakeConfigTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    wakeConfigTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
    wakeConfigTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Fixed);
    wakeConfigTable->horizontalHeader()->setStretchLastSection(false);

    // 确保滚动条正常工作
    wakeConfigTable->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    wakeConfigTable->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    wakeConfigTable->verticalScrollBar()->setSingleStep(30);

    wakeConfigLayout->addWidget(wakeConfigTable);

    // 表格下方按钮
    QHBoxLayout *tableButtonLayout = new QHBoxLayout();
    manualWakeButton = new QPushButton("手动唤醒选中客户端", this);
    clearWakeConfigsButton = new QPushButton("清除所有唤醒配置", this);
    connect(manualWakeButton, SIGNAL(clicked()), this, SLOT(onManualWake()));
    connect(clearWakeConfigsButton, SIGNAL(clicked()), this, SLOT(onClearWakeConfigs()));
    tableButtonLayout->addWidget(manualWakeButton);
    tableButtonLayout->addWidget(clearWakeConfigsButton);
    tableButtonLayout->addStretch();
    wakeConfigLayout->addLayout(tableButtonLayout);

    // --- 系统日志 ---
    QGroupBox *logGroup = new QGroupBox("系统日志", this);
    QVBoxLayout *logLayout = new QVBoxLayout(logGroup);

    logTextEdit = new QTextEdit(this);
    logTextEdit->setReadOnly(true);
    logTextEdit->setMinimumHeight(100);

    QHBoxLayout *logButtonsLayout = new QHBoxLayout();
    clearLogsButton = new QPushButton("清除日志", this);
    connect(clearLogsButton, SIGNAL(clicked()), this, SLOT(onClearLogs()));
    logButtonsLayout->addWidget(clearLogsButton);
    logButtonsLayout->addStretch();

    logLayout->addWidget(logTextEdit);
    logLayout->addLayout(logButtonsLayout);

    // --- 组装分割器 ---
    mainSplitter->addWidget(wakeConfigGroup);
    mainSplitter->addWidget(logGroup);
    mainSplitter->setStretchFactor(0, 3);
    mainSplitter->setStretchFactor(1, 1);
    mainSplitter->setSizes(QList<int>() << 500 << 200);

    // ========== 组装主布局 ==========
    mainLayout->addWidget(statusGroup);
    mainLayout->addWidget(manualWakeGroup);
    mainLayout->addWidget(mainSplitter, 1);

    logMessage(QString("研盛测试服务端已启动 - %1").arg(getPlatformName()));
    logMessage(QString("系统架构: %1, Qt版本: %2").arg(getSystemArchitecture()).arg(QT_VERSION_STR));
    logMessage("服务器就绪，等待客户端连接...");
}

// 添加开机启动相关的函数实现
bool ServerWindow::setServerAutoStart(bool enable) {
#ifdef Q_OS_WIN
    if (enable) {
        return createWindowsAutoStart();
    } else {
        return removeWindowsAutoStart();
    }
#else
    if (enable) {
        return createSystemdService();
    } else {
        return removeSystemdService();
    }
#endif
}

#ifdef Q_OS_WIN
// Windows平台：通过注册表实现开机自启动
bool ServerWindow::createWindowsAutoStart() {
    logMessage("开始设置Windows注册表开机自启动...");

    QString appPath = QCoreApplication::applicationFilePath();
    if (appPath.isEmpty()) {
        logMessage("错误: 无法获取应用程序路径");
        return false;
    }

    // 替换正斜杠为反斜杠，Windows注册表路径需要
    appPath.replace('/', '\\');

    logMessage(QString("应用程序路径: %1").arg(appPath));
    logMessage(QString("目标平台: %1").arg(getPlatformName()));

    // 写入注册表: HKCU\Software\Microsoft\Windows\CurrentVersion\Run
    QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                       QSettings::NativeFormat);
    settings.setValue("ELSKY_test_server", appPath);

    if (settings.status() == QSettings::NoError) {
        logMessage("成功设置Windows开机自启动(注册表)");
        return true;
    } else {
        logMessage("错误: 设置Windows开机自启动失败");
        return false;
    }
}

bool ServerWindow::removeWindowsAutoStart() {
    logMessage("开始移除Windows注册表开机自启动...");

    QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                       QSettings::NativeFormat);
    settings.remove("ELSKY_test_server");

    if (settings.status() == QSettings::NoError) {
        logMessage("成功移除Windows开机自启动(注册表)");
        return true;
    } else {
        logMessage("错误: 移除Windows开机自启动失败");
        return false;
    }
}
#endif

// 修改开机启动相关方法，增加平台兼容性
bool ServerWindow::createSystemdService() {
    logMessage("开始创建用户级systemd服务...");

    // 获取应用程序路径
    QString appPath = QCoreApplication::applicationFilePath();
    if (appPath.isEmpty()) {
        logMessage("错误: 无法获取应用程序路径");
        return false;
    }

    // 获取当前用户主目录
    QString homeDir = QDir::homePath();
    QString userName = qgetenv("USER");
    if (userName.isEmpty()) {
        userName = "it0";
    }

    // 用户级systemd目录
    QString userSystemdDir = homeDir + "/.config/systemd/user";
    QString serviceFilePath = userSystemdDir + "/power-test-server.service";

    logMessage(QString("使用用户级systemd目录: %1").arg(userSystemdDir));
    logMessage(QString("目标平台: %1").arg(getPlatformName()));

    // 确保目录存在
    QDir dir;
    if (!dir.mkpath(userSystemdDir)) {
        logMessage("错误: 无法创建用户systemd目录: " + userSystemdDir);
        return false;
    }

    // 创建服务文件内容 - 根据平台调整延时
    int delaySeconds = 10;
    QString platformSpecificConfig = "";

    if (isX86Architecture()) {
        delaySeconds = 15;
        platformSpecificConfig = "# X86平台优化配置\n";
        logMessage("检测到X86平台，使用优化配置");
    } else if (isFTArchitecture()) {
        delaySeconds = 10;
        platformSpecificConfig = "# 飞腾平台配置\n";
        logMessage("检测到飞腾平台，使用标准配置");
    }

    QString serviceContent = QString(
        "[Unit]\n"
        "Description=Power Test Server\n"
        "After=network.target\n"
        "After=graphical-session.target\n"
        "%4"
        "\n"
        "[Service]\n"
        "Type=simple\n"
        "ExecStartPre=/bin/sleep %1\n"
        "ExecStart=%2\n"
        "WorkingDirectory=%3\n"
        "Environment=DISPLAY=:0\n"
        "Environment=XAUTHORITY=%3/.Xauthority\n"
        "Restart=on-failure\n"
        "RestartSec=5\n"
        "\n"
        "[Install]\n"
        "WantedBy=default.target\n"
    ).arg(delaySeconds).arg(appPath).arg(QDir::homePath()).arg(platformSpecificConfig);

    QFile serviceFile(serviceFilePath);
    if (!serviceFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        logMessage("错误: 无法创建用户级systemd服务文件: " + serviceFilePath);
        logMessage("错误详情: " + serviceFile.errorString());
        return false;
    }

    QTextStream out(&serviceFile);
    out << serviceContent;
    serviceFile.close();

    logMessage("成功创建用户级systemd服务文件: " + serviceFilePath);

    QProcess process;

    process.start("systemctl", QStringList() << "--user" << "daemon-reload");
    if (!process.waitForFinished(5000)) {
        logMessage("警告: systemctl --user daemon-reload 执行超时");
    }

    process.start("systemctl", QStringList() << "--user" << "enable" << "power-test-server.service");
    if (!process.waitForFinished(5000)) {
        logMessage("警告: systemctl --user enable 执行超时");
        return false;
    }

    if (process.exitCode() != 0) {
        logMessage("错误: 启用用户级服务失败，退出码: " + QString::number(process.exitCode()));
        logMessage("错误输出: " + process.readAllStandardError());
        return false;
    }

    logMessage(QString("成功启用用户级systemd服务，启动时将延时%1秒").arg(delaySeconds));
    return true;
}

bool ServerWindow::removeSystemdService() {
    logMessage("开始移除systemd服务...");

    QProcess process;
    QString homeDir = QDir::homePath();
    QString serviceFilePath = homeDir + "/.config/systemd/user/power-test-server.service";

    logMessage("服务文件路径: " + serviceFilePath);

    process.start("systemctl", QStringList() << "--user" << "disable" << "power-test-server.service");
    if (process.waitForFinished(5000)) {
        if (process.exitCode() == 0) {
            logMessage("成功禁用开机启动");
        } else {
            logMessage("禁用开机启动失败，退出码: " + QString::number(process.exitCode()));
            logMessage("错误输出: " + process.readAllStandardError());
        }
    } else {
        logMessage("禁用开机启动执行超时");
    }

    if (QFile::exists(serviceFilePath)) {
        if (QFile::remove(serviceFilePath)) {
            logMessage("成功删除服务文件: " + serviceFilePath);
        } else {
            logMessage("删除服务文件失败，尝试使用rm命令: " + serviceFilePath);
            process.start("rm", QStringList() << "-f" << serviceFilePath);
            process.waitForFinished(3000);
            if (!QFile::exists(serviceFilePath)) {
                logMessage("通过rm命令成功删除服务文件");
            } else {
                logMessage("警告: 服务文件可能仍然存在");
            }
        }
    } else {
        logMessage("服务文件不存在: " + serviceFilePath);
    }

    process.start("systemctl", QStringList() << "--user" << "daemon-reload");
    process.waitForFinished(3000);
    logMessage("重新加载systemd配置");

    process.start("systemctl", QStringList() << "--user" << "reset-failed" << "power-test-server.service");
    process.waitForFinished(3000);

    logMessage("开机启动已取消，当前程序继续运行");

    process.start("systemctl", QStringList() << "--user" << "is-enabled" << "power-test-server.service");
    if (process.waitForFinished(3000)) {
        QString output = process.readAllStandardOutput().trimmed();
        if (output == "enabled") {
            logMessage("警告: 服务仍然处于启用状态");
            return false;
        } else {
            logMessage("确认: 服务已成功禁用，状态: " + output);
            return true;
        }
    }

    return true;
}

QString ServerWindow::getSystemdServicePath() {
    QString homeDir = QDir::homePath();
    return homeDir + "/.config/systemd/user/power-test-server.service";
}

void ServerWindow::onAutoStartToggled(bool checked) {
    if (checked) {
        logMessage("开始设置开机自启动...");
        if (setServerAutoStart(true)) {
            logMessage("开机自启动设置成功");
            saveWakeConfigs();
        } else {
            logMessage("开机自启动设置失败");
            autoStartCheckBox->setChecked(false);
            QMessageBox::warning(this, "错误", "设置开机自启动失败");
        }
    } else {
        int result = QMessageBox::question(this, "确认取消",
                                          "确定要取消开机自启动吗？",
                                          QMessageBox::Yes | QMessageBox::No,
                                          QMessageBox::No);
        if (result == QMessageBox::Yes) {
            logMessage("开始取消开机自启动...");
            if (setServerAutoStart(false)) {
                logMessage("已取消开机自启动");
            } else {
                logMessage("取消开机自启动失败");
                autoStartCheckBox->setChecked(true);
                QMessageBox::warning(this, "错误", "取消开机自启动失败");
            }
        } else {
            autoStartCheckBox->setChecked(true);
        }
    }
}

void ServerWindow::onAlwaysOnTopToggled(bool checked) {
    // 保存当前窗口位置和大小
    QRect geometry = this->geometry();

    if (checked) {
        setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
    } else {
        setWindowFlags(windowFlags() & ~Qt::WindowStaysOnTopHint);
    }
    // 恢复位置和大小，避免窗口大小变化
    setGeometry(geometry);
    show();
    logMessage(QString("窗口置顶已%1").arg(checked ? "开启" : "关闭"));
}

// 以下函数保持不变
void ServerWindow::onStartStopServer() {
    if (!isRunning) {
        startServer();
    } else {
        stopServer();
    }
}

// 修改启动服务器方法，增加平台信息
void ServerWindow::startServer() {
    tcpServer = new QTcpServer(this);
    if (!tcpServer->listen(QHostAddress::Any, portSpinBox->value())) {
        QMessageBox::critical(this, "错误",
            QString("无法启动服务器: %1\n平台: %2").arg(tcpServer->errorString()).arg(getPlatformName()));
        return;
    }

    connect(tcpServer, SIGNAL(newConnection()), this, SLOT(onNewConnection()));

    serverPort = portSpinBox->value();
    isRunning = true;
    statusLabel->setText(QString("服务器运行中 - 端口: %1 - %2").arg(serverPort).arg(getPlatformName()));
    startStopButton->setText("停止服务器");
    portSpinBox->setEnabled(false);

    logMessage(QString("服务器启动成功，监听端口: %1").arg(serverPort));
    logMessage(QString("运行平台: %1, 系统架构: %2").arg(getPlatformName()).arg(getSystemArchitecture()));

    // 启动唤醒定时器（每5秒检查一次）
    wakeUpTimer->start(5000);
}

void ServerWindow::stopServer() {
    if (tcpServer) {
        tcpServer->close();
        tcpServer->deleteLater();
        tcpServer = nullptr;
    }

    if (wakeUpTimer) {
        wakeUpTimer->stop();
    }

    // 断开所有客户端连接
    for (auto it = connectedClients.begin(); it != connectedClients.end(); ++it) {
        it.key()->disconnectFromHost();
        it.key()->deleteLater();
    }
    connectedClients.clear();

    isRunning = false;
    statusLabel->setText("服务器未启动");
    startStopButton->setText("启动服务器");
    portSpinBox->setEnabled(true);

    logMessage("服务器已停止");
}

void ServerWindow::onNewConnection() {
    if (!tcpServer) return;

    QTcpSocket *clientSocket = tcpServer->nextPendingConnection();
    if (!clientSocket) return;

    QString clientInfo = QString("%1:%2").arg(clientSocket->peerAddress().toString()).arg(clientSocket->peerPort());

    connectedClients[clientSocket] = clientInfo;

    connect(clientSocket, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
    connect(clientSocket, SIGNAL(disconnected()), this, SLOT(onClientDisconnected()));

    logMessage("客户端连接: " + clientInfo);
}

void ServerWindow::onReadyRead() {
    QTcpSocket *clientSocket = qobject_cast<QTcpSocket*>(sender());
    if (!clientSocket) return;

    QByteArray data = clientSocket->readAll();
    NetworkMessage msg = NetworkMessage::fromByteArray(data);

    processMessage(msg, clientSocket);
}

void ServerWindow::onClientDisconnected() {
    QTcpSocket *clientSocket = qobject_cast<QTcpSocket*>(sender());
    if (!clientSocket) return;

    QString clientInfo = connectedClients.value(clientSocket);
    connectedClients.remove(clientSocket);

    logMessage("客户端断开连接: " + clientInfo);
    clientSocket->deleteLater();
}

void ServerWindow::processMessage(const NetworkMessage &msg, QTcpSocket *client) {
    QString clientId = connectedClients.value(client);

    switch (msg.type) {
    case MSG_WAKE_ON_LAN_SETUP: {
        QDataStream stream(msg.data);
        WakeOnLanConfig config;
        stream >> config;  // 使用重载的操作符

        // 存储配置并更新最后唤醒时间
        wakeConfigs[clientId] = config;
        lastWakeTime[clientId] = QDateTime::currentDateTime();

        // 更新界面
        updateWakeConfigTable();
        saveWakeConfigs();

        if (config.enabled) {
            logMessage(QString("设置网络唤醒 - 客户端: %1, MAC: %2, 间隔: %3秒")
                      .arg(clientId).arg(config.macAddress).arg(config.intervalSeconds));

            // 立即发送一次唤醒包
            sendWakeOnLanPacket(config.macAddress);
            lastWakeTime[clientId] = QDateTime::currentDateTime();
            logMessage("立即发送初始唤醒包: " + config.macAddress);
        } else {
            logMessage("禁用网络唤醒: " + config.macAddress);
        }
        break;
    }
    case MSG_WAKE_ON_LAN_CANCEL: {
        // 处理取消唤醒消息
        QDataStream stream(msg.data);
        WakeOnLanConfig config;
        stream >> config;

        QString targetMac = config.macAddress;

        // 查找并移除对应的唤醒配置
        bool found = false;
        for (auto it = wakeConfigs.begin(); it != wakeConfigs.end(); ++it) {
            if (it.value().macAddress == targetMac) {
                QString removedClientId = it.key();
                wakeConfigs.erase(it);
                lastWakeTime.remove(removedClientId);
                found = true;

                logMessage(QString("已取消唤醒配置 - 客户端: %1, MAC: %2")
                          .arg(removedClientId).arg(targetMac));
                break;
            }
        }

        if (!found) {
            logMessage(QString("未找到MAC地址 %1 的唤醒配置").arg(targetMac));
        } else {
            // 更新界面
            updateWakeConfigTable();
            saveWakeConfigs();
        }
        break;
    }
    case MSG_SHUTDOWN_REQUEST:
        logMessage("收到关机请求: " + clientId);
        break;
    case MSG_REBOOT_REQUEST:
        logMessage("收到重启请求: " + clientId);
        break;
    default:
        break;
    }
}

void ServerWindow::onWakeUpTimer() {
    QDateTime currentTime = QDateTime::currentDateTime();

    for (auto it = wakeConfigs.begin(); it != wakeConfigs.end(); ++it) {
        const QString &clientId = it.key();
        const WakeOnLanConfig &config = it.value();

        if (config.enabled) {
            QDateTime lastTime = lastWakeTime.value(clientId, QDateTime());

            // 如果从未唤醒过或者已经过了间隔时间，则唤醒
            if (!lastTime.isValid() ||
                lastTime.addSecs(config.intervalSeconds) <= currentTime) {

                sendWakeOnLanPacket(config.macAddress);
                lastWakeTime[clientId] = currentTime;

                logMessage(QString("定时唤醒客户端: %1, MAC: %2, 间隔: %3秒")
                          .arg(clientId).arg(config.macAddress).arg(config.intervalSeconds));

                // 更新表格显示
                updateWakeConfigTable();
            }
        }
    }
}

void ServerWindow::onManualWake() {
    QPushButton *button = qobject_cast<QPushButton*>(sender());
    if (button) {
        // 从按钮属性获取客户端信息
        QString clientId = button->property("clientId").toString();
        QString macAddress = button->property("macAddress").toString();

        if (!clientId.isEmpty() && !macAddress.isEmpty()) {
            sendWakeOnLanPacket(macAddress);
            lastWakeTime[clientId] = QDateTime::currentDateTime();
            logMessage(QString("手动唤醒客户端: %1, MAC: %2").arg(clientId).arg(macAddress));
            updateWakeConfigTable();
            return;
        }
    }

    // 备用方式：通过表格选中行
    int currentRow = wakeConfigTable->currentRow();
    if (currentRow >= 0) {
        QString clientId = wakeConfigTable->item(currentRow, 0)->text();
        QString macAddress = wakeConfigTable->item(currentRow, 1)->text();

        sendWakeOnLanPacket(macAddress);
        lastWakeTime[clientId] = QDateTime::currentDateTime();
        logMessage(QString("手动唤醒客户端: %1, MAC: %2").arg(clientId).arg(macAddress));
        updateWakeConfigTable();
    } else {
        QMessageBox::information(this, "提示", "请先选择要唤醒的客户端");
    }
}

// 手动添加唤醒配置槽函数
void ServerWindow::onManualAddWakeConfig() {
    QString macAddress = manualMacEdit->text().trimmed();
    int intervalSeconds = manualIntervalSpinBox->value();

    if (macAddress.isEmpty()) {
        QMessageBox::warning(this, "输入错误", "请输入MAC地址");
        return;
    }

    // 去除已有冒号，取纯十六进制字符
    QString pureMac = macAddress;
    pureMac.remove(':').remove('-').remove(' ');

    // 验证：必须是12位十六进制字符
    if (pureMac.length() != 12) {
        QMessageBox::warning(this, "输入格式错误",
            "请重新输入\n\n"
            "MAC地址必须是12位十六进制数字\n"
            "例如: 001122AABBCC");
        manualMacEdit->clear();
        return;
    }
    QRegularExpression hexCheck("^[0-9A-Fa-f]{12}$");
    if (!hexCheck.match(pureMac).hasMatch()) {
        QMessageBox::warning(this, "输入格式错误",
            "请重新输入\n\n"
            "MAC地址包含非法字符，只能输入0-9和A-F\n"
            "例如: 001122AABBCC");
        manualMacEdit->clear();
        return;
    }

    // 标准化MAC地址格式 AA:BB:CC:DD:EE:FF
    QString formattedMac;
    for (int i = 0; i < 12; ++i) {
        if (i > 0 && i % 2 == 0) formattedMac += ':';
        formattedMac += pureMac[i].toUpper();
    }

    // 生成手动配置的客户端ID
    QString clientId = "手动配置_" + formattedMac;

    // 检查是否已存在相同MAC的配置
    for (auto it = wakeConfigs.begin(); it != wakeConfigs.end(); ++it) {
        if (it.value().macAddress == formattedMac) {
            QMessageBox::information(this, "提示", "该MAC地址的唤醒配置已存在");
            return;
        }
    }

    // 创建唤醒配置
    WakeOnLanConfig config;
    config.macAddress = formattedMac;
    config.intervalSeconds = intervalSeconds;
    config.enabled = true;

    // 存储配置
    wakeConfigs[clientId] = config;
    lastWakeTime[clientId] = QDateTime::currentDateTime().addSecs(-intervalSeconds);

    // 更新界面
    updateWakeConfigTable();
    saveWakeConfigs();

    // 清空输入框
    manualMacEdit->clear();

    // 记录日志
    logMessage(QString("手动添加唤醒配置 - MAC: %1, 间隔: %2秒").arg(formattedMac).arg(intervalSeconds));

    // 立即发送一次唤醒包
    sendWakeOnLanPacket(formattedMac);
    lastWakeTime[clientId] = QDateTime::currentDateTime();
    logMessage("立即发送初始唤醒包: " + formattedMac);
}

void ServerWindow::onClearSingleConfig() {
    QPushButton *button = qobject_cast<QPushButton*>(sender());
    if (!button) return;

    QString clientId = button->property("clientId").toString();
    QString macAddress = button->property("macAddress").toString();

    if (clientId.isEmpty()) return;

    int result = QMessageBox::question(this, "确认清除",
                                      QString("确定要清除客户端 %1 的唤醒配置吗？").arg(clientId),
                                      QMessageBox::Yes | QMessageBox::No,
                                      QMessageBox::No);

    if (result == QMessageBox::Yes) {
        wakeConfigs.remove(clientId);
        lastWakeTime.remove(clientId);
        updateWakeConfigTable();
        saveWakeConfigs();

        logMessage(QString("已清除客户端唤醒配置: %1, MAC: %2").arg(clientId).arg(macAddress));
        QMessageBox::information(this, "完成", QString("已成功清除客户端 %1 的唤醒配置").arg(clientId));
    }
}

void ServerWindow::onClearWakeConfigs() {
    if (wakeConfigs.isEmpty()) {
        QMessageBox::information(this, "提示", "当前没有唤醒配置可清除");
        return;
    }

    int result = QMessageBox::question(this, "确认清除",
                                      "确定要清除所有客户端的唤醒配置吗？\n此操作不可恢复。",
                                      QMessageBox::Yes | QMessageBox::No,
                                      QMessageBox::No);

    if (result == QMessageBox::Yes) {
        int count = wakeConfigs.size();
        wakeConfigs.clear();
        lastWakeTime.clear();
        updateWakeConfigTable();
        saveWakeConfigs();

        logMessage(QString("已清除所有唤醒配置，共 %1 条记录").arg(count));
        QMessageBox::information(this, "完成", QString("已成功清除 %1 条唤醒配置").arg(count));
    }
}

void ServerWindow::onClearLogs() {
    if (logTextEdit->document()->isEmpty()) {
        QMessageBox::information(this, "提示", "日志已经为空");
        return;
    }

    int result = QMessageBox::question(this, "确认清除",
                                      "确定要清除所有日志吗？\n此操作不可恢复。",
                                      QMessageBox::Yes | QMessageBox::No,
                                      QMessageBox::No);

    if (result == QMessageBox::Yes) {
        logTextEdit->clear();
        logMessage("日志已清除");
    }
}

void ServerWindow::updateWakeConfigTable() {
    // 禁用表格更新，避免频繁重绘导致UI卡顿
    wakeConfigTable->setUpdatesEnabled(false);

    // 保存当前滚动位置
    int currentScrollValue = wakeConfigTable->verticalScrollBar()->value();

    // 先断开所有现有的信号连接，避免重复连接
    wakeConfigTable->clearContents();
    wakeConfigTable->setRowCount(0);

    wakeConfigTable->setRowCount(wakeConfigs.size());

    int row = 0;
    for (auto it = wakeConfigs.begin(); it != wakeConfigs.end(); ++it, ++row) {
        const QString &clientId = it.key();
        const WakeOnLanConfig &config = it.value();

        // 客户端ID
        QTableWidgetItem *clientItem = new QTableWidgetItem(clientId);
        clientItem->setFlags(clientItem->flags() & ~Qt::ItemIsEditable);
        wakeConfigTable->setItem(row, 0, clientItem);

        // MAC地址
        QTableWidgetItem *macItem = new QTableWidgetItem(config.macAddress);
        macItem->setFlags(macItem->flags() & ~Qt::ItemIsEditable);
        wakeConfigTable->setItem(row, 1, macItem);

        // 唤醒间隔
        QTableWidgetItem *intervalItem = new QTableWidgetItem(QString::number(config.intervalSeconds));
        intervalItem->setFlags(intervalItem->flags() & ~Qt::ItemIsEditable);
        wakeConfigTable->setItem(row, 2, intervalItem);

        // 最后唤醒时间
        QDateTime lastTime = lastWakeTime.value(clientId, QDateTime());
        QTableWidgetItem *timeItem = new QTableWidgetItem(
            lastTime.isValid() ? lastTime.toString("yyyy-MM-dd hh:mm:ss") : "从未唤醒");
        timeItem->setFlags(timeItem->flags() & ~Qt::ItemIsEditable);
        wakeConfigTable->setItem(row, 3, timeItem);

        // 唤醒操作按钮
        QPushButton *wakeButton = new QPushButton("立即唤醒", this);
        wakeButton->setProperty("clientId", clientId);
        wakeButton->setProperty("macAddress", config.macAddress);
        wakeButton->setFixedHeight(25); // 固定按钮高度
        connect(wakeButton, SIGNAL(clicked()), this, SLOT(onManualWake()));
        wakeConfigTable->setCellWidget(row, 4, wakeButton);

        // 清除操作按钮
        QPushButton *clearButton = new QPushButton("清除", this);
        clearButton->setProperty("clientId", clientId);
        clearButton->setProperty("macAddress", config.macAddress);
        clearButton->setFixedHeight(25); // 固定按钮高度
        connect(clearButton, SIGNAL(clicked()), this, SLOT(onClearSingleConfig()));
        wakeConfigTable->setCellWidget(row, 5, clearButton);
    }

    // 设置合适的行高
    wakeConfigTable->verticalHeader()->setDefaultSectionSize(30);

    // 恢复滚动位置
    wakeConfigTable->verticalScrollBar()->setValue(currentScrollValue);

    // 更新清除按钮状态
    if (clearWakeConfigsButton) {
        clearWakeConfigsButton->setEnabled(!wakeConfigs.isEmpty());
    }

    // 重新启用表格更新
    wakeConfigTable->setUpdatesEnabled(true);
}

// 修改网络唤醒相关方法，增加平台兼容性日志
void ServerWindow::sendWakeOnLanPacket(const QString &macAddress) {
    WakeOnLan wol;
    if (wol.sendMagicPacket(macAddress)) {
        logMessage(QString("唤醒包发送成功: %1 [%2]").arg(macAddress).arg(getPlatformName()));
    } else {
        logMessage(QString("唤醒包发送失败: %1 [%2]").arg(macAddress).arg(getPlatformName()));
    }
}

void ServerWindow::logMessage(const QString &message) {
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    logTextEdit->append(QString("[%1] %2").arg(timestamp, message));

    // 自动滚动到底部
    QTextCursor cursor = logTextEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    logTextEdit->setTextCursor(cursor);

    // 自动清理日志，避免内存占用过大
    const int MAX_LOG_LINES = 1000;
    int lineCount = logTextEdit->document()->blockCount();
    if (lineCount > MAX_LOG_LINES) {
        QTextCursor cursor(logTextEdit->document()->firstBlock());
        cursor.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor, lineCount - MAX_LOG_LINES);
        cursor.removeSelectedText();
    }
}

// 析构函数实现
ServerWindow::~ServerWindow() {
    stopServer();
}

// 显示事件处理
void ServerWindow::showEvent(QShowEvent *event) {
    QMainWindow::showEvent(event);

    QTimer::singleShot(100, this, [this]() {
        QPoint globalPos = mapToGlobal(QPoint(0, 0));

        if (globalPos.x() < 0 || globalPos.y() < 0 ||
            globalPos.x() > 2000 || globalPos.y() > 2000) {
            move(300, 200);
        }

        // 根据勾选框状态决定是否置顶，避免覆盖用户设置
        if (alwaysOnTopCheckBox && alwaysOnTopCheckBox->isChecked()) {
            setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
            show();
        }
        activateWindow();
        raise();

        logMessage("窗口显示和位置设置完成");
    });
}

// 设置窗口属性
void ServerWindow::setupWindowProperties() {
    // 根据勾选框状态决定是否置顶
    if (alwaysOnTopCheckBox && alwaysOnTopCheckBox->isChecked()) {
        setWindowFlags(Qt::Window | Qt::WindowStaysOnTopHint);
    } else {
        setWindowFlags(Qt::Window);
    }

    setAttribute(Qt::WA_ShowWithoutActivating, false);
    logMessage("窗口属性设置完成");
}

// 配置文件路径
QString ServerWindow::getConfigFilePath() {
    QString dir = QCoreApplication::applicationDirPath();
    return dir + "/wake_configs.ini";
}

// 保存唤醒配置到文件
void ServerWindow::saveWakeConfigs() {
    QSettings settings(getConfigFilePath(), QSettings::IniFormat);

    // 清除旧数据
    settings.remove("WakeConfigs");

    settings.beginWriteArray("WakeConfigs");
    int index = 0;
    for (auto it = wakeConfigs.begin(); it != wakeConfigs.end(); ++it) {
        settings.setArrayIndex(index);
        settings.setValue("clientId", it.key());
        settings.setValue("macAddress", it.value().macAddress);
        settings.setValue("intervalSeconds", it.value().intervalSeconds);
        settings.setValue("enabled", it.value().enabled);
        index++;
    }
    settings.endArray();
    settings.sync();
}

// 从文件加载唤醒配置
void ServerWindow::loadWakeConfigs() {
    QSettings settings(getConfigFilePath(), QSettings::IniFormat);

    int size = settings.beginReadArray("WakeConfigs");
    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        QString clientId = settings.value("clientId").toString();
        if (clientId.isEmpty()) continue;

        WakeOnLanConfig config;
        config.macAddress = settings.value("macAddress").toString();
        config.intervalSeconds = settings.value("intervalSeconds", 60).toInt();
        config.enabled = settings.value("enabled", true).toBool();

        wakeConfigs[clientId] = config;
        // 不恢复 lastWakeTime，让定时器立即触发一次唤醒
        lastWakeTime[clientId] = QDateTime::currentDateTime().addSecs(-config.intervalSeconds);
    }
    settings.endArray();

    if (size > 0) {
        logMessage(QString("已从文件加载 %1 条唤醒配置").arg(size));
    }
}
