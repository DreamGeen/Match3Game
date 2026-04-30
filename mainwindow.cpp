#include "MainWindow.h"
#include <QHBoxLayout>
#include <QFrame>
#include <QMessageBox>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <QGraphicsDropShadowEffect>
#include <QVariantAnimation>
#include <QApplication>
#include <QStackedLayout>
#include <QStyle>
#include <QTimer> // 记得包含这个
#include <QRandomGenerator>
#include "uihelper.h"
#include <QPixmap>
#include <QDateTime>
#include <QStandardPaths>
#include <QDir>


MainWindow::MainWindow(UserSession session, GameMode mode, AIDifficulty diff,bool isHost, QString targetIp ,QWidget *parent)
    : QMainWindow(parent), m_session(session), m_currentMode(mode),m_isHost(isHost),m_targetIp(targetIp)
{
    setWindowTitle("Bocchi Clear! - 全国巡演模式");

    // 永远存在的玩家逻辑与面板
    m_logic = new GameLogic(GameConfig::BOARD_ROWS, GameConfig::BOARD_COLS, this);
    m_gamePanel = new GamePanel(m_logic, this);

    // 【新增】如果是AI模式，初始化AI组件并开启托管
    if (m_currentMode == GameMode::AI) {
        // 根据难度设置 AI 的“手速”
        if (diff == AIDifficulty::Easy) m_aiDelay = 2500;
        else if (diff == AIDifficulty::Hard) m_aiDelay = 600;
        else m_aiDelay = 1800;

        m_aiLogic = new GameLogic(GameConfig::BOARD_ROWS, GameConfig::BOARD_COLS, this);

        // 👇 【关键连接】：把大厅传来的 diff 难度参数注入给底层 AI 大脑
        m_aiLogic->setBotMode(true, diff);

        m_aiPanel = new GamePanel(m_aiLogic, this);
        m_aiPanel->setInteractive(false); // 封锁AI面板的鼠标点击
    }


    // 👇 1. 处理网络管理器的初始化（但不在这里画 UI 和连信号）
    if (m_currentMode == GameMode::Online) {
        m_netManager = new NetworkManager(this);
        if (m_isHost) {
            QString roomName = m_session.username + " 的Livehouse";
            m_netManager->hostGame(roomName, 8888);
        } else {
            m_netManager->joinGame(m_targetIp, 8888);
        }

        m_aiLogic = new GameLogic(GameConfig::BOARD_ROWS, GameConfig::BOARD_COLS, this);
        m_aiLogic->setBotMode(true);
        m_aiPanel = new GamePanel(m_aiLogic, this);
        m_aiPanel->setInteractive(false);
    }

    // ==========================================
    // 关卡数据配置表：加入物理磁盘路径下的 MV 视频
    // ==========================================
    QString appPath = QApplication::applicationDirPath();
    m_levels = {
        {300,  10, ":/res/backgrounds/level1.png", appPath + "/video/mv1.mp4"},
        {3500,  25, ":/res/backgrounds/level2.png", appPath + "/video/mv2.mp4"},
        {6000, 22, ":/res/backgrounds/level3.png", appPath + "/video/mv3.mp4"},
        {10000, 20, ":/res/backgrounds/level4.png", appPath + "/video/mv4.mp4"},
        {18000, 15, ":/res/backgrounds/level5.png", appPath + "/video/mv5.mp4"}
    };

    // 如果你暂时只有 stage_bg.png，可以取消下面这行的注释进行测试：
    // for(auto& lvl : m_levels) lvl.bgPath = ":/res/backgrounds/stage_bg.png";

    setupUI();          // 调用路由入口
    setupResultPanel(); // 原样保留
    initConnections();  // 这里处理了胜负结算逻辑分流

    // 💡 只有等 setupUI 把所有按钮都画出来了，我们再去绑定网络专属的按钮信号！
    if (m_currentMode == GameMode::Online) {
        initOnlineConnections();
    }

    // 找到这段代码，修改为：
    // 根据模式决定初始关卡
    if (m_currentMode == GameMode::AI) {
        int randomIndex = QRandomGenerator::global()->bounded(m_levels.size());
        loadLevel(randomIndex);
    } else if (m_currentMode == GameMode::Single) { // 👇 关键修复 2：改成 else if
        loadLevel(0);
    }
    // 💡 重点：如果是 GameMode::Online，这里什么都不要做！
    // 两边的黑框框会静静地等待。直到网络连通，调用 startGameWithSeed() 时，才会正式开始掉落方块！

    // 连接逻辑层更新信号
    connect(m_logic, &GameLogic::scoreUpdated, this, &MainWindow::updateScoreLabel);
    connect(m_logic, &GameLogic::comboUpdated, this, &MainWindow::updateComboLabel);
}


// ==========================================
// 核心 UI 路由入口
// ==========================================
void MainWindow::setupUI() {
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    centralWidget->setObjectName("mainContainer");
    centralWidget->setStyleSheet("background: transparent;");

    setupCommonBackground(centralWidget);

    if (m_currentMode == GameMode::Single) {
        setupSinglePlayerUI(centralWidget);
    } else if (m_currentMode == GameMode::AI|| m_currentMode == GameMode::Online) {
        setupAIBattleUI(centralWidget);
    }
}

// ==========================================
// 公共底层背景 (视频 + 透明图)
// ==========================================
void MainWindow::setupCommonBackground(QWidget *centralWidget) {
    m_videoView = new QGraphicsView(centralWidget);
    m_videoView->setStyleSheet("background: transparent; border: none;");
    m_videoView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_videoView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_videoView->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_videoView->hide();

    m_videoScene = new QGraphicsScene(m_videoView);
    m_videoView->setScene(m_videoScene);

    m_videoItem = new QGraphicsVideoItem();
    m_videoItem->setAspectRatioMode(Qt::KeepAspectRatioByExpanding);
    m_videoScene->addItem(m_videoItem);

    m_mediaPlayer = new QMediaPlayer(this);
    m_mediaPlayer->setVideoOutput(m_videoItem);
    connect(m_mediaPlayer, &QMediaPlayer::mediaStatusChanged, [this](QMediaPlayer::MediaStatus status){
        if (status == QMediaPlayer::EndOfMedia) {
            m_mediaPlayer->setPosition(0);
            m_mediaPlayer->play();
        }
    });

    m_baseBgLabel = new QLabel(centralWidget);
    m_bgRevealLabel = new QLabel(centralWidget);
    m_baseBgLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_bgRevealLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
}

