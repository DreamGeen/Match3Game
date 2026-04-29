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
void LobbyWidget::onOnlineClicked() { emit modeSelected(m_session, GameMode::Online); }

// 点了“人机对战” -> 不发信号，直接将面板页码翻到第 2 页 (索引1)
void LobbyWidget::onAIClicked() {
    m_stackedLayout->setCurrentIndex(1);
}

// 点了“返回菜单” -> 面板页码翻回第 1 页 (索引0)
void LobbyWidget::onBackToMenuClicked() {
    m_stackedLayout->setCurrentIndex(0);
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
