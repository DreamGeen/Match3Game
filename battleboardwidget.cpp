#include "BattleBoardWidget.h"
#include <QTimer>

BattleBoardWidget::BattleBoardWidget(QString playerName, bool isBot, QWidget *parent)
    : QFrame(parent), m_isBot(isBot)
{
    m_logic = new GameLogic(GameConfig::BOARD_ROWS, GameConfig::BOARD_COLS, this);
    m_panel = new GamePanel(m_logic, this);

    setupUI(playerName);
    initConnections();

    // 如果是托管模式（AI或网络对手），封锁鼠标点击，开启机器大脑
    if (m_isBot) {
        m_logic->setBotMode(true);
        m_panel->setInteractive(false);
    }
}

void BattleBoardWidget::applyTextShadow(QWidget* widget) {
    QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect(widget);
    shadow->setOffset(1, 2);
    shadow->setBlurRadius(5);
    shadow->setColor(QColor(0, 0, 0, 220));
    widget->setGraphicsEffect(shadow);
}

void BattleBoardWidget::setupUI(QString playerName) {
    // 完美继承你原来的玻璃面板样式
    this->setObjectName("boardPanel");
    this->setStyleSheet(R"(
        #boardPanel {
            background-color: rgba(0, 0, 0, 80);
            border-radius: 20px;
            border: 1px solid rgba(255, 255, 255, 40);
        }
        QLabel { background: transparent; font-family: "Microsoft YaHei", "Segoe UI"; }
    )");

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(15);

    // 顶部状态栏：左边名字，右边分数
    QHBoxLayout *topLayout = new QHBoxLayout();
    m_nameLabel = new QLabel(playerName, this);
    m_nameLabel->setStyleSheet(m_isBot ? "font-size: 18px; font-weight: bold; color: #00FFFF;"
                                       : "font-size: 18px; font-weight: bold; color: #FFD700;");
    applyTextShadow(m_nameLabel);

    m_scoreLabel = new QLabel("🎵 得分: 0", this);
    m_scoreLabel->setStyleSheet("font-size: 18px; font-weight: 900; color: #ffb6c1;");
    applyTextShadow(m_scoreLabel);

    topLayout->addWidget(m_nameLabel);
    topLayout->addStretch();
    topLayout->addWidget(m_scoreLabel);

    layout->addLayout(topLayout);
    layout->addWidget(m_panel, 0, Qt::AlignHCenter);
}

void BattleBoardWidget::initConnections() {
    // 1. 内部消化分数更新
    connect(m_logic, &GameLogic::scoreUpdated, this, [this](int score){
        m_scoreLabel->setText(QString("🎵 得分: %1").arg(score));
    });

    // 2. 向上层透传关键事件
    connect(m_logic, &GameLogic::targetReached, this, &BattleBoardWidget::targetReached);
    connect(m_logic, &GameLogic::levelFinished, this, &BattleBoardWidget::levelFinished);

    // 3. AI 的神经回路连接
    if (m_isBot) {
        // AI 思考出了结果 -> 发给面板播动画
        connect(m_logic, &GameLogic::aiMoveDecided, this, &BattleBoardWidget::executeMove);

        // 面板播完了一波连消动画 -> 延迟 0.8 秒后，触发 AI 思考下一步
        connect(m_panel, &GamePanel::actionFinished, m_logic, [this](){
            if (!m_logic->isGameOver()) {
                QTimer::singleShot(800, m_logic, &GameLogic::triggerNextBotMove);
            }
        });
    }
}

void BattleBoardWidget::startLevel(int targetScore, int maxMoves, int uid, GameMode mode) {
    m_scoreLabel->setText("🎵 得分: 0");
    m_logic->startLevel(uid, mode, targetScore, maxMoves);

    // 如果是 AI，开局 1.5 秒后主动发起第一次攻击
    if (m_isBot) {
        QTimer::singleShot(1500, m_logic, &GameLogic::triggerNextBotMove);
    }
}

void BattleBoardWidget::executeMove(QPoint p1, QPoint p2) {
    m_panel->onAiMoveDecided(p1, p2);
}