// ==========================================
// 单人闯关 UI (100%原样复刻你的代码)
// ==========================================
void MainWindow::setupSinglePlayerUI(QWidget *centralWidget) {
    QFrame *boardPanel = new QFrame(centralWidget);
    boardPanel->setObjectName("boardPanel");
    boardPanel->setFixedSize(820, 780);
    boardPanel->setStyleSheet(R"(
        #boardPanel {
            background-color: rgba(0, 0, 0, 80);
            border-radius: 20px;
            border: 1px solid rgba(255, 255, 255, 40);
        }
        QLabel { color: white; background: transparent; font-family: "Microsoft YaHei", "Segoe UI"; }
    )");

    QVBoxLayout *panelLayout = new QVBoxLayout(boardPanel);
    panelLayout->setContentsMargins(30, 30, 30, 30);
    panelLayout->setSpacing(20);


    auto applyTextShadow = [](QWidget* widget) {
        QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect(widget);
        shadow->setOffset(1, 2); shadow->setBlurRadius(5); shadow->setColor(QColor(0, 0, 0, 220));
        widget->setGraphicsEffect(shadow);
    };

    QHBoxLayout *topLayout = new QHBoxLayout();



    // 👇【核心新增】：给单人模式加一个“退出”按钮
    QPushButton *exitBtn = new QPushButton("🔙 退出", boardPanel);
    exitBtn->setCursor(Qt::PointingHandCursor);
    exitBtn->setStyleSheet(R"(
        QPushButton {
            background-color: rgba(255, 255, 255, 20);
            color: #ffffff;
            border-radius: 15px;
            font-size: 14px;
            font-weight: bold;
            padding: 6px 15px;
            border: 1px solid rgba(255, 255, 255, 50);
        }
        QPushButton:hover {
            background-color: rgba(255, 255, 255, 50);
            border: 1px solid #ffffff;
        }
    )");

    // 绑定点击事件，调用你之前写好的 UIHelper 弹窗
    connect(exitBtn, &QPushButton::clicked, this, [this](){
        UIHelper::showCustomPopup(this, "🚪 中断演出", "确定要中断当前的演出并返回大厅吗？\n当前的得分将不会被记录。", true, [this](){
            this->onReturnClicked(); // 调用已有的返回逻辑，它会自动处理BGM和路由
        });
    });

    // 将按钮放在最左边
    topLayout->addWidget(exitBtn);
    // 👇【修改 1】：把原本的 addSpacing(20) 改成 10，或者直接删掉这行
    topLayout->addSpacing(10);


    m_infoLabel = new QLabel(QString("🎸 %1 | 单人").arg(m_session.nickname));
    m_infoLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #ffffff;");
    applyTextShadow(m_infoLabel);

    QHBoxLayout *targetLayout = new QHBoxLayout();
    targetLayout->setSpacing(5);// 这个控制的是星星和星星之间的间距

    m_levelTargetLabel = new QLabel("🎯 目标: 0");

    m_levelTargetLabel->setMinimumWidth(160);

    m_levelTargetLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #FFD700;");
    applyTextShadow(m_levelTargetLabel);
    targetLayout->addWidget(m_levelTargetLabel);

    targetLayout->addSpacing(15);

    for (int i = 0; i < 3; ++i) {
        m_starLabels[i] = new QLabel("☆");
        m_starLabels[i]->setFixedSize(30, 30);
        m_starLabels[i]->setAlignment(Qt::AlignCenter);
        m_starLabels[i]->setStyleSheet("font-size: 24px; color: rgba(255,255,255,200);");
        applyTextShadow(m_starLabels[i]);
        targetLayout->addWidget(m_starLabels[i]);
    }

    m_movesLabel = new QLabel("⏳ 步数: 20");
    m_movesLabel->setStyleSheet("font-size: 18px; color: #00FF7F; font-weight: bold;");
    applyTextShadow(m_movesLabel);

    m_scoreLabel = new QLabel("🎵 当前得分: 0");
    m_scoreLabel->setStyleSheet("font-size: 18px; font-weight: 900; color: #ffb6c1;");
    applyTextShadow(m_scoreLabel);

    topLayout->addWidget(m_infoLabel); topLayout->addStretch();
    topLayout->addLayout(targetLayout); topLayout->addStretch();
    topLayout->addWidget(m_movesLabel); topLayout->addStretch();
    topLayout->addWidget(m_scoreLabel);

    panelLayout->addLayout(topLayout);
    panelLayout->addWidget(m_gamePanel, 0, Qt::AlignHCenter);

    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(boardPanel, 0, Qt::AlignCenter);
}

