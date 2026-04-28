#include "LobbyWidget.h"
#include <QFrame>
#include "DBHelper.h"

LobbyWidget::LobbyWidget(UserSession session, QWidget *parent)
    : QWidget(parent), m_session(session) {

    // 强制 Qt 渲染顶层 QWidget 的 QSS 背景
    this->setAttribute(Qt::WA_StyledBackground, true);

    initUI();
    setWindowTitle("Bocchi Clear! - 游戏大厅");




    // 如果你想要真正的纯粹全屏（没有任何边框，连任务栏都遮住），把上面那行注释掉，换成这行：
    // this->showFullScreen();
    // 注意：如果是纯全屏，你需要按 Alt+F4 才能关闭窗口，或者得在界面里自己写一个“退出游戏”的按钮。
}

void LobbyWidget::initUI() {
    this->setObjectName("lobbyMain");
    this->setStyleSheet("#lobbyMain { border-image: url(:/res/backgrounds/stage_bg.png); }");

    auto *mainLayout = new QVBoxLayout(this);
    // 外层边距设为 0，让背景完全铺满
    mainLayout->setContentsMargins(0, 0, 0, 0);

    QFrame *panel = new QFrame(this);
    panel->setObjectName("glassPanel");

    // 【修改点 2】：因为屏幕变大了，把中间的菜单面板也适当放大一点，比例更协调
    panel->setFixedSize(500, 520);

    panel->setStyleSheet(R"(
        #glassPanel {
            background-color: rgba(20, 20, 30, 210);
            border-radius: 15px;
            border: 1px solid rgba(255, 105, 180, 80);
        }
    )");

    auto *panelLayout = new QVBoxLayout(panel);
    panelLayout->setSpacing(30); // 间距也稍微拉大一点
    panelLayout->setContentsMargins(40, 50, 40, 50);

    // --- 下面的文字和按钮部分 ---
    QLabel *welcomeLabel = new QLabel(QString("🎸 欢迎回来，%1！").arg(m_session.nickname), panel);
    welcomeLabel->setStyleSheet("font-size: 26px; font-weight: 900; color: #ffffff; background: transparent;"); // 字号微微放大
    welcomeLabel->setAlignment(Qt::AlignCenter);

    // 改成下面这样：
    m_scoreLabel = new QLabel(QString("🏆 当前总积分: %1").arg(m_session.totalScore), panel);
    m_scoreLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #FFD700; background: transparent;");
    m_scoreLabel->setAlignment(Qt::AlignCenter);



    QString btnStyle = R"(
        QPushButton {
            background-color: rgba(255, 105, 180, 30);
            border: 2px solid #ff69b4;
            border-radius: 8px;
            color: #ffffff;
            font-size: 20px; /* 按钮字号放大 */
            font-weight: bold;
            padding: 15px;   /* 按钮增厚 */
            font-family: "Microsoft YaHei";
        }
        QPushButton:hover {
            background-color: #ff69b4;
            color: #ffffff;
            border: 2px solid #ffffff;
        }
        QPushButton:pressed {
            background-color: #c71585;
        }
    )";

    QPushButton *singleBtn = new QPushButton("🎸 单人闯关 (Classic)", panel);
    QPushButton *aiBtn = new QPushButton("🤖 人机对战 (AI Mode)", panel);
    QPushButton *onlineBtn = new QPushButton("🌐 网络竞技 (Online)", panel);

    singleBtn->setStyleSheet(btnStyle);
    aiBtn->setStyleSheet(btnStyle);
    onlineBtn->setStyleSheet(btnStyle);

    singleBtn->setCursor(Qt::PointingHandCursor);
    aiBtn->setCursor(Qt::PointingHandCursor);
    onlineBtn->setCursor(Qt::PointingHandCursor);

    panelLayout->addWidget(welcomeLabel);
    // 把后面加进布局的 panelLayout->addWidget(scoreLabel); 也改成 m_scoreLabel
    panelLayout->addWidget(m_scoreLabel);
    panelLayout->addSpacing(15);
    panelLayout->addWidget(singleBtn);
    panelLayout->addWidget(aiBtn);
    panelLayout->addWidget(onlineBtn);
    panelLayout->addStretch();

    // 【核心机制】：把组装好的半透明面板居中添加到最大化的主窗口里
    mainLayout->addWidget(panel, 0, Qt::AlignCenter);

    connect(singleBtn, &QPushButton::clicked, this, &LobbyWidget::onSingleClicked);
    connect(aiBtn, &QPushButton::clicked, this, &LobbyWidget::onAIClicked);
    connect(onlineBtn, &QPushButton::clicked, this, &LobbyWidget::onOnlineClicked);
}

// 第二处：在文件末尾实现刷新函数
void LobbyWidget::refreshScore() {
    // 从数据库重新拉取最新积分
    int newScore = DBHelper::getInstance().getUserTotalScore(m_session.uid);
    m_session.totalScore = newScore; // 更新本地 session
    m_scoreLabel->setText(QString("🏆 当前总积分: %1").arg(newScore)); // 刷新 UI
}

void LobbyWidget::onSingleClicked() { emit modeSelected(m_session, GameMode::Single); }
void LobbyWidget::onAIClicked() { emit modeSelected(m_session, GameMode::AI); }
void LobbyWidget::onOnlineClicked() { emit modeSelected(m_session, GameMode::Online); }
