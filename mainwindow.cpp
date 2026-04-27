#include "MainWindow.h"
#include <QHBoxLayout> // 用于顶部状态栏的水平排布
#include <QFrame> // 确保引入了 QFrame

// 【修改点】：接收 GameMode mode
MainWindow::MainWindow(UserSession session, GameMode mode, QWidget *parent)
    : QMainWindow(parent), m_session(session), m_currentMode(mode)
{
    setWindowTitle("Bocchi Clear! - 摇滚消消乐");

    // 1. 初始化逻辑层与舞台
    m_logic = new GameLogic(GameConfig::BOARD_ROWS, GameConfig::BOARD_COLS, this);
    m_gamePanel = new GamePanel(m_logic, this);

    setupUI();

    // 2. 启动游戏逻辑
    // 【关键修改】：使用大厅传过来的 mode 启动游戏，而不是写死的 GameMode::Single
    m_logic->startNewGame(m_session.uid, m_currentMode);

    // 如果选择了人机对战模式，可以在这里或者 GameLogic 中触发 AI 相关的定时器
    if (m_currentMode == GameMode::AI) {
        // TODO: 启动 AI 定时器等操作
        // 例如：m_logic->startAI();
    }

    // 3. 监听得分变化
    connect(m_logic, &GameLogic::scoreUpdated, this, &MainWindow::updateScoreLabel);

    // 4. 连接连击信号
    connect(m_logic, &GameLogic::comboUpdated, this, &MainWindow::updateComboLabel);
}

void MainWindow::setupUI() {
    QWidget *centralWidget = new QWidget(this);
    centralWidget->setObjectName("mainContainer");

    // 【修改点1】：去掉重复的 stage_bg 背景图设置，因为底层的 QStackedWidget 已经有背景了
    // 这里把 centralWidget 设置为全透明
    centralWidget->setStyleSheet("background: transparent;");

    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // 【修改点2】：创建一个中央玻璃面板（游戏机台）
    QFrame *boardPanel = new QFrame(centralWidget);
    boardPanel->setObjectName("boardPanel");

    // 设定一个固定大小，刚好能包住 640x640 的棋盘和顶部的计分板
    boardPanel->setFixedSize(740, 780);

    boardPanel->setStyleSheet(R"(
        #boardPanel {
            /* 【关键修改】：把最后的 210 改成 100，让它变透，露出背景灯光！ */
            background-color: rgba(30, 30, 45, 100);
            border-radius: 20px;
            border: 2px solid rgba(255, 105, 180, 150); /* 稍微加亮一点粉色边框 */
        }

        QLabel {
            color: white;
            background-color: transparent;
            font-family: "Microsoft YaHei", "Segoe UI";
        }

        QLabel#infoLabel {
            font-size: 16px;
            font-weight: bold;
        }

        QLabel#scoreLabel {
            font-size: 22px;
            font-weight: 900;
            color: #ffb6c1;
        }
    )");


    // 为面板内部创建一个垂直布局
    QVBoxLayout *panelLayout = new QVBoxLayout(boardPanel);
    panelLayout->setContentsMargins(40, 30, 40, 30);
    panelLayout->setSpacing(20);

    // --- 顶部状态栏排布 ---
    QString modeStr = (m_currentMode == GameMode::Single) ? "单人" : (m_currentMode == GameMode::AI ? "人机" : "网络");
    m_infoLabel = new QLabel(QString("🎸 乐手: %1 | 模式: %2 | 🏆 最高: %3")
                                 .arg(m_session.nickname)
                                 .arg(modeStr)
                                 .arg(m_session.totalScore));
    m_infoLabel->setObjectName("infoLabel");

    m_scoreLabel = new QLabel("🎵 当前得分: 0");
    m_scoreLabel->setObjectName("scoreLabel");

    m_comboLabel = new QLabel("");
    m_comboLabel->setObjectName("comboLabel");
    m_comboLabel->setAlignment(Qt::AlignCenter);
    m_comboLabel->hide();

    // 水平组装顶部栏
    QHBoxLayout *topLayout = new QHBoxLayout();
    topLayout->addWidget(m_infoLabel);
    topLayout->addStretch();
    topLayout->addWidget(m_comboLabel);
    topLayout->addStretch();
    topLayout->addWidget(m_scoreLabel);

    // 【核心组装】：把顶部栏和游戏棋盘装进玻璃面板里！
    panelLayout->addLayout(topLayout);
    // 将 GamePanel 居中放入面板
    panelLayout->addWidget(m_gamePanel, 0, Qt::AlignHCenter);

    // 【最外层排版】：将整个玻璃机台正居中放在全屏的主窗口里
    mainLayout->addWidget(boardPanel, 0, Qt::AlignCenter);

    setCentralWidget(centralWidget);
}

void MainWindow::updateScoreLabel(int score) {
    // 保持 Emoji 符号，每次更新时重新拼接
    m_scoreLabel->setText(QString("🎵 当前得分: %1").arg(score));
}

void MainWindow::updateComboLabel(int combo) {
    // 连击数小于 2 时不显示
    if (combo < 2) {
        m_comboLabel->hide();
        return;
    }

    m_comboLabel->show();
    // 更新文字内容，加上火焰 Emoji 增加燃点
    m_comboLabel->setText(QString("🔥 %1 COMBO!").arg(combo));

    // 使用 QVariantAnimation 动态改变 QSS 中的 font-size
    QVariantAnimation *ani = new QVariantAnimation(this);
    ani->setDuration(300); // 动画持续 300 毫秒

    ani->setStartValue(20);      // 初始字号：20px
    ani->setKeyValueAt(0.5, 40); // 关键帧（动画进行到一半时）：突然放大到 40px
    ani->setEndValue(26);        // 结束字号：稳定在 26px

    // 每次数值变化时，重新应用样式表
    connect(ani, &QVariantAnimation::valueChanged, [this](const QVariant &value) {
        int fontSize = value.toInt();
        // 专门为 Combo 设置的摇滚金配色，并且去掉背景黑框，让它看起来像悬浮特效
        QString style = QString(
                            "font-size: %1px; "
                            "font-weight: 900; "
                            "color: #FFD700; "             /* 耀眼的金色 */
                            "background-color: transparent; "
                            "border: none;"
                            ).arg(fontSize);

        m_comboLabel->setStyleSheet(style);
    });

    ani->start(QAbstractAnimation::DeleteWhenStopped);
}