// ==========================================
// AI 对战 UI (分屏竞速专属)
// ==========================================
void MainWindow::setupAIBattleUI(QWidget *centralWidget) {
    auto applyTextShadow = [](QWidget* widget) {
        QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect(widget);
        shadow->setOffset(1, 2); shadow->setBlurRadius(5); shadow->setColor(QColor(0, 0, 0, 220));
        widget->setGraphicsEffect(shadow);
    };

    QString panelQss = R"(
        QFrame {
            background-color: rgba(0, 0, 0, 80);
            border-radius: 20px;
            border: 1px solid rgba(255, 255, 255, 40);
        }
        QLabel { color: white; background: transparent; font-family: "Microsoft YaHei", "Segoe UI"; }
    )";

    // -- 玩家面板 (仍然叫 boardPanel 以兼容你的特效函数) --
    QFrame *playerPanel = new QFrame(centralWidget);
    playerPanel->setObjectName("boardPanel");
    playerPanel->setFixedSize(820, 780);
    playerPanel->setStyleSheet(panelQss);

    QVBoxLayout *pLayout = new QVBoxLayout(playerPanel);
    pLayout->setContentsMargins(30, 30, 30, 30);
    pLayout->setSpacing(20);

    QHBoxLayout *pTop = new QHBoxLayout();
    m_infoLabel = new QLabel(QString("🎸 %1").arg(m_session.nickname));
    m_infoLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #FFD700;");
    applyTextShadow(m_infoLabel);

    m_scoreLabel = new QLabel("🎵 得分: 0");
    m_scoreLabel->setStyleSheet("font-size: 18px; font-weight: 900; color: #ffb6c1;");
    applyTextShadow(m_scoreLabel);

    // AI模式公用目标分数，不显示步数(纯竞速)
    m_levelTargetLabel = new QLabel("🎯 目标: 0");
    m_levelTargetLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #FFD700;");
    applyTextShadow(m_levelTargetLabel);

    // 初始化防空指针（即使隐藏也保留引用，防止 updateScoreLabel 报错）
    m_movesLabel = new QLabel();
    for(int i=0; i<3; ++i) m_starLabels[i] = new QLabel();

    pTop->addWidget(m_infoLabel);
    pTop->addStretch();
    pTop->addWidget(m_levelTargetLabel);
    pTop->addStretch();
    pTop->addWidget(m_scoreLabel);

    pLayout->addLayout(pTop);
    pLayout->addWidget(m_gamePanel, 0, Qt::AlignHCenter);

    // -- AI 面板 --
    QFrame *aiPanel = new QFrame(centralWidget);
    aiPanel->setObjectName("aiBoardPanel");
    aiPanel->setFixedSize(820, 780);
    aiPanel->setStyleSheet(panelQss);

    QVBoxLayout *aLayout = new QVBoxLayout(aiPanel);
    aLayout->setContentsMargins(30, 30, 30, 30);
    aLayout->setSpacing(20);

    QHBoxLayout *aTop = new QHBoxLayout();
    //QLabel *aiInfo = new QLabel("🤖 强力机器");
    // 👈 修改这行：不再使用局部变量 QLabel *aiInfo，而是赋值给成员变量 m_aiInfoLabel
    QString defaultName = (m_currentMode == GameMode::Online) ? "等待对手..." : "🤖 强力机器";
    m_aiInfoLabel = new QLabel(defaultName);
    m_aiInfoLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #00FFFF;");
    applyTextShadow(m_aiInfoLabel);

    m_aiScoreLabel = new QLabel("🎵 得分: 0");
    m_aiScoreLabel->setStyleSheet("font-size: 18px; font-weight: 900; color: #ffb6c1;");
    applyTextShadow(m_aiScoreLabel);

    aTop->addWidget(m_aiInfoLabel);
    aTop->addStretch();
    aTop->addWidget(m_aiScoreLabel);

    aLayout->addLayout(aTop);
    aLayout->addWidget(m_aiPanel, 0, Qt::AlignHCenter);

    // -- 拼装大舞台 --
    QHBoxLayout *mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setContentsMargins(50, 20, 50, 20);
    mainLayout->setSpacing(30);

    QVBoxLayout *vsLayout = new QVBoxLayout();
    QFrame *vLineTop = new QFrame(); vLineTop->setFrameShape(QFrame::VLine); vLineTop->setStyleSheet("border: 2px dashed rgba(255,105,180, 150);");
    QLabel *vsLabel = new QLabel("VS"); vsLabel->setStyleSheet("font-size: 60px; font-weight: 900; color: #FFD700; font-style: italic;"); applyTextShadow(vsLabel);
    QFrame *vLineBottom = new QFrame(); vLineBottom->setFrameShape(QFrame::VLine); vLineBottom->setStyleSheet("border: 2px dashed rgba(255,105,180, 150);");


    // 👇 1. 重新创建取消建房和认输按钮（换成更圆润的尺寸）
    m_cancelWaitBtn = new QPushButton("🔙 取消等待", centralWidget);
    m_surrenderBtn = new QPushButton("🏳️ 拔线认输", centralWidget);

    // 🌟 霓虹粉样式（取消等待）
    QString pinkNeonQss = R"(
        QPushButton {
            background-color: rgba(255, 105, 180, 30);
            color: #ffffff;
            border-radius: 20px;
            font-size: 18px;
            font-weight: bold;
            padding: 12px 25px;
            border: 2px solid #ff69b4;
        }
        QPushButton:hover {
            background-color: #ff69b4;
            border: 2px solid #ffffff;
        }
        QPushButton:pressed { background-color: #c71585; }
    )";

    // 🌟 警示红样式（认输按钮）- 完美融入面板
    QString redNeonQss = R"(
        QPushButton {
            background-color: rgba(0, 0, 0, 80); /* 👈 与游戏面板完全一致的通透暗度 */
            color: #ff4500;
            border-radius: 20px;
            font-size: 18px;
            font-weight: bold;
            padding: 12px 25px;
            border: 1px solid rgba(255, 69, 0, 80); /* 微光边框 */
        }
        QPushButton:hover { background-color: rgba(255, 69, 0, 40); border: 1px solid #ff4500; }
        QPushButton:pressed { background-color: rgba(255, 69, 0, 80); color: white; }
    )";

    m_cancelWaitBtn->setStyleSheet(pinkNeonQss); m_cancelWaitBtn->setCursor(Qt::PointingHandCursor);
    m_surrenderBtn->setStyleSheet(redNeonQss);   m_surrenderBtn->setCursor(Qt::PointingHandCursor);

    // 👇 修改：如果是纯 AI 对战，直接亮出认输按钮！
    if (m_currentMode == GameMode::AI) {
        m_surrenderBtn->show();
    } else {
        m_surrenderBtn->hide(); // 联机模式依然保持隐藏，等连上人再显示
    }

    if (m_currentMode != GameMode::Online) m_cancelWaitBtn->hide();

    // 把按钮塞进 VS 布局的中间
    vsLayout->addWidget(vLineTop, 1, Qt::AlignHCenter);
    vsLayout->addWidget(vsLabel, 0, Qt::AlignHCenter);
    vsLayout->addWidget(m_cancelWaitBtn, 0, Qt::AlignHCenter); // 👈 加在这里
    vsLayout->addWidget(m_surrenderBtn, 0, Qt::AlignHCenter);  // 👈 加在这里
    vsLayout->addWidget(vLineBottom, 1, Qt::AlignHCenter);

    mainLayout->addWidget(playerPanel, 0, Qt::AlignRight | Qt::AlignVCenter);
    mainLayout->addLayout(vsLayout);
    mainLayout->addWidget(aiPanel, 0, Qt::AlignLeft | Qt::AlignVCenter);
}


