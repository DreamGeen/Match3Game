#include "LobbyWidget.h"
#include <QFrame>
#include "DBHelper.h"


LobbyWidget::LobbyWidget(UserSession session, QWidget *parent)
    : QWidget(parent), m_session(session) {

    this->setAttribute(Qt::WA_StyledBackground, true);
    initUI();
    setWindowTitle("Bocchi Clear! - 游戏大厅");
}

void LobbyWidget::initUI() {
    this->setObjectName("lobbyMain");
    this->setStyleSheet("#lobbyMain { border-image: url(:/res/backgrounds/stage_bg.png); }");

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    QFrame *panel = new QFrame(this);
    panel->setObjectName("glassPanel");
    panel->setFixedSize(500, 520);
    panel->setStyleSheet(R"(
        #glassPanel {
            background-color: rgba(20, 20, 30, 210);
            border-radius: 15px;
            border: 1px solid rgba(255, 105, 180, 80);
        }
    )");

    // ==========================================
    // 🌟 核心：为面板装上“栈布局管理器”
    // ==========================================
    m_stackedLayout = new QStackedLayout(panel);

    QString btnStyle = R"(
        QPushButton {
            background-color: rgba(255, 105, 180, 30); border: 2px solid #ff69b4;
            border-radius: 8px; color: #ffffff; font-size: 20px;
            font-weight: bold; padding: 15px; font-family: "Microsoft YaHei";
        }
        QPushButton:hover { background-color: #ff69b4; color: #ffffff; border: 2px solid #ffffff; }
        QPushButton:pressed { background-color: #c71585; }
    )";

    // ==========================================
    // 📖 第 1 页：主菜单 (m_mainMenuWidget)
    // ==========================================
    m_mainMenuWidget = new QWidget(panel);
    auto *mainMenuLayout = new QVBoxLayout(m_mainMenuWidget);
    mainMenuLayout->setSpacing(30);
    mainMenuLayout->setContentsMargins(40, 50, 40, 50);

    QLabel *welcomeLabel = new QLabel(QString("🎸 欢迎回来，%1！").arg(m_session.nickname), m_mainMenuWidget);
    welcomeLabel->setStyleSheet("font-size: 26px; font-weight: 900; color: #ffffff; background: transparent;");
    welcomeLabel->setAlignment(Qt::AlignCenter);

    m_scoreLabel = new QLabel(QString("🏆 当前总积分: %1").arg(m_session.totalScore), m_mainMenuWidget);
    m_scoreLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #FFD700; background: transparent;");
    m_scoreLabel->setAlignment(Qt::AlignCenter);

    QPushButton *singleBtn = new QPushButton("🎸 单人闯关 (Classic)", m_mainMenuWidget);
    QPushButton *aiBtn = new QPushButton("🤖 人机对战 (AI Mode)", m_mainMenuWidget);
    QPushButton *onlineBtn = new QPushButton("🌐 网络竞技 (Online)", m_mainMenuWidget);

    singleBtn->setStyleSheet(btnStyle); singleBtn->setCursor(Qt::PointingHandCursor);
    aiBtn->setStyleSheet(btnStyle);     aiBtn->setCursor(Qt::PointingHandCursor);
    onlineBtn->setStyleSheet(btnStyle); onlineBtn->setCursor(Qt::PointingHandCursor);

    mainMenuLayout->addWidget(welcomeLabel);
    mainMenuLayout->addWidget(m_scoreLabel);
    mainMenuLayout->addSpacing(15);
    mainMenuLayout->addWidget(singleBtn);
    mainMenuLayout->addWidget(aiBtn);
    mainMenuLayout->addWidget(onlineBtn);
    mainMenuLayout->addStretch();

    m_stackedLayout->addWidget(m_mainMenuWidget); // 将第 1 页加入栈

    // ==========================================
    // 📖 第 2 页：难度选择 (m_diffMenuWidget)
    // ==========================================
    m_diffMenuWidget = new QWidget(panel);
    auto *diffLayout = new QVBoxLayout(m_diffMenuWidget);
    diffLayout->setSpacing(20);
    diffLayout->setContentsMargins(40, 50, 40, 40);

    QLabel *diffTitle = new QLabel("🤖 难度选择", m_diffMenuWidget);
    diffTitle->setStyleSheet("font-size: 26px; font-weight: 900; color: #00FFFF; background: transparent;");
    diffTitle->setAlignment(Qt::AlignCenter);

    QPushButton *easyBtn = new QPushButton("🌱 呆萌 (反应慢)", m_diffMenuWidget);
    QPushButton *normalBtn = new QPushButton("🎸 普通 (标准)", m_diffMenuWidget);
    QPushButton *hardBtn = new QPushButton("🔥 疯狂 (手速怪)", m_diffMenuWidget);

    // 返回按钮专属的淡入淡出暗色样式
    QPushButton *backBtn = new QPushButton("⬅ 返回菜单", m_diffMenuWidget);
    backBtn->setStyleSheet(R"(
        QPushButton { background: transparent; border: 2px solid #aaaaaa; border-radius: 8px; color: #aaaaaa; font-size: 16px; font-weight: bold; padding: 10px; }
        QPushButton:hover { background-color: rgba(255,255,255,30); color: #ffffff; border: 2px solid #ffffff; }
    )");

    easyBtn->setStyleSheet(btnStyle); easyBtn->setCursor(Qt::PointingHandCursor);
    normalBtn->setStyleSheet(btnStyle); normalBtn->setCursor(Qt::PointingHandCursor);
    hardBtn->setStyleSheet(btnStyle); hardBtn->setCursor(Qt::PointingHandCursor);
    backBtn->setCursor(Qt::PointingHandCursor);

    diffLayout->addWidget(diffTitle);
    diffLayout->addSpacing(15);
    diffLayout->addWidget(easyBtn);
    diffLayout->addWidget(normalBtn);
    diffLayout->addWidget(hardBtn);
    diffLayout->addStretch();
    diffLayout->addWidget(backBtn);

    m_stackedLayout->addWidget(m_diffMenuWidget); // 将第 2 页加入栈


    // ==========================================
    // 📖 第 3 页：网络联机大厅 (m_onlineMenuWidget)
    // ==========================================
    m_udpSocket = new QUdpSocket(this); // 初始化雷达

    m_onlineMenuWidget = new QWidget(panel);
    auto *onlineLayout = new QVBoxLayout(m_onlineMenuWidget);
    onlineLayout->setSpacing(15);
    onlineLayout->setContentsMargins(40, 40, 40, 40);

    QLabel *onlineTitle = new QLabel("🌐 附近 Livehouse 雷达", m_onlineMenuWidget);
    onlineTitle->setStyleSheet("font-size: 24px; font-weight: 900; color: #00FFFF; background: transparent;");
    onlineTitle->setAlignment(Qt::AlignCenter);

    // 🎸 房间列表控件 (美化版)
    m_roomList = new QListWidget(m_onlineMenuWidget);
    m_roomList->setStyleSheet(R"(
        QListWidget {
            background-color: rgba(255, 255, 255, 15);
            border: 2px solid rgba(255, 105, 180, 80);
            border-radius: 10px;
            color: white;
            font-size: 18px;
            font-weight: bold;
            padding: 5px;
        }
        QListWidget::item { padding: 15px; border-bottom: 1px solid rgba(255,255,255,30); }
        QListWidget::item:hover { background-color: rgba(255, 105, 180, 50); border-radius: 8px; }
        QListWidget::item:selected { background-color: #ff69b4; color: white; border-radius: 8px; }
    )");
    m_roomList->setCursor(Qt::PointingHandCursor);

    // 按钮区
    QPushButton *hostBtn = new QPushButton("🎸 创建我的 Livehouse", m_onlineMenuWidget);


    QPushButton *onlineBackBtn = new QPushButton("⬅ 返回菜单", m_onlineMenuWidget);

    hostBtn->setStyleSheet(btnStyle); hostBtn->setCursor(Qt::PointingHandCursor);



    onlineBackBtn->setStyleSheet(backBtn->styleSheet()); onlineBackBtn->setCursor(Qt::PointingHandCursor);

    onlineLayout->addWidget(onlineTitle);
    onlineLayout->addWidget(m_roomList);
    onlineLayout->addWidget(hostBtn);
    onlineLayout->addWidget(onlineBackBtn);

    m_stackedLayout->addWidget(m_onlineMenuWidget); // 将第 3 页加入栈

    // ==========================================
    // 绑定第 3 页的信号
    // ==========================================
    connect(hostBtn, &QPushButton::clicked, this, &LobbyWidget::onHostRoomClicked);


    // 这里复用了返回菜单槽函数，一并处理停止雷达的逻辑
    connect(onlineBackBtn, &QPushButton::clicked, this, &LobbyWidget::onBackToMenuClicked);
    // 列表点击事件
    connect(m_roomList, &QListWidget::itemClicked, this, &LobbyWidget::onRoomSelected);
    // 雷达接收事件
    connect(m_udpSocket, &QUdpSocket::readyRead, this, &LobbyWidget::onUdpReadyRead);


    // ==========================================
    // 将整个 Panel 丢到大厅正中间
    // ==========================================
    mainLayout->addWidget(panel, 0, Qt::AlignCenter);

    // ==========================================
    // 信号绑定路由
    // ==========================================
    connect(singleBtn, &QPushButton::clicked, this, &LobbyWidget::onSingleClicked);
    connect(aiBtn, &QPushButton::clicked, this, &LobbyWidget::onAIClicked);
    connect(onlineBtn, &QPushButton::clicked, this, &LobbyWidget::onOnlineClicked);

    connect(easyBtn, &QPushButton::clicked, this, &LobbyWidget::onEasyClicked);
    connect(normalBtn, &QPushButton::clicked, this, &LobbyWidget::onNormalClicked);
    connect(hardBtn, &QPushButton::clicked, this, &LobbyWidget::onHardClicked);
    connect(backBtn, &QPushButton::clicked, this, &LobbyWidget::onBackToMenuClicked);
}

void LobbyWidget::refreshScore() {
    int newScore = DBHelper::getInstance().getUserTotalScore(m_session.uid);
    m_session.totalScore = newScore;
    m_scoreLabel->setText(QString("🏆 当前总积分: %1").arg(newScore));
}

// === 核心逻辑槽函数 ===

void LobbyWidget::onSingleClicked() { emit modeSelected(m_session, GameMode::Single); }

// 点了“人机对战” -> 不发信号，直接将面板页码翻到第 2 页 (索引1)
void LobbyWidget::onAIClicked() {
    m_stackedLayout->setCurrentIndex(1);
}


// 发射进游戏的信号前，顺手把状态切回第 1 页，这样下次你退回大厅时不会卡在难度选择界面
void LobbyWidget::onEasyClicked() {
    m_stackedLayout->setCurrentIndex(0);
    emit modeSelected(m_session, GameMode::AI, AIDifficulty::Easy);
}

void LobbyWidget::onNormalClicked() {
    m_stackedLayout->setCurrentIndex(0);
    emit modeSelected(m_session, GameMode::AI, AIDifficulty::Normal);
}

void LobbyWidget::onHardClicked() {
    m_stackedLayout->setCurrentIndex(0);
    emit modeSelected(m_session, GameMode::AI, AIDifficulty::Hard);
}



// ==== 页面切换逻辑 ====

// 点击主菜单的“网络竞技”
void LobbyWidget::onOnlineClicked() {
    m_roomList->clear(); // 清空旧列表

    // 👇 UI 升级：加入科技感占位符
    QListWidgetItem *loadingItem = new QListWidgetItem("📡 正在全频段扫描附近的 Livehouse...");
    loadingItem->setFlags(Qt::NoItemFlags); // 设置为不可点击
    loadingItem->setTextAlignment(Qt::AlignCenter);
    loadingItem->setForeground(QColor(170, 170, 170)); // 灰色字体
    m_roomList->addItem(loadingItem);


    m_stackedLayout->setCurrentIndex(2); // 丝滑翻到第 3 页
    startScanning(); // 开启雷达监听
}

// 修改原有的返回菜单逻辑
void LobbyWidget::onBackToMenuClicked() {
    stopScanning(); // 退回主菜单时，关闭雷达省电
    m_stackedLayout->setCurrentIndex(0);
}

// ==== 雷达扫描逻辑 ====

void LobbyWidget::startScanning() {
    // 绑定 8889 端口听广播
    if (m_udpSocket->state() != QAbstractSocket::BoundState) {
        m_udpSocket->bind(8889, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
    }
}

void LobbyWidget::stopScanning() {
    m_udpSocket->close();
}

void LobbyWidget::onUdpReadyRead() {
    while (m_udpSocket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = m_udpSocket->receiveDatagram();
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(datagram.data(), &error);

        if (error.error == QJsonParseError::NoError && doc.isObject()) {
            QJsonObject json = doc.object();
            if (json["type"].toString() == "room_broadcast") {
                QString roomName = json["roomName"].toString();
                QString hostIp = datagram.senderAddress().toString().remove("::ffff:"); // 清理 IPv6 前缀

                // 检查列表里是不是已经有这个 IP 的房间了（去重）
                bool alreadyExists = false;
                for (int i = 0; i < m_roomList->count(); ++i) {
                    if (m_roomList->item(i)->data(Qt::UserRole).toString() == hostIp) {
                        alreadyExists = true;
                        break;
                    }
                }

                QString formattedRoomName = QString("🎵 %1").arg(roomName);
                // 检查列表里是不是已经有这个名字的房间了
                for (int i = 0; i < m_roomList->count(); ++i) {
                    if (m_roomList->item(i)->text() == formattedRoomName) {
                        alreadyExists = true;

                        // 👇 【神级修复】：如果房间已存在，且新收到的 IP 是 127.0.0.1
                        // 说明玩家是在同一台电脑上双开测试，优先使用本地回环地址覆盖！
                        if (hostIp == "127.0.0.1") {
                            m_roomList->item(i)->setData(Qt::UserRole, hostIp);
                        }

                        break;
                    }
                }

                if (!alreadyExists) {
                    // 👇 UI 升级：如果第一行是咱们的不可点击占位符，就清空它
                    if (m_roomList->count() > 0 && m_roomList->item(0)->flags() == Qt::NoItemFlags) {
                        m_roomList->clear();
                    }
                    // 💡 神级操作：界面上只显示名字，但把 IP 藏在 UserRole 里！
                    QListWidgetItem *item = new QListWidgetItem(QString("🎵 %1").arg(roomName));
                    item->setData(Qt::UserRole, hostIp);
                    m_roomList->addItem(item);
                }
            }
        }
    }
}

// ==== 联机行动逻辑 ====

void LobbyWidget::onHostRoomClicked() {
    stopScanning();
    m_stackedLayout->setCurrentIndex(0); // 顺手把页面切回主菜单，防下次回来卡住
    // 发射建房信号 (isHost = true)
    emit onlineGameRequested(m_session, true, "");
}

void LobbyWidget::onRoomSelected(QListWidgetItem *item) {
    stopScanning();
    m_stackedLayout->setCurrentIndex(0);

    // 💡 取出偷偷藏在条目里的 IP 地址
    QString targetIp = item->data(Qt::UserRole).toString();

    // 发射加入信号 (isHost = false)
    emit onlineGameRequested(m_session, false, targetIp);
}

void LobbyWidget::openRadar() {
    // onOnlineClicked 里面已经包含了清空列表、显示扫描中动画、切到第3页并启动雷达的代码！
    onOnlineClicked();
}
