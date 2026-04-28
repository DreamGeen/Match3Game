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
        {500,  25, ":/res/backgrounds/level1.png", appPath + "/video/mv1.mp4"},
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

    // === 【新增】：初始化视频播放器，并放在最底层 ===
    m_videoWidget = new QVideoWidget(centralWidget);
    m_videoWidget->hide(); // 默认隐藏，不达标不放视频

    // 👇【新增1：解决未填满屏幕】设置视频缩放模式为：保持比例并扩展铺满全屏（会裁掉多余的黑边）
    m_videoWidget->setAspectRatioMode(Qt::KeepAspectRatioByExpanding);
    // 如果你宁愿画面被拉伸变形也要看到完整 MV，可以改成 Qt::IgnoreAspectRatio

    m_mediaPlayer = new QMediaPlayer(this);
    m_mediaPlayer->setVideoOutput(m_videoWidget);

    // 👇【新增2：解决播完黑屏】监听播放状态，如果播到底了，就把进度条拉回 0 秒重新播！
    connect(m_mediaPlayer, &QMediaPlayer::mediaStatusChanged, [this](QMediaPlayer::MediaStatus status){
        if (status == QMediaPlayer::EndOfMedia) {
            m_mediaPlayer->setPosition(0);
            m_mediaPlayer->play();
        }
    });

    // === 1. 双层背景 ===
    m_baseBgLabel = new QLabel(centralWidget);
    m_bgRevealLabel = new QLabel(centralWidget);


    // 👇【加上这三行核心修复】：让这三个图层像幽灵一样，鼠标直接穿透它们点到方块！
    m_baseBgLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_bgRevealLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_videoWidget->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    m_baseBgLabel->lower();
    m_videoWidget->lower(); // 确保视频在 boardPanel 下方，但在 baseBg 之上

    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // === 2. 游戏主面板（改为浅色磨砂质感！） ===
    QFrame *boardPanel = new QFrame(centralWidget);
    boardPanel->setObjectName("boardPanel");
    boardPanel->setFixedSize(820, 780); // 稍微加宽，给顶部留出充裕空间

    // 【视觉终极版】：高级暗黑高透玻璃（墨镜质感）
    boardPanel->setStyleSheet(R"(
        #boardPanel {
            /* 使用存粹的黑色，透明度调到 80（大约30%不透明），就像一层高级汽车贴膜 */
            background-color: rgba(0, 0, 0, 80);
            border-radius: 20px;
            /* 弃用粗边框，改为极细（1px）、极弱（透明度40）的白线，勾勒出面板边缘即可 */
            border: 1px solid rgba(255, 255, 255, 40);
        }
        QLabel { color: white; background: transparent; font-family: "Microsoft YaHei", "Segoe UI"; }
    )");

    QVBoxLayout *panelLayout = new QVBoxLayout(boardPanel);
    panelLayout->setContentsMargins(30, 30, 30, 30);
    panelLayout->setSpacing(20);

    // === 3. 高品质文字阴影（解决亮色背景看不清字的问题） ===
    auto applyTextShadow = [](QWidget* widget) {
        QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect(widget);
        shadow->setOffset(1, 2);                // 阴影向右下偏移
        shadow->setBlurRadius(5);               // 阴影边缘微模糊
        shadow->setColor(QColor(0, 0, 0, 220)); // 强烈的纯黑投影
        widget->setGraphicsEffect(shadow);
    };

    // === 4. 顶部状态栏排布 ===
    QHBoxLayout *topLayout = new QHBoxLayout();

    QString modeStr = (m_currentMode == GameMode::Single) ? "单人" : "人机";
    m_infoLabel = new QLabel(QString("🎸 %1 | %2").arg(m_session.nickname).arg(modeStr));
    m_infoLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #ffffff;");
    applyTextShadow(m_infoLabel); // <--- 应用纯黑阴影

    QHBoxLayout *targetLayout = new QHBoxLayout();
    targetLayout->setSpacing(5);

    m_levelTargetLabel = new QLabel("🎯 目标: 0");
    // 【关键修复】：给目标分数锁定至少 160 的宽度，彻底防止它挤压、盖住星星！
    m_levelTargetLabel->setMinimumWidth(160);
    m_levelTargetLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #FFD700;");
    applyTextShadow(m_levelTargetLabel);
    targetLayout->addWidget(m_levelTargetLabel);

    targetLayout->addSpacing(10); // 强行拉开一段距离

    for (int i = 0; i < 3; ++i) {
        m_starLabels[i] = new QLabel("☆");
        m_starLabels[i]->setFixedSize(30, 30); // 给每个星星画死固定大小的格子，绝对不越界
        m_starLabels[i]->setAlignment(Qt::AlignCenter);
        m_starLabels[i]->setStyleSheet("font-size: 24px; color: rgba(255,255,255,200);");
        applyTextShadow(m_starLabels[i]); // <--- 星星也加上阴影
        targetLayout->addWidget(m_starLabels[i]);
    }

    m_movesLabel = new QLabel("⏳ 步数: 20");
    m_movesLabel->setStyleSheet("font-size: 18px; color: #00FF7F; font-weight: bold;");
    applyTextShadow(m_movesLabel);

    m_scoreLabel = new QLabel("🎵 当前得分: 0");
    m_scoreLabel->setStyleSheet("font-size: 18px; font-weight: 900; color: #ffb6c1;");
    applyTextShadow(m_scoreLabel);

    // 用 addStretch 撑开所有元素，均匀分布
    topLayout->addWidget(m_infoLabel);
    topLayout->addStretch();
    topLayout->addLayout(targetLayout);
    topLayout->addStretch();
    topLayout->addWidget(m_movesLabel);
    topLayout->addStretch();
    topLayout->addWidget(m_scoreLabel);

    panelLayout->addLayout(topLayout);
    panelLayout->addWidget(m_gamePanel, 0, Qt::AlignHCenter);

    mainLayout->addWidget(boardPanel, 0, Qt::AlignCenter);
}