void MainWindow::loadLevel(int index) {
    if (index >= m_levels.size()) return;

    // 👇【新增】：恢复全局背景音乐
    QSoundEffect *bgm = this->window()->findChild<QSoundEffect*>();
    if (bgm && !bgm->isPlaying()) {
        bgm->play();
    }


    m_isBonusTime = false; // 【新增】：新关卡一定要重置开关！


    // 👇【加上这行救命代码】：确保新关卡开始时，彻底关闭硬件透明，让 QSS 重新接管背景绘制！
    QFrame* boardPanel = centralWidget()->findChild<QFrame*>("boardPanel");
    if (boardPanel) {
        boardPanel->setAttribute(Qt::WA_TranslucentBackground, false);
    }



    m_currentLevelIndex = index;
    LevelConfig cfg = m_levels[index];

    m_starThresholds[0] = cfg.targetScore * 0.3;
    m_starThresholds[1] = cfg.targetScore * 0.6;
    m_starThresholds[2] = cfg.targetScore;




    // 2. 如果是对战模式（AI或网络），修改这份【拷贝】的目标分数
    // 👇 修改：加上网络模式的判断
    if (m_currentMode == GameMode::AI || m_currentMode == GameMode::Online) {
        cfg.targetScore = qMax(cfg.targetScore * 3, 3000);
    }

    // 👇 1. 动态决定步数：如果是 AI 模式或网络模式，直接给 999 步！否则保持原关卡步数
    // 👇 修改：加上网络模式的判断
    int maxMoves = (m_currentMode == GameMode::AI || m_currentMode == GameMode::Online) ? 999 : cfg.maxMoves;

    // 👇 2. 把算好的 maxMoves 传给玩家！
    m_logic->startLevel(m_session.uid, m_currentMode, cfg.targetScore, maxMoves);


    // 👇 修改为：如果是 AI 模式 或 网络联机模式，都要启动右侧对手的引擎！
    if ((m_currentMode == GameMode::AI || m_currentMode == GameMode::Online) && m_aiLogic) {
        m_aiScoreLabel->setText("🎵 得分: 0");
        m_aiLogic->startLevel(0, m_currentMode, cfg.targetScore, maxMoves);

        // 👇 重点：只有纯 AI 模式才需要定时器触发机器人自己下棋！
        // 网络模式的移动指令是靠对面真实玩家发包传过来的，不需要自动触发！
        if (m_currentMode == GameMode::AI) {
            QTimer::singleShot(2500, m_aiLogic, &GameLogic::triggerNextBotMove);
        }
    }


    // 装载对应关卡的 MV
    m_mediaPlayer->setMedia(QUrl::fromLocalFile(cfg.mvPath));


     m_videoView->hide();

    m_baseBgLabel->show(); // 确保静态背景图恢复显示

    // === 【核心修复：背景传承逻辑】 ===
    // 1. 设置底层背景（如果这是第一关，底层就是默认的舞台；如果是后续关卡，底层就是上一关解锁的图！）
    if (index == 0) {
        m_baseBgPixmap.load(":/res/backgrounds/stage_bg.png");
    } else {
        m_baseBgPixmap.load(m_levels[index - 1].bgPath);
    }

    // 2. 设置顶层的目标背景
    m_targetBgPixmap.load(cfg.bgPath);

    // 触发重绘缩放
    QResizeEvent re(size(), size());
    resizeEvent(&re);

    m_levelTargetLabel->setText(QString("🎯 目标: %1").arg(cfg.targetScore));
    updateScoreLabel(0);
}

void MainWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);

    if (!m_baseBgPixmap.isNull()) {
        m_baseBgLabel->setGeometry(0, 0, width(), height());
        m_baseBgLabel->setPixmap(m_baseBgPixmap.scaled(size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    }

    if (!m_targetBgPixmap.isNull()) {
        m_scaledBg = m_targetBgPixmap.scaled(size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        m_bgRevealLabel->setGeometry(0, 0, width(), height());
        m_bgRevealLabel->setPixmap(m_scaledBg);
        updateScoreLabel(m_logic->getCurrentScore());
    }

    if (m_resultOverlay) m_resultOverlay->setGeometry(0, 0, width(), height());

    // 👇 新增：确保视频永远撑满全屏
    if (m_videoView) {
        m_videoView->setGeometry(0, 0, width(), height());
        m_videoScene->setSceneRect(0, 0, width(), height());
        m_videoItem->setSize(QSizeF(width(), height()));
    }
}

void MainWindow::updateScoreLabel(int score) {
    m_scoreLabel->setText(QString("🎵 当前得分: %1").arg(score));

    // =======================================================
    // 🚨 终极防错防御：不查表，直接问引擎现在的满分是多少！
    // =======================================================
    int target = m_logic->getTargetScore();
    if (target <= 0) target = 1; // 物理防御，防止除以 0

    // 实时计算当前局的三星及格线（抛弃容易错位的 m_starThresholds 数组）
    int currentThresholds[3] = { (int)(target * 0.3), (int)(target * 0.6), target };


    // ==========================================
    // 💥 终极物理拦截：狂欢时间强制透明并退出
    // ==========================================
    if (m_isBonusTime) {
        QFrame* boardPanel = centralWidget()->findChild<QFrame*>("boardPanel");
        if (boardPanel) {
            boardPanel->setStyleSheet(R"(
                #boardPanel { background: none; border: none; }
                QLabel { color: white; background: transparent; font-family: "Microsoft YaHei", "Segoe UI"; }
            )");

            // ❌ 把下面这行代码彻底删掉！我们现在换了视频引擎，根本不需要它了！
            // boardPanel->setAttribute(Qt::WA_TranslucentBackground, true);

        }
        m_bgRevealLabel->hide();
        m_baseBgLabel->hide();

        for (int i = 0; i < 3; ++i) {
            if (score >= m_starThresholds[i]) {
                m_starLabels[i]->setText("★");
                m_starLabels[i]->setStyleSheet("font-size: 28px; color: #FFD700; background: transparent; font-weight: bold;");
            }
        }
        return; // 💥 直接退出，保持 MV 播放时的全透明
    }

    // 👇【核心修复 2】：不要去原始的 m_levels 数组里拿了！
    // 删掉这行: int target = m_levels[m_currentLevelIndex].targetScore;


    double percent = qBound(0.0, (double)score / target, 1.0);

    // ==========================================
    // 视觉特效 1：完美融合版 (开场平滑显现 + 得分黑雾变淡 + 边框同步渐变)
    // ==========================================
    int targetBgAlpha = 120 - (int)(percent * 80); // 背景目标透明度 (120 -> 40)
    int targetBorderAlpha = 40;                    // 边框的最终透明度

    QFrame* boardPanel = centralWidget()->findChild<QFrame*>("boardPanel");
    if (boardPanel) {
        QVariantAnimation* oldAnim = boardPanel->findChild<QVariantAnimation*>("alphaAnim");
        int startBgAlpha = targetBgAlpha;

        if (oldAnim) {
            startBgAlpha = oldAnim->currentValue().toInt(); // 继承当前动画进度
            oldAnim->stop();
            oldAnim->deleteLater();
        } else if (score == 0) {
            // 👇【核心修复】：新关卡开始时，让它从 0 (完全透明) 开始渐变！
            startBgAlpha = 0;
        }

        QVariantAnimation *alphaAnim = new QVariantAnimation(boardPanel);
        alphaAnim->setObjectName("alphaAnim");
        alphaAnim->setDuration(score == 0 ? 800 : 150); // 开场动画慢一点(800ms)，平时得分快一点(150ms)
        alphaAnim->setStartValue(startBgAlpha);
        alphaAnim->setEndValue(targetBgAlpha);
        alphaAnim->setEasingCurve(score == 0 ? QEasingCurve::InOutQuad : QEasingCurve::Linear);

        // 👇【完美融合】：同时计算背景和边框的透明度，写入同一个 QSS
        connect(alphaAnim, &QVariantAnimation::valueChanged, [boardPanel, targetBorderAlpha, score](const QVariant &value){
            int bgAlpha = value.toInt();
            int borderAlpha = targetBorderAlpha;

            // 如果是开场动画(score==0)，让边框也跟随背景的进度从 0 慢慢变到 40
            if (score == 0) {
                double animProgress = (double)bgAlpha / 120.0; // 计算动画进度 0.0 ~ 1.0
                borderAlpha = (int)(targetBorderAlpha * animProgress);
            }

            boardPanel->setStyleSheet(QString(R"(
                #boardPanel {
                    background-color: rgba(0, 0, 0, %1);
                    border-radius: 20px;
                    border: 1px solid rgba(255, 255, 255, %2);
                }
                QLabel { color: white; background: transparent; font-family: "Microsoft YaHei", "Segoe UI"; }
            )").arg(bgAlpha).arg(borderAlpha)); // %1 填入背景透明度，%2 填入边框透明度
        });

        connect(alphaAnim, &QVariantAnimation::finished, alphaAnim, &QObject::deleteLater);
        alphaAnim->start();
    }

    // ==========================================
    // 视觉特效 2：背景图从底部升起，作为主进度条
    // ==========================================
    if (!m_scaledBg.isNull()) {
        if (score >= target) {
            m_bgRevealLabel->hide();
            m_baseBgLabel->hide();
        } else {
            int revealHeight = m_scaledBg.height() * percent;
            if (revealHeight > 0) {
                m_bgRevealLabel->setMask(QRegion(0, this->height() - revealHeight, this->width(), revealHeight));
                m_bgRevealLabel->show();
                if (boardPanel) boardPanel->raise(); // 保持游戏面板在最顶层
            } else {
                m_bgRevealLabel->hide();
            }
        }
    }

    // 更新星星
    for (int i = 0; i < 3; ++i) {
        if (score >= m_starThresholds[i]) {
            m_starLabels[i]->setText("★");
            m_starLabels[i]->setStyleSheet("font-size: 28px; color: #FFD700; background: transparent; font-weight: bold;");
        } else {
            m_starLabels[i]->setText("☆");
            m_starLabels[i]->setStyleSheet("font-size: 24px; color: rgba(255,255,255,200); background: transparent;");
        }
    }
}

void MainWindow::updateComboLabel(int combo) {
    if (combo < 2) return;

    QFrame* panel = centralWidget()->findChild<QFrame*>("boardPanel");
    if (!panel) return;

    // 创建独立的标签用于动画
    QLabel *comboText = new QLabel(QString("%1\nCOMBO!").arg(combo), centralWidget());
    comboText->setAlignment(Qt::AlignCenter);
    comboText->setStyleSheet(R"(
        font-family: 'Impact', 'Arial Black', sans-serif;
        font-size: 50px;
        font-weight: 900;
        font-style: italic;
        color: #FFea00;
        background: transparent;
    )");

    // 【特效调整】：在棋盘面板的左上角外部生成
    int startX = panel->x() - 140 + (rand() % 40);
    int startY = panel->y() + 50 + (rand() % 40);
    comboText->setGeometry(startX, startY, 250, 150);
    comboText->show();

    QParallelAnimationGroup *group = new QParallelAnimationGroup(comboText);

    // 动画：向极左上方滑出
    QPropertyAnimation *move = new QPropertyAnimation(comboText, "pos");
    move->setDuration(1800);
    move->setStartValue(QPoint(startX, startY));
    move->setEndValue(QPoint(startX - 100, startY - 120));
    move->setEasingCurve(QEasingCurve::OutExpo);

    // 动画：淡出消失，滞空时间加长
    QGraphicsOpacityEffect *eff = new QGraphicsOpacityEffect(comboText);
    comboText->setGraphicsEffect(eff);
    QPropertyAnimation *fade = new QPropertyAnimation(eff, "opacity");
    fade->setDuration(1800);
    fade->setStartValue(1.0);
    fade->setKeyValueAt(0.7, 1.0); // 70%的时间内完全清晰
    fade->setEndValue(0.0);

    group->addAnimation(move);
    group->addAnimation(fade);

    // 动画播完自动销毁，防止内存溢出
    connect(group, &QAnimationGroup::finished, comboText, &QObject::deleteLater);
    group->start();
}
// 替换原有的 initConnections
void MainWindow::initConnections() {
    // 监听步数变动 (保留你原有的逻辑)
    connect(m_logic, &GameLogic::movesUpdated, this, [this](int moves){
        m_movesLabel->setText(QString("⏳ 步数: %1").arg(moves));
        if (moves <= 5) m_movesLabel->setStyleSheet("font-size: 24px; color: #ff4500; font-weight: bold;");
        else m_movesLabel->setStyleSheet("font-size: 20px; color: #00FF7F; font-weight: bold;");
    });

    // 【替换1】：接管结束结算信号
    connect(m_logic, &GameLogic::levelFinished, this, &MainWindow::onLevelFinished);

    connect(m_logic, &GameLogic::targetReached, this, [this](){
        m_isBonusTime = true;

        m_baseBgLabel->hide();
        m_bgRevealLabel->hide();

        m_videoView->show(); // 让咱们的新视频引擎露出来
        m_mediaPlayer->play();

        // 👇【新增】：查找到全局背景音乐，并停止播放
        QSoundEffect *bgm = this->window()->findChild<QSoundEffect*>();
        if (bgm) bgm->stop();

        QFrame* boardPanel = centralWidget()->findChild<QFrame*>("boardPanel");
        if (boardPanel) {
            // 👇【核心修复】：不要一刀切！只精准停掉控制背景黑雾的 "alphaAnim"
            QVariantAnimation* alphaAnim = boardPanel->findChild<QVariantAnimation*>("alphaAnim");
            if (alphaAnim) {
                alphaAnim->stop();
                alphaAnim->deleteLater();
            }

            // 扒掉底板（因为换了软件渲染，这次背景会完美的融合成视频画面！）
            boardPanel->setStyleSheet(R"(
                #boardPanel { background: none; border: none; }
                QLabel { color: white; background: transparent; font-family: "Microsoft YaHei", "Segoe UI"; }
            )");
        }

        // 👇【核心修改】：AI 模式 或 网络模式下赢了之后的专属“狂欢处理”
        if (m_currentMode == GameMode::AI|| m_currentMode == GameMode::Online) {
            // 1. 强杀对手：停掉AI的行动，并把右边的棋盘直接隐藏！
            if (m_aiLogic) m_aiLogic->endAndSaveGame(false);
            QFrame* aiPanel = centralWidget()->findChild<QFrame*>("aiBoardPanel");
            if (aiPanel) aiPanel->hide();

            // 2. 颁发奖励：把你 999 的无限步数改成 10 步，享受个人 Show Time！
            m_logic->setRemainingMoves(10);

            // 3. 顺便把左上角的文字改得炫酷一点
            m_infoLabel->setText("🎸 狂欢时间 (Bonus Time!)");
            m_infoLabel->setStyleSheet("font-size: 20px; font-weight: 900; color: #FFea00;");
        }
    });





    // 👇 【核心修改】：把 AI 专属判断，扩大为 AI 和 网络 都能用
    if ((m_currentMode == GameMode::AI || m_currentMode == GameMode::Online) && m_aiLogic) {
        // AI计分更新
        connect(m_aiLogic, &GameLogic::scoreUpdated, this, [this](int score){
            m_aiScoreLabel->setText(QString("🎵 得分: %1").arg(score));
        });

        // AI算力路由到面板
        connect(m_aiLogic, &GameLogic::aiMoveDecided, m_aiPanel, &GamePanel::onAiMoveDecided);

        // 👇 重点防坑：只有纯 AI 模式才需要动画播完后触发下一步！联机是等真人手速的！
        if (m_currentMode == GameMode::AI) {
            connect(m_aiPanel, &GamePanel::actionFinished, this, [this](){
                if (!m_aiLogic->isGameOver() && !m_logic->isGameOver()) {
                    QTimer::singleShot(m_aiDelay, m_aiLogic, &GameLogic::triggerNextBotMove);
                }
            });
        }

        // 对手（机器或真人）抢先达到目标分 -> 对方胜利
        connect(m_aiLogic, &GameLogic::targetReached, this, [this](){
            m_aiLogic->endAndSaveGame(true); // 🚨 强杀后台计算
            m_logic->endAndSaveGame(false);  // 判定我方失败
            onLevelFinished(false);          // 弹出失败面板
        });
    }

    // ==========================================
    // 👇 新增：AI 模式专属的认输功能绑定
    // ==========================================
    if (m_currentMode == GameMode::AI && m_surrenderBtn) {
        connect(m_surrenderBtn, &QPushButton::clicked, this, [this](){
            // 弹出极具赛博朋克感的确认框
            UIHelper::showCustomPopup(this, "🏳️ 终止演奏", "确定要放弃这场人机对决吗？\n机器的算力确实令人畏惧...", true, [this](){
                if (m_aiLogic) {
                    m_aiLogic->endAndSaveGame(true); // 让 AI 判定为赢
                }
                m_logic->endAndSaveGame(false); // 强制判自己输

                // 停止游戏并弹出结算面板
                onLevelFinished(false);
            });
        });
    }


    // ==========================================
    // 👇 新增：绑定魔力鸟三段式神级特效信号
    // ==========================================
    connect(m_logic, &GameLogic::magicBirdTriggered,
            m_gamePanel, &GamePanel::playMagicBirdAbsorbEffect);

    // 如果是对战模式（右侧也有一个镜像或AI面板），也给右侧绑上特效！
    if (m_aiLogic && m_aiPanel) {
        connect(m_aiLogic, &GameLogic::magicBirdTriggered,
                m_aiPanel, &GamePanel::playMagicBirdAbsorbEffect);
    }


}
// 槽函数实现
void MainWindow::onLevelFinished(bool isWin) {
    m_mediaPlayer->stop(); // 游戏结束，停止视频
    m_videoView->hide();

    // 1. 设置标题
    if (isWin) {
        m_resultTitle->setText(m_currentLevelIndex < m_levels.size() - 1 ? "✨ 演出大成功 ✨" : "🏆 巡演完美收官！");
        m_resultTitle->setStyleSheet("font-size: 40px; color: #00FF7F; font-weight: 900; background: transparent;");
    } else {
        m_resultTitle->setText("🌧 演出中断...");
        m_resultTitle->setStyleSheet("font-size: 40px; color: #FF4500; font-weight: 900; background: transparent;");
    }

    // 2. 按钮显隐逻辑分流
    if (m_currentMode == GameMode::Online) {
        // 网络模式：点赞 + 截图
        m_nextBtn->setText("👍 为对手点赞");
        m_nextBtn->setEnabled(true);
        m_nextBtn->show();
        m_restartBtn->setText("📷 演出留影");
        m_restartBtn->show();
    }
    else if (m_currentMode == GameMode::AI) {
        // 👇【核心修改】：人机模式无论输赢，都隐藏“下一场演出”按钮
        m_nextBtn->hide();
        m_restartBtn->setText("🔄 重新开始");
        m_restartBtn->show();
    }
    else {
        // 单人闯关模式：逻辑不变，赢了且有下一关才显示
        m_nextBtn->setText("🎸 下一场演出");
        m_restartBtn->setText("🔄 重新开始");
        m_restartBtn->show();

        if (isWin && m_currentLevelIndex < m_levels.size() - 1) {
            m_nextBtn->show();
        } else {
            m_nextBtn->hide();
        }
    }

    m_resultScore->setText(QString("最终得分: %1").arg(m_logic->getCurrentScore()));

    m_resultOverlay->raise(); // 提到最顶层
    m_resultOverlay->show();
}


// 初始化结算面板
void MainWindow::setupResultPanel() {
    m_resultOverlay = new QFrame(this);
    m_resultOverlay->setStyleSheet("background-color: rgba(0, 0, 0, 210);"); // 黑暗半透明遮罩
    m_resultOverlay->hide();

    QFrame *panel = new QFrame(m_resultOverlay);
    panel->setFixedSize(450, 500);
    panel->setStyleSheet(R"(
        background-color: rgba(20, 20, 30, 230);
        border-radius: 20px;
        border: 2px solid #ff69b4;
    )");

    QVBoxLayout *layout = new QVBoxLayout(panel);
    layout->setSpacing(25);
    layout->setContentsMargins(40, 50, 40, 50);

    m_resultTitle = new QLabel("STAGE CLEAR", panel);
    m_resultTitle->setAlignment(Qt::AlignCenter);

    m_resultScore = new QLabel("得分: 0", panel);
    m_resultScore->setAlignment(Qt::AlignCenter);
    m_resultScore->setStyleSheet("font-size: 34px; color: #FFD700; font-weight: 900; background: transparent;");

    QString btnStyle = R"(
        QPushButton {
            background-color: #ff69b4; color: white; border-radius: 12px;
            font-size: 22px; font-weight: bold; padding: 12px;
        }
        QPushButton:hover { background-color: #ff1493; border: 2px solid white; }
    )";

    m_nextBtn = new QPushButton("🎸 下一场演出", panel);
    m_restartBtn = new QPushButton("🔄 重新开始", panel);
    m_returnBtn = new QPushButton("🚪 返回检票口", panel);

    m_nextBtn->setStyleSheet(btnStyle); m_nextBtn->setCursor(Qt::PointingHandCursor);
    m_restartBtn->setStyleSheet(btnStyle); m_restartBtn->setCursor(Qt::PointingHandCursor);
    m_returnBtn->setStyleSheet(btnStyle); m_returnBtn->setCursor(Qt::PointingHandCursor);

    layout->addWidget(m_resultTitle);
    layout->addWidget(m_resultScore);
    layout->addStretch();
    layout->addWidget(m_nextBtn);
    layout->addWidget(m_restartBtn);
    layout->addWidget(m_returnBtn);

    QVBoxLayout *mainLayout = new QVBoxLayout(m_resultOverlay);
    mainLayout->addWidget(panel, 0, Qt::AlignCenter);

    connect(m_nextBtn, &QPushButton::clicked, this, &MainWindow::onNextLevelClicked);
    connect(m_restartBtn, &QPushButton::clicked, this, &MainWindow::onRestartClicked);
    connect(m_returnBtn, &QPushButton::clicked, this, &MainWindow::onReturnClicked);
}

void MainWindow::onNextLevelClicked() {
    // 👇 网络模式下，变成真实的发送点赞功能
    if (m_currentMode == GameMode::Online) {
        m_netManager->sendLike(); // 👈 核心：通过网络把赞发给对方！

        // 弹窗提示自己发送成功
        UIHelper::showCustomPopup(this, "👍 点赞发送", "已向对方发送了点赞！\n吉他英雄惺惺相惜！", false, [this](){
            m_nextBtn->setEnabled(false);
            m_nextBtn->setText("已点赞 ✔");
        });
        return; // 拦截执行
    }

    m_resultOverlay->hide();
    loadLevel(m_currentLevelIndex + 1);
}

void MainWindow::onRestartClicked() {
    // 👇 新增拦截：网络模式下，变身截图留影功能！
    if (m_currentMode == GameMode::Online) {
        // 👇 真·截图逻辑
        QPixmap pixmap = this->grab(); // 截取整个 MainWindow 窗口

        // 获取系统的“图片”文件夹路径，获取不到就放程序当前目录
        QString path = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
        if (path.isEmpty()) path = QDir::currentPath();

        // 按时间生成文件名，例如 Bocchi_Show_20260430_112045.png
        QString fileName = path + QString("/Bocchi_Show_%1.png").arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));

        bool success = pixmap.save(fileName); // 保存到硬盘

        QString msg = success ? QString("演出实况已真实保存至：\n%1").arg(fileName)
                              : "截图保存失败，请检查文件权限！";

        UIHelper::showCustomPopup(this, "📷 演出留影", msg, false, [this](){
            m_restartBtn->setEnabled(false);
            m_restartBtn->setText("已保存 ✔");
        });
        return;
    }



    m_resultOverlay->hide();
    loadLevel(m_currentLevelIndex);
}

