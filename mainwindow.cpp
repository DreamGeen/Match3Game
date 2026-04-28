#include "MainWindow.h"
#include <QHBoxLayout>
#include <QFrame>
#include <QMessageBox>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <QGraphicsDropShadowEffect>


MainWindow::MainWindow(UserSession session, GameMode mode, QWidget *parent)
    : QMainWindow(parent), m_session(session), m_currentMode(mode)
{
    setWindowTitle("Bocchi Clear! - 全国巡演模式");

    m_logic = new GameLogic(GameConfig::BOARD_ROWS, GameConfig::BOARD_COLS, this);
    m_gamePanel = new GamePanel(m_logic, this);

    // ==========================================
    // 关卡数据配置表：目标分数、步数限制、解锁美图
    // ==========================================
    m_levels = {
        {800,  25, ":/res/backgrounds/level1.png"},
        {3500,  25, ":/res/backgrounds/level2.png"},
        {6000, 22, ":/res/backgrounds/level3.png"},
        {10000, 20, ":/res/backgrounds/level4.png"},
        {18000, 15, ":/res/backgrounds/level5.png"}
    };

    // 如果你暂时只有 stage_bg.png，可以取消下面这行的注释进行测试：
    // for(auto& lvl : m_levels) lvl.bgPath = ":/res/backgrounds/stage_bg.png";

    setupUI();
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

    // === 1. 双层背景 ===
    m_baseBgLabel = new QLabel(centralWidget);
    m_bgRevealLabel = new QLabel(centralWidget);
    m_baseBgLabel->lower();

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
    m_currentLevelIndex = index;
    LevelConfig cfg = m_levels[index];

    m_starThresholds[0] = cfg.targetScore * 0.3;
    m_starThresholds[1] = cfg.targetScore * 0.6;
    m_starThresholds[2] = cfg.targetScore;

    m_logic->startLevel(m_session.uid, m_currentMode, cfg.targetScore, cfg.maxMoves);

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
        updateScoreLabel(m_logic->getCurrentScore());
    }
}
void MainWindow::updateScoreLabel(int score) {
    m_scoreLabel->setText(QString("🎵 当前得分: %1").arg(score));

    int target = m_levels[m_currentLevelIndex].targetScore;
    double percent = qBound(0.0, (double)score / target, 1.0);

    // ==========================================
    // 视觉特效 1：分数越高，中间面板的“黑雾”越淡
    // ==========================================
    int dynamicAlpha = 120 - (int)(percent * 80); // 透明度从 120 逐渐降到 40
    centralWidget()->findChild<QFrame*>("boardPanel")->setStyleSheet(
        QString("#boardPanel { background-color: rgba(20, 20, 30, %1); border-radius: 15px; border: 2px solid rgba(255, 105, 180, 100); }")
            .arg(dynamicAlpha)
        );

    // ==========================================
    // 视觉特效 2：背景图从底部升起，作为主进度条
    // ==========================================
    if (!m_scaledBg.isNull()) {
        int revealHeight = m_scaledBg.height() * percent;
        if (revealHeight > 0) {
            // 切割出下半部分图像
            QPixmap cropped = m_scaledBg.copy(0, m_scaledBg.height() - revealHeight, m_scaledBg.width(), revealHeight);
            m_bgRevealLabel->setPixmap(cropped);
            m_bgRevealLabel->setGeometry(0, this->height() - revealHeight, this->width(), revealHeight);
            m_bgRevealLabel->show();
        } else {
            m_bgRevealLabel->hide();
        }
    }

    // 更新星星
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

void MainWindow::initConnections() {
    // 监听步数变动
    connect(m_logic, &GameLogic::movesUpdated, this, [this](int moves){
        m_movesLabel->setText(QString("⏳ 步数: %1").arg(moves));
        if (moves <= 5) {
            // 步数极少时爆红警告
            m_movesLabel->setStyleSheet("font-size: 24px; color: #ff4500; font-weight: bold;");
        } else {
            // 正常为绿色
            m_movesLabel->setStyleSheet("font-size: 20px; color: #00FF7F; font-weight: bold;");
        }
    });

    // 监听逻辑层的关卡结束判定
    connect(m_logic, &GameLogic::levelFinished, this, [this](bool isWin){
        if (isWin) {
            if (m_currentLevelIndex < m_levels.size() - 1) {
                QMessageBox::information(this, "Stage Clear", "🎸 演唱会大成功！全场沸腾！准备进入下一场演出！");
                loadLevel(m_currentLevelIndex + 1); // 成功，自动加载下一关
            } else {
                QMessageBox::information(this, "All Clear", "🏆 恭喜！你已完成所有全国巡演！");
            }
        } else {
            QMessageBox::warning(this, "Game Over", "🎤 演出中断... 步数用完了。没关系，再试一次！");
            loadLevel(m_currentLevelIndex); // 失败，自动重开当前关卡
        }
    });
}
