#include "MainWindow.h"
#include <QHBoxLayout> // 用于顶部状态栏的水平排布

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
    // 给主容器起个名字，防止背景污染其他窗口
    centralWidget->setObjectName("mainContainer");

    // 注入全局摇滚风格 QSS
    this->setStyleSheet(R"(
        #mainContainer {
            /* 替换为你的绝美 Livehouse/活动室 背景图路径 */
            border-image: url(:/res/backgrounds/stage_bg.png);
        }

        /* 通用标签样式：白字 + 半透明黑底 + 圆角，保证在花哨背景下绝对清晰 */
        QLabel {
            color: white;
            background-color: rgba(0, 0, 0, 160);
            border-radius: 8px;
            padding: 8px 12px;
            font-family: "Microsoft YaHei", "Segoe UI";
        }

        /* 针对 infoLabel 的专属字号 */
        QLabel#infoLabel {
            font-size: 16px;
            font-weight: bold;
        }

        /* 针对 scoreLabel 的专属字号和波奇粉色 */
        QLabel#scoreLabel {
            font-size: 22px;
            font-weight: 900;
            color: #ffb6c1; /* 调整为更柔和的高亮粉色，避免刺眼 */
            border: 2px solid #ff69b4; /* 加上粉色边框增加立体感 */
        }
    )");

    // 主垂直布局
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    // 增加四周的边距(左, 上, 右, 下)，露出更多背景图，形成“相框”感
    mainLayout->setContentsMargins(30, 20, 30, 30);
    mainLayout->setSpacing(20); // 顶部栏和棋盘之间的间距

    // 优化顶部状态栏排布
    QString modeStr = (m_currentMode == GameMode::Single) ? "单人" : (m_currentMode == GameMode::AI ? "人机" : "网络");
    m_infoLabel = new QLabel(QString("🎸 乐手: %1 | 模式: %2 | 🏆 历史最高: %3")
                                 .arg(m_session.nickname)
                                 .arg(modeStr)
                                 .arg(m_session.totalScore));
    m_infoLabel->setObjectName("infoLabel"); // 绑定 QSS

    m_scoreLabel = new QLabel("🎵 当前得分: 0");
    m_scoreLabel->setObjectName("scoreLabel"); // 绑定 QSS

    // 初始化 Combo 标签
    m_comboLabel = new QLabel("");
    m_comboLabel->setObjectName("comboLabel");
    m_comboLabel->setAlignment(Qt::AlignCenter);
    m_comboLabel->hide(); // 默认隐藏，等有连击了再显示

    // 创建一个水平布局来放置信息和分数
    QHBoxLayout *topLayout = new QHBoxLayout();
    topLayout->addWidget(m_infoLabel);
    topLayout->addStretch(); // 左弹簧

    topLayout->addWidget(m_comboLabel); // 把 Combo 放在正中间！

    topLayout->addStretch(); // 右弹簧
    topLayout->addWidget(m_scoreLabel);

    // 将组装好的顶部栏和游戏面板加入主布局
    mainLayout->addLayout(topLayout);
    // 将 GamePanel 居中放置，即使窗口拉伸，棋盘也不会变形
    mainLayout->addWidget(m_gamePanel, 0, Qt::AlignHCenter);

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