void MainWindow::onReturnClicked() {
    m_mediaPlayer->stop();
    // 👇【新增】：返回大厅前，恢复全局背景音乐
    QSoundEffect *bgm = this->window()->findChild<QSoundEffect*>();
    if (bgm && !bgm->isPlaying()) {
        bgm->play();
    }
    // 👇【核心修改】：智能判断该退回哪里
    if (m_currentMode == GameMode::Online) {
        emit returnToRadarRequested(); // 在线模式，退回雷达并重新扫描
    } else {
        emit returnToLobbyRequested(); // 单人/AI模式，退回主菜单
    }
}


// ==============================================================
// 🌐 网络对战专属逻辑实现部分
// ==============================================================

void MainWindow::initOnlineConnections() {
    if (!m_netManager) return;


    // =====================================
    // 1. 取消等待按钮：复用已有的返回逻辑
    // =====================================
    if (m_cancelWaitBtn) {
        connect(m_cancelWaitBtn, &QPushButton::clicked, this, [this](){
            // 断开网络后再返回，防止触发 disconnected 信号导致的“判负”弹窗
            if (m_netManager) {
                m_netManager->disconnect();
                m_netManager->deleteLater();
                m_netManager = nullptr;
            }
            this->onReturnClicked(); // 执行停止视频、恢复BGM并退回大厅
        });
    }

    // =====================================
    // 2. 掉线立即判负逻辑
    // =====================================
    connect(m_netManager, &NetworkManager::disconnected, this, [this](){
        // 判定：只有在游戏已经开始（认输按钮可见）且 结果面板还没弹出来时，断线才算输
        if (m_surrenderBtn && m_surrenderBtn->isVisible() && m_resultOverlay && !m_resultOverlay->isVisible()) {

            // 👇 替换在这里：调用 UIHelper
            UIHelper::showCustomPopup(this, "🔌 信号中断", "与对方失去连接！\n由于演出尚未结束，判定本场演出中断。", false, [this](){
                if (m_aiLogic) m_aiLogic->endAndSaveGame(false);
                m_logic->endAndSaveGame(false); // 强判自己输
                onLevelFinished(false);         // 弹结算框
            });

        }
    });


    // =====================================
    // 3. 认输功能实现
    // =====================================
    if (m_surrenderBtn) {
        connect(m_surrenderBtn, &QPushButton::clicked, this, [this](){

            // 👇 替换在这里：调用 UIHelper，注意第三个参数是 true（显示确认/取消按钮）
            UIHelper::showCustomPopup(this, "🏳️ 拔线认输", "确定要提前结束这场演出吗？\n你将被判定为失败！", true, [this](){
                m_netManager->sendSurrender();
                m_logic->endAndSaveGame(false);
                onLevelFinished(false);
            });

        });
    }

    // =====================================
    // 4. 接收对方认输的信号
    // =====================================
    connect(m_netManager, &NetworkManager::opponentSurrendered, this, [this](){

        // 👇 替换在这里：调用 UIHelper
        UIHelper::showCustomPopup(this, "🏆 演出不战而胜", "🎸 对方吉他弦断了（已离场）！\n恭喜你直接获得本场胜利！", false, [this](){
            if (m_aiLogic) m_aiLogic->endAndSaveGame(false);
            m_logic->endAndSaveGame(true); // 强判自己赢
            onLevelFinished(true);
        });

    });

    // =====================================
    // 5. 接收对方发来的点赞！
    // =====================================
    connect(m_netManager, &NetworkManager::opponentLiked, this, [this](){
        // 收到对方的赞，用你的霓虹 UI 弹个炫酷的提示框
        UIHelper::showCustomPopup(this, "💖 收到点赞", "对方玩家觉得你的演出太棒了，\n向你发送了一个赞 👍！", false, nullptr);
    });

    // 1. 联机成功后，房主负责发牌（生成种子并发送）
    connect(m_netManager, &NetworkManager::connectedToOpponent, this, [this](){
        if (m_netManager->isHost()) {
            // 👇 修复这里：不要用时间戳，直接生成一个 1 到 9999999 的安全整数
            quint32 seed = QRandomGenerator::global()->bounded(1, 9999999);
            // 发送我的昵称给对方
            m_netManager->sendGameStart(seed, m_session.nickname);
            // 房主本地显示名暂时不用改（因为右边本来就是等对方），或者直接设为"等待中"
            startGameWithSeed(seed, "连接中...");
        }
    });

    // 2. 客机接收到房主的开局指令
    connect(m_netManager, &NetworkManager::gameStartReceived, this, [this](quint32 seed, QString opponentName){
        // 关键：在这里拿到了对方的真名！
        startGameWithSeed(seed, opponentName);

        // 如果我是客机，我还没给房主发过我的名字，这里可以补发一个确认包或者让房主也监听 scoreUpdate
        // 简单做法：客机连上后也回发一个包含自己名字的 start 包（此时种子不重要）
        if (!m_isHost) {
            m_netManager->sendGameStart(0, m_session.nickname);
        }
    });

    // 3. 我方操作同步给对方
    // 监听本地逻辑的交换信号，发送给网络（注：如果GameLogic还没写 playerSwapped 信号，需要去补上）
    connect(m_logic, &GameLogic::playerSwapped, this, [this](QPoint p1, QPoint p2){
        m_netManager->sendSwapAction(p1, p2);
    });

    // // 我方分数同步给对方
    // connect(m_logic, &GameLogic::scoreUpdated, this, [this](int score){
    //     m_netManager->sendScoreUpdate(score);
    // });

    // 4. 接收对方的操作，让右侧（对手）面板动起来
    connect(m_netManager, &NetworkManager::opponentSwapped, this, [this](QPoint p1, QPoint p2){
        if(m_aiPanel) {
            // 复用你写好的 AI 面板走步函数
            m_aiPanel->onAiMoveDecided(p1, p2);
        }
    });

    // 接收对方的分数更新
    connect(m_netManager, &NetworkManager::opponentScoreUpdated, this, [this](int score){
        if(m_aiScoreLabel) { // 假设你右侧面板的计分板叫 m_aiScoreLabel
            m_aiScoreLabel->setText(QString("🎵 对手得分: %1").arg(score));
        }
    });



}

void MainWindow::startGameWithSeed(quint32 seed, const QString &name) {
    // 1. 更新 UI 上的对手名字
    if (m_aiInfoLabel) {
        m_aiInfoLabel->setText("🌐 " + name);
    }

    // 2. 如果收到有效的种子（不为0），则同步棋盘并开局
    if (seed != 0) {
        // 给自己的逻辑层和对手的镜像逻辑层设置相同的种子
        m_logic->setRandomSeed(seed);
        m_aiLogic->setRandomSeed(seed);

        // 👇 新增这两行：开局后隐藏返回按钮，亮出认输按钮！
        if (m_cancelWaitBtn) m_cancelWaitBtn->hide();
        if (m_surrenderBtn) m_surrenderBtn->show();

        // 👇【核心修改】：利用双方绝对一致的种子，算出同一个随机关卡！
        m_currentLevelIndex = seed % m_levels.size();
        loadLevel(m_currentLevelIndex);

       qDebug() << ">>> 联机对战正式开始，随机数种子：" << seed << "，关卡：" << m_currentLevelIndex;
    }
}