void MainWindow::loadLevel(int index) {
    if (index >= m_levels.size()) return;

    m_isBonusTime = false; // 【新增】：新关卡一定要重置开关！

    m_currentLevelIndex = index;
    LevelConfig cfg = m_levels[index];

    m_starThresholds[0] = cfg.targetScore * 0.3;
    m_starThresholds[1] = cfg.targetScore * 0.6;
    m_starThresholds[2] = cfg.targetScore;

    m_logic->startLevel(m_session.uid, m_currentMode, cfg.targetScore, cfg.maxMoves);


    // 装载对应关卡的 MV
    m_mediaPlayer->setMedia(QUrl::fromLocalFile(cfg.mvPath));
    m_videoWidget->hide(); // 重置为隐藏
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

    // 1. 铺满底层背景
    if (!m_baseBgPixmap.isNull()) {
        m_baseBgLabel->setGeometry(0, 0, width(), height());
        m_baseBgLabel->setPixmap(m_baseBgPixmap.scaled(size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    }

    // 2. 缩放顶层背景（准备被裁剪）
    if (!m_targetBgPixmap.isNull()) {
        m_scaledBg = m_targetBgPixmap.scaled(size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

        // 👇【核心修复】：让目标背景图永远和窗口一样大，位置死死固定在 (0,0)
        m_bgRevealLabel->setGeometry(0, 0, width(), height());
        m_bgRevealLabel->setPixmap(m_scaledBg);

        updateScoreLabel(m_logic->getCurrentScore());
    }

    if (m_resultOverlay) m_resultOverlay->setGeometry(0, 0, width(), height());
    if (m_videoWidget) m_videoWidget->setGeometry(0, 0, width(), height());

}
void MainWindow::updateScoreLabel(int score) {
    m_scoreLabel->setText(QString("🎵 当前得分: %1").arg(score));

    // ==========================================
    // 💥 终极物理拦截：只要开关打开，强制透明并直接 return！
    // ==========================================
    if (m_isBonusTime) {
        QFrame* boardPanel = centralWidget()->findChild<QFrame*>("boardPanel");
        if (boardPanel) {
            // 用 background: transparent 彻底干掉黑框
            boardPanel->setStyleSheet("background: transparent; border: none;");
        }
        m_bgRevealLabel->hide();
        m_baseBgLabel->hide();

        // 狂欢时间分数还在涨，星星也要跟着更新
        for (int i = 0; i < 3; ++i) {
            if (score >= m_starThresholds[i]) {
                m_starLabels[i]->setText("★");
                m_starLabels[i]->setStyleSheet("font-size: 28px; color: #FFD700; background: transparent; font-weight: bold;");
            }
        }
        return; // 💥 直接退出函数！彻底斩断后续所有的透明度计算和旧动画逻辑！
    }


    int target = m_levels[m_currentLevelIndex].targetScore;
    double percent = qBound(0.0, (double)score / target, 1.0);

    // ==========================================
    // 视觉特效 1：分数越高，中间面板的“黑雾”越淡 (加入平滑过渡动画版)
    // ==========================================
    int targetAlpha = 120 - (int)(percent * 80); // 计算目标透明度
    QFrame* boardPanel = centralWidget()->findChild<QFrame*>("boardPanel");

    if (boardPanel) {
        // 核心机制：查找是否已经有正在运行的透明度动画，有的话先停止，防止快速连击时画面闪烁
        QVariantAnimation* oldAnim = boardPanel->findChild<QVariantAnimation*>("alphaAnim");
        int currentAlpha = targetAlpha;

        if (oldAnim) {
            currentAlpha = oldAnim->currentValue().toInt(); // 从当前动画进行到的一半截断
            oldAnim->stop();
            oldAnim->deleteLater();
        } else if (score == 0) {
            // 如果没有正在播放的动画，且分数刚重置为0（说明刚进入新关卡），让它从通透(40)开始逐渐变暗
            currentAlpha = 40;
        }

        // 创建新的过渡动画并挂载到 boardPanel 上
        QVariantAnimation *alphaAnim = new QVariantAnimation(boardPanel);
        alphaAnim->setObjectName("alphaAnim");

        // 动画策略：新关卡变暗时时间长一点(800ms)更丝滑，平时得分变亮时快一点(150ms)跟随消除节奏
        alphaAnim->setDuration(score == 0 ? 800 : 150);
        alphaAnim->setStartValue(currentAlpha);
        alphaAnim->setEndValue(targetAlpha);
        alphaAnim->setEasingCurve(score == 0 ? QEasingCurve::InOutQuad : QEasingCurve::Linear);

        // 绑定动画值到 QSS 样式表
        connect(alphaAnim, &QVariantAnimation::valueChanged, [boardPanel](const QVariant &value){
            int alpha = value.toInt();
            boardPanel->setStyleSheet(
                QString("#boardPanel { background-color: rgba(20, 20, 30, %1); border-radius: 15px; border: 2px solid rgba(255, 105, 180, 100); }")
                    .arg(alpha)
                );
        });

        // 动画结束自动释放内存
        connect(alphaAnim, &QVariantAnimation::finished, alphaAnim, &QObject::deleteLater);
        alphaAnim->start();
    }

    // ==========================================
    // 视觉特效 2：背景图从底部升起，作为主进度条
    // ==========================================
    if (!m_scaledBg.isNull()) {
        // 👇【核心修复】：如果已经达到目标分数（正在放 MV），就不要再显示背景图了
        if (score >= target) {
            m_bgRevealLabel->hide();
            m_baseBgLabel->hide();
        }else{


         int revealHeight = m_scaledBg.height() * percent;
         if (revealHeight > 0) {
            // 👇【核心修复】：不再反复切图和移动组件位置！
            // 而是给组件套一个“视口遮罩”，只显示底部 revealHeight 那么高的一块区域
            m_bgRevealLabel->setMask(QRegion(0, this->height() - revealHeight, this->width(), revealHeight));
            m_bgRevealLabel->show();

            // 👇【加上这一行核心修复】：不管背景怎么升起，强制让游戏面板跑到最顶层来接管鼠标！
            if (boardPanel) boardPanel->raise();

          } else {
            m_bgRevealLabel->hide();
          }
        }
    }


    // 更新星星 (保持原样)
    for (int i = 0; i < 3; ++i) {
        if (score >= m_starThresholds[i]) {
            // 换成实心的大星星 ★
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

    // 【新增2】：触发狂欢时间 MV！
    connect(m_logic, &GameLogic::targetReached, this, [this](){

        m_isBonusTime = true; // 【新增】：拉下开关，进入狂欢模式

        // 隐藏掉原本用来做进度条的图，露出下方的 VideoWidget
        m_baseBgLabel->hide();
        m_bgRevealLabel->hide();

        m_videoWidget->show();
        m_videoWidget->lower(); // 确保它在最底层
        m_mediaPlayer->play();

        // 可以在这里加个醒目的动画文字提示 "BONUS TIME!!"


        // 👇【加上这两行核心修复】：播放视频时，也必须强制把游戏面板提上来！
        QFrame* boardPanel = centralWidget()->findChild<QFrame*>("boardPanel");
        if (boardPanel) {
            boardPanel->raise();


                // 👇👇👇【核心秒杀补丁】：在放 MV 的这一瞬间，立刻亲手把面板变透明！ 👇👇👇

                // 1. 掐死正在进行变暗的动画
                QList<QVariantAnimation*> anims = boardPanel->findChildren<QVariantAnimation*>();
            for (QVariantAnimation* anim : anims) {
                anim->stop();
                anim->deleteLater();
            }

            // 2. 瞬间扒掉底裤（同时保留里面文字的样式防止变黑）
            boardPanel->setStyleSheet(R"(
                #boardPanel { background: transparent; border: none; }
                QLabel { color: white; background: transparent; font-family: "Microsoft YaHei", "Segoe UI"; }
            )");
        };
    });
}
// 槽函数实现
void MainWindow::onLevelFinished(bool isWin) {
    m_mediaPlayer->stop(); // 游戏结束，停止视频
    m_videoWidget->hide();

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
