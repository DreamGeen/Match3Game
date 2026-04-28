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


MainWindow::MainWindow(UserSession session, GameMode mode, QWidget *parent)
    : QMainWindow(parent), m_session(session), m_currentMode(mode)
{
    setWindowTitle("Bocchi Clear! - 全国巡演模式");

    m_logic = new GameLogic(GameConfig::BOARD_ROWS, GameConfig::BOARD_COLS, this);
    m_gamePanel = new GamePanel(m_logic, this);

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

    setupUI();
    setupResultPanel(); // 【新增】初始化结算界面
    initConnections();

    // 默认从第一关开始加载
    loadLevel(0);

    // 连接逻辑层更新信号
    connect(m_logic, &GameLogic::scoreUpdated, this, &MainWindow::updateScoreLabel);
    connect(m_logic, &GameLogic::comboUpdated, this, &MainWindow::updateComboLabel);
}


void MainWindow::setupUI() {
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    centralWidget->setObjectName("mainContainer");
    centralWidget->setStyleSheet("background: transparent;");

    // 👇 1. 全新的软件级视频合成引擎 (完美支持透明UI层叠)
    m_videoView = new QGraphicsView(centralWidget);
    m_videoView->setStyleSheet("background: transparent; border: none;");
    m_videoView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_videoView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_videoView->setAttribute(Qt::WA_TransparentForMouseEvents, true); // 让鼠标穿透视频层打到方块上
    m_videoView->hide(); // 默认先隐藏

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

    // 👇 2. 静态图片背景层 (依然贴在最底层)
    m_baseBgLabel = new QLabel(centralWidget);
    m_bgRevealLabel = new QLabel(centralWidget);
    m_baseBgLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_bgRevealLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    // 👇 3. 游戏UI面板层
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

    // --- 顶部状态栏排布 ---
    QHBoxLayout *topLayout = new QHBoxLayout();
    QString modeStr = (m_currentMode == GameMode::Single) ? "单人" : "人机";
    m_infoLabel = new QLabel(QString("🎸 %1 | %2").arg(m_session.nickname).arg(modeStr));
    m_infoLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #ffffff;");
    applyTextShadow(m_infoLabel);

    QHBoxLayout *targetLayout = new QHBoxLayout();
    targetLayout->setSpacing(5);

    m_levelTargetLabel = new QLabel("🎯 目标: 0");
    m_levelTargetLabel->setMinimumWidth(160);
    m_levelTargetLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #FFD700;");
    applyTextShadow(m_levelTargetLabel);
    targetLayout->addWidget(m_levelTargetLabel);
    targetLayout->addSpacing(10);

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

    // 用布局把整个游戏框死死居中
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(boardPanel, 0, Qt::AlignCenter);
}


void MainWindow::loadLevel(int index) {
    if (index >= m_levels.size()) return;

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

    m_logic->startLevel(m_session.uid, m_currentMode, cfg.targetScore, cfg.maxMoves);


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

    int target = m_levels[m_currentLevelIndex].targetScore;
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

        QFrame* boardPanel = centralWidget()->findChild<QFrame*>("boardPanel");
        if (boardPanel) {
            // 停掉动画
            QList<QVariantAnimation*> anims = boardPanel->findChildren<QVariantAnimation*>();
            for (QVariantAnimation* anim : anims) {
                anim->stop();
                anim->deleteLater();
            }

            // 扒掉底板（因为换了软件渲染，这次背景会完美的融合成视频画面！）
            boardPanel->setStyleSheet(R"(
                #boardPanel { background: none; border: none; }
                QLabel { color: white; background: transparent; font-family: "Microsoft YaHei", "Segoe UI"; }
            )");
        }
    });
}
// 槽函数实现
void MainWindow::onLevelFinished(bool isWin) {
    m_mediaPlayer->stop(); // 游戏结束，停止视频


     m_videoView->hide();

    if (isWin) {
        if (m_currentLevelIndex < m_levels.size() - 1) {
            m_resultTitle->setText("✨ 演出大成功 ✨");
            m_resultTitle->setStyleSheet("font-size: 40px; color: #00FF7F; font-weight: 900; background: transparent;");
            m_nextBtn->show();
        } else {
            m_resultTitle->setText("🏆 巡演完美收官！");
            m_resultTitle->setStyleSheet("font-size: 40px; color: #FFD700; font-weight: 900; background: transparent;");
            m_nextBtn->hide(); // 已经是最后一关，不显示下一关
        }
    } else {
        m_resultTitle->setText("🌧 演出中断...");
        m_resultTitle->setStyleSheet("font-size: 40px; color: #FF4500; font-weight: 900; background: transparent;");
        m_nextBtn->hide();
    }

    m_resultScore->setText(QString("最终得分: %1").arg(m_logic->getCurrentScore()));

    m_resultOverlay->raise(); // 必须提到最顶层，挡住所有方块
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
    m_resultOverlay->hide();
    loadLevel(m_currentLevelIndex + 1);
}

void MainWindow::onRestartClicked() {
    m_resultOverlay->hide();
    loadLevel(m_currentLevelIndex);
}

void MainWindow::onReturnClicked() {
    m_mediaPlayer->stop();
    emit returnToLobbyRequested(); // 通知主程序切回大厅
}
