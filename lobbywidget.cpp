#include "LobbyWidget.h"

LobbyWidget::LobbyWidget(UserSession session, QWidget *parent)
    : QWidget(parent), m_session(session) {
    initUI();
    setWindowTitle("Bocchi Clear! - 游戏大厅");
    setFixedSize(400, 500);
}

void LobbyWidget::initUI() {
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(20);
    mainLayout->setContentsMargins(40, 30, 40, 30);

    // 1. 玩家信息展示区
    QLabel *welcomeLabel = new QLabel(QString("欢迎回来，%1！").arg(m_session.nickname), this);
    welcomeLabel->setStyleSheet("font-size: 20px; font-weight: bold; color: #333;");
    welcomeLabel->setAlignment(Qt::AlignCenter);

    QLabel *scoreLabel = new QLabel(QString("🎸 当前总积分: %1").arg(m_session.totalScore), this);
    scoreLabel->setStyleSheet("font-size: 14px; color: #666;");
    scoreLabel->setAlignment(Qt::AlignCenter);

    // 2. 模式选择按钮
    QString btnStyle = "QPushButton { background-color: #f0f0f0; border: 2px solid #ddd; border-radius: 10px; font-size: 16px; padding: 15px; }"
                       "QPushButton:hover { background-color: #ffb6c1; border-color: #ff69b4; color: white; }";

    QPushButton *singleBtn = new QPushButton("🎸 单人闯关 (Classic)", this);
    QPushButton *aiBtn = new QPushButton("🤖 人机对战 (AI Mode)", this);
    QPushButton *onlineBtn = new QPushButton("🌐 网络竞技 (Online)", this);

    singleBtn->setStyleSheet(btnStyle);
    aiBtn->setStyleSheet(btnStyle);
    onlineBtn->setStyleSheet(btnStyle);

    // 布局组装
    mainLayout->addWidget(welcomeLabel);
    mainLayout->addWidget(scoreLabel);
    mainLayout->addSpacing(20);
    mainLayout->addWidget(singleBtn);
    mainLayout->addWidget(aiBtn);
    mainLayout->addWidget(onlineBtn);
    mainLayout->addStretch();

    // 信号绑定
    connect(singleBtn, &QPushButton::clicked, this, &LobbyWidget::onSingleClicked);
    connect(aiBtn, &QPushButton::clicked, this, &LobbyWidget::onAIClicked);
    connect(onlineBtn, &QPushButton::clicked, this, &LobbyWidget::onOnlineClicked);
}

void LobbyWidget::onSingleClicked() { emit modeSelected(m_session, GameMode::Single); }
void LobbyWidget::onAIClicked() { emit modeSelected(m_session, GameMode::AI); }
void LobbyWidget::onOnlineClicked() { emit modeSelected(m_session, GameMode::Online); }
