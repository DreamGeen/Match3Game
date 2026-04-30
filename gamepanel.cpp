#include "GamePanel.h"
#include <QMouseEvent>
#include <QDebug>
#include <QPainter>                // 【新增】：绘图必需
#include <QGraphicsOpacityEffect>  // 【新增】：修复激光透明度动画必需
#include <QPropertyAnimation>
#include <QSequentialAnimationGroup> // <--- 加上这个
#include <cmath>
#include <QTimer>
#include <QVariantAnimation>
#include <QRandomGenerator>
#include <QLabel>
#include <QParallelAnimationGroup>
#include <QPoint>
#include <QKeyEvent>



GamePanel::GamePanel(GameLogic *logic, QWidget *parent)
    : QWidget(parent), m_logic(logic)
{
    // 设置舞台大小：80*8=640
    setFixedSize(GameConfig::BOARD_COLS * GameConfig::TILE_SIZE,
                 GameConfig::BOARD_ROWS * GameConfig::TILE_SIZE);

    initPanel();

    // 让 GamePanel 背景全透明，露出底下的主窗口背景
    this->setAttribute(Qt::WA_TranslucentBackground);

    // ================= 原有信号绑定 =================
    connect(m_logic, &GameLogic::boardChanged, this, &GamePanel::onBoardChanged);
    connect(m_logic, &GameLogic::specialEffectTriggered, this, &GamePanel::onSpecialEffectTriggered);

    initAudio();

    // ===============================================
    // 👇 【新增】：绑定新引擎的核心信号，接管所有动画与反馈
    // ===============================================

    // 1. 监听【无效交换】信号：触发退回动画
    connect(m_logic, &GameLogic::invalidSwap, this, [=](QPoint p1, QPoint p2){
        // 调用你刚刚改好的动画函数，isBack 设为 true 让方块弹回去
        performSwapAnimation(p1, p2, true);
    });

    // 2. 监听【方块爆炸】信号：播放音效和特效
    connect(m_logic, &GameLogic::boardExploded, this, [=](QSet<QPoint> killList){
        // 消除成功！播放吉他扫弦音效
        if (m_matchSound) {
            m_matchSound->play();
        }
        // （未来如果你想加爆炸粒子特效，可以遍历 killList 里的坐标，在这里生成特效动画）
    });

    // 3. 监听【回合结束】信号：解除锁定，推进游戏流程
    connect(m_logic, &GameLogic::turnEnded, this, [=](){
        // 无论连击了多少次，引擎现在彻底安静了
        m_isAnimating = false; // 解除 UI 动画锁定，允许玩家再次拖拽
        emit actionFinished(); // 发送操作结束信号（通知 AI 可以思考下一步了）
    });


    // 4. 监听【魔力鸟吸收】信号：触发放大与全屏吸收动画
    connect(m_logic, &GameLogic::magicBirdTriggered, this, &GamePanel::onMagicBirdTriggered);




    // 接收键盘输入
    this->setFocusPolicy(Qt::StrongFocus);
}



void GamePanel::initPanel() {
    // 根据逻辑层初始状态生成 64 个波奇/虹夏方块
    for(int r = 0; r < GameConfig::BOARD_ROWS; ++r) {
        for(int c = 0; c < GameConfig::BOARD_COLS; ++c) {

            Tile data = m_logic->getTile(r, c);
            m_blocks[r][c] = new BlockWidget(r, c, data, this);
        }
    }
}

void GamePanel::mousePressEvent(QMouseEvent *event) {
    if (!m_isInteractive || m_isAnimating) return; // 【修改点】被托管时锁死鼠标

    int col = event->pos().x() / GameConfig::TILE_SIZE;
    int row = event->pos().y() / GameConfig::TILE_SIZE;

    // 防止越界检测
    if (row < 0 || row >= GameConfig::BOARD_ROWS || col < 0 || col >= GameConfig::BOARD_COLS) return;

    QPoint currentClick(row, col);

    if (m_firstSelected == QPoint(-1, -1)) {
        // --- 第一次点击 ---
        m_firstSelected = currentClick;
        // 让选中的波奇“亮起来”
        m_blocks[row][col]->setSelected(true);

        // 【新增】：播放点击音效，配合缩放动画
        if (m_selectSound->isLoaded()) {
            m_selectSound->play();
        }

    } else {
        // --- 第二次点击 ---
        // 无论点击哪里，都要取消第一个方块的选中状态
        m_blocks[m_firstSelected.x()][m_firstSelected.y()]->setSelected(false);

        if (currentClick == m_firstSelected) {
            // 如果点的是同一个，直接取消选中
            m_firstSelected = QPoint(-1, -1);
            return;
        }

        if (isNeighbor(m_firstSelected, currentClick)) {
            // 相邻则尝试交换
            performSwapAnimation(m_firstSelected, currentClick);
        } else {
            // 不相邻，则将当前点击的方块设为新的“第一个选中”
            m_firstSelected = currentClick;
            m_blocks[row][col]->setSelected(true);
        }
    }
}

bool GamePanel::isNeighbor(QPoint p1, QPoint p2) {
    return (std::abs(p1.x() - p2.x()) + std::abs(p1.y() - p2.y())) == 1;
}

void GamePanel::performSwapAnimation(QPoint p1, QPoint p2, bool isBack) {
    m_isAnimating = true;

    BlockWidget *b1 = m_blocks[p1.x()][p1.y()];
    BlockWidget *b2 = m_blocks[p2.x()][p2.y()];

    auto *group = new QParallelAnimationGroup(this);

    // 动画 A：b1 移向 b2
    auto *a1 = new QPropertyAnimation(b1, "pos");
    a1->setDuration(300);
    a1->setEndValue(BlockWidget::logicalToPixel(p2.x(), p2.y()));
    a1->setEasingCurve(QEasingCurve::OutCubic);

    // 动画 B：b2 移向 b1
    auto *a2 = new QPropertyAnimation(b2, "pos");
    a2->setDuration(300);
    a2->setEndValue(BlockWidget::logicalToPixel(p1.x(), p1.y()));
    a2->setEasingCurve(QEasingCurve::OutCubic);

    group->addAnimation(a1);
    group->addAnimation(a2);

    connect(group, &QAnimationGroup::finished, [=]() {
        // UI 物理指针交换
        std::swap(m_blocks[p1.x()][p1.y()], m_blocks[p2.x()][p2.y()]);

        if (!isBack) {
            // 👇 修改：不要判断返回值，直接把操作丢给后端引擎处理！
            // 引擎处理完会自己发射信号来决定下一步动画。
            m_logic->handleSwap(p1, p2);
        } else {
            // 如果是“退回动画”播完了，说明一次无效交换彻底结束
            m_isAnimating = false;
            emit actionFinished(); // 通知 AI 没消成，可以继续算下一步了
        }

        group->deleteLater();
    });

    group->start();
}

void GamePanel::onBoardChanged() {
    for(int r = 0; r < GameConfig::BOARD_ROWS; ++r) {
        for(int c = 0; c < GameConfig::BOARD_COLS; ++c) {
            // 获取方块前后的状态
            Tile oldData = m_blocks[r][c]->getTileData();
            Tile newData = m_logic->getTile(r, c);

            m_blocks[r][c]->setTileData(newData);

            // 👇 【核心补丁】：保底重置透明度。
            // 确保那些没有被触发动画的方块（比如只是往下掉的方块）不会因为上一轮特效而永远隐身
            m_blocks[r][c]->setOpacity(1.0);


            // 👇 核心动画逻辑：如果这个位置变成了新的实体方块（颜色不同，或是新生成的）
            if (newData.color != 0 && (oldData.color != newData.color || oldData.special != newData.special)) {

                QPoint targetPos = BlockWidget::logicalToPixel(r, c);
                // 让方块起始位置偏上（制造一种从上空掉下来的视觉欺骗）
                QPoint startPos = targetPos - QPoint(0, GameConfig::TILE_SIZE / 2);

                m_blocks[r][c]->move(startPos);
                m_blocks[r][c]->setOpacity(0.0); // 初始全透明

                // 创建组合动画（同时播放掉落和淡入）
                QParallelAnimationGroup *group = new QParallelAnimationGroup(m_blocks[r][c]);

                // 1. 掉落动画
                QPropertyAnimation *moveAnim = new QPropertyAnimation(m_blocks[r][c], "pos");
                moveAnim->setDuration(300 + r * 40); // 加上 r * 40 让底部的方块掉落得稍微久一点，产生级联感
                moveAnim->setEndValue(targetPos);
                moveAnim->setEasingCurve(QEasingCurve::OutBounce); // 落地时带有 Q弹 的物理反馈

                // 2. 淡入动画
                QPropertyAnimation *fadeAnim = new QPropertyAnimation(m_blocks[r][c], "opacity");
                fadeAnim->setDuration(250);
                fadeAnim->setEndValue(1.0);

                group->addAnimation(moveAnim);
                group->addAnimation(fadeAnim);
                group->start(QAbstractAnimation::DeleteWhenStopped);
            }
        }
    }
}


// 补全分数更新的槽函数
void GamePanel::onScoreUpdated(int score) {
    // 这里通常用于触发面板层级的特效，比如在得分时让背景闪烁一下
    // 目前可以先留空，或者打一行日志测试
    qDebug() << "GamePanel 收到分数更新信号:" << score;
}

// 补全特殊效果触发的槽函数
void GamePanel::onSpecialEffect(QPoint pos, SpecialType type) {
    // 产生特殊方块时，让那个格子闪烁 3 次
    BlockWidget* block = m_blocks[pos.x()][pos.y()];
    QPropertyAnimation *ani = new QPropertyAnimation(block, "opacity");
    ani->setDuration(200);
    ani->setKeyValueAt(0.5, 0.2);
    ani->setEndValue(1.0);
    ani->setLoopCount(3);
    ani->start(QAbstractAnimation::DeleteWhenStopped);

    // 当逻辑层触发“吉他扫弦”或“音箱爆炸”时，在这里播放对应的全屏或局部特效
    qDebug() << "触发特殊特效！位置:" << pos << " 类型:" << static_cast<int>(type);

    // 毕设亮点：可以在这里根据 type 播放不同的 QPropertyAnimation
    if (type == SpecialType::Bomb) {
        // 播放摇滚放大器爆炸的粒子效果
    }
}


void GamePanel::initAudio() {
    // 初始化点击音效
    m_selectSound = new QSoundEffect(this);
    m_selectSound->setSource(QUrl::fromLocalFile(":/res/audio/select.wav"));
    m_selectSound->setVolume(0.8); // 音量 0.0 到 1.0

    // 初始化消除音效
    m_matchSound = new QSoundEffect(this);
    m_matchSound->setSource(QUrl::fromLocalFile(":/res/audio/match.wav"));
    m_matchSound->setVolume(1.0);
}



void GamePanel::onSpecialEffectTriggered(QPoint pos, SpecialType type) {
        qDebug() << "UI层收到特效信号！类型：" << (int)type << " 坐标：" << pos;
    // 1. 播放你准备好的帅气音效
     m_matchSound->play();

    // 2. 根据类型绘制不同的激光
    if (type == SpecialType::LineHorizontal) {
        showLaserAnimation(pos.x(), true);  // 横向扫弦
    }
    else if (type == SpecialType::LineVertical) {
        showLaserAnimation(pos.y(), false); // 纵向扫弦
    }
    else if (type == SpecialType::Bomb) {
        playBombEffect(pos.x(),pos.y());
        // 放大器爆炸：可以让屏幕轻微抖动一下
         shakeScreen();
    }
}

// 将此函数添加到 GamePanel.cpp 中
void GamePanel::shakeScreen() {
    // 抖动的是 GamePanel 自己。但如果你把 GamePanel 放在一个 MainWindow 里，
    // 更好的做法是抖动那个 MainWindow，或者抖动 GamePanel 的父控件。
    // 这里我们假设直接抖动 GamePanel 就行。

    // 记录抖动前的原始位置
    QPoint originalPos = this->pos();

    // 定义抖动的幅度和次数
    int shakeAmount = 6;  // 每次移动的最大像素数
    int shakeDuration = 50; // 每次摆动的持续时间（毫秒）
    int numShakes = 6;    // 摆动的次数

    // 创建一个串行动画组，让每次抖动依次执行
    auto *group = new QSequentialAnimationGroup(this);

    for (int i = 0; i < numShakes; ++i) {
        // 创建一个位置动画
        auto *ani = new QPropertyAnimation(this, "pos");
        ani->setDuration(shakeDuration);

        // 计算一个新的随机偏差位置
        int dx = (qrand() % (shakeAmount * 2 + 1)) - shakeAmount;
        int dy = (qrand() % (shakeAmount * 2 + 1)) - shakeAmount;
        QPoint offsetPos = originalPos + QPoint(dx, dy);

        ani->setStartValue(this->pos()); // 从当前位置开始
        ani->setEndValue(offsetPos);     // 移动到偏差位置
        // 使用一个更有“冲击感”的缓动曲线
        ani->setEasingCurve(QEasingCurve::Linear); // 或者 OutCubic，但抖动一般用线性更激烈

        group->addAnimation(ani);
    }

    // 最后添加一个动画，让它回到原始位置
    auto *finalAni = new QPropertyAnimation(this, "pos");
    finalAni->setDuration(shakeDuration * 2); // 稍微慢一点复位
    finalAni->setStartValue(this->pos());
    finalAni->setEndValue(originalPos);
    finalAni->setEasingCurve(QEasingCurve::OutCubic);
    group->addAnimation(finalAni);

    // 动画组完成后自动删除，防止内存泄漏
    connect(group, &QAnimationGroup::finished, group, &QObject::deleteLater);

    // 开始抖动！
    group->start();
}

void GamePanel::showLaserAnimation(int index, bool isHorizontal) {
    int cellSize = width() / 8;
    int laserThick = cellSize * 0.8;
    int centerX = width() / 2;
    int centerY = height() / 2;

    // ==========================================
    // 💥 第一层：主干激光 (居合斩爆开与消散)
    // ==========================================
    QWidget *laser = new QWidget(this);

    // 初始高亮样式
    QString baseStyle = isHorizontal ?
                            "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(255,20,147,0), stop:0.2 rgba(255,105,180,200), stop:0.5 rgba(255,255,255,255), stop:0.8 rgba(255,105,180,200), stop:1 rgba(255,20,147,0));" :
                            "background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 rgba(255,20,147,0), stop:0.2 rgba(255,105,180,200), stop:0.5 rgba(255,255,255,255), stop:0.8 rgba(255,105,180,200), stop:1 rgba(255,20,147,0));";
    laser->setStyleSheet(baseStyle);

    // 初始化为屏幕正中心的一个“点”
    if (isHorizontal) {
        laser->setGeometry(centerX, index * cellSize + (cellSize - laserThick) / 2, 0, laserThick);
    } else {
        laser->setGeometry(index * cellSize + (cellSize - laserThick) / 2, centerY, laserThick, 0);
    }
    laser->show();
    laser->raise();

    QGraphicsOpacityEffect *opEff = new QGraphicsOpacityEffect(laser);
    laser->setGraphicsEffect(opEff);

    // 横扫/纵扫展开动画 (极致爆发速度)
    QPropertyAnimation *geomAnim = new QPropertyAnimation(laser, "geometry");
    geomAnim->setDuration(250);
    geomAnim->setEndValue(isHorizontal ?
                              QRect(0, index * cellSize + (cellSize - laserThick) / 2, width(), laserThick) :
                              QRect(index * cellSize + (cellSize - laserThick) / 2, 0, laserThick, height()));
    geomAnim->setEasingCurve(QEasingCurve::OutExpo);

    // 缓慢暗淡动画
    QPropertyAnimation *alphaAnim = new QPropertyAnimation(opEff, "opacity");
    alphaAnim->setDuration(800);
    alphaAnim->setStartValue(1.0);
    alphaAnim->setKeyValueAt(0.2, 1.0);
    alphaAnim->setEndValue(0.0);

    QParallelAnimationGroup *mainGroup = new QParallelAnimationGroup(laser);
    mainGroup->addAnimation(geomAnim);
    mainGroup->addAnimation(alphaAnim);
    connect(mainGroup, &QAnimationGroup::finished, laser, &QWidget::deleteLater);
    mainGroup->start(QAbstractAnimation::DeleteWhenStopped);

    // ==========================================
    // ⚡ 第二层：核心高频脉冲 (动态能量带)
    // 核心秘诀：利用正弦波实时改变 CSS 渐变色，制造闪烁电弧感
    // ==========================================
    QVariantAnimation *pulseAnim = new QVariantAnimation(laser);
    pulseAnim->setDuration(800);
    pulseAnim->setStartValue(0.0);
    pulseAnim->setEndValue(1.0);
    connect(pulseAnim, &QVariantAnimation::valueChanged, [laser, isHorizontal](const QVariant &val){
        double v = val.toDouble();
        // 产生高频脉冲 (乘以 6 代表在存活期内激烈波动 6 次)
        double coreWidth = 0.05 + 0.35 * std::abs(std::sin(v * 3.14159 * 6));
        double s1 = 0.5 - coreWidth;
        double s2 = 0.5 + coreWidth;

        QString pulseStyle = isHorizontal ?
                                 QString("background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(255,20,147,0), stop:%1 rgba(255,105,180,200), stop:0.5 rgba(255,255,255,255), stop:%2 rgba(255,105,180,200), stop:1 rgba(255,20,147,0));").arg(s1).arg(s2) :
                                 QString("background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 rgba(255,20,147,0), stop:%1 rgba(255,105,180,200), stop:0.5 rgba(255,255,255,255), stop:%2 rgba(255,105,180,200), stop:1 rgba(255,20,147,0));").arg(s1).arg(s2);
        laser->setStyleSheet(pulseStyle);
    });
    pulseAnim->start(QAbstractAnimation::DeleteWhenStopped);

    // ==========================================
    // ☄️ 第三层：高速能量流 (离子光束飞掠穿梭)
    // 核心秘诀：生成细长的白条，从中心极速射向两边
    // ==========================================
    for(int i = 0; i < 4; i++) {
        QWidget *streak = new QWidget(this);
        streak->setStyleSheet("background-color: rgba(255, 255, 255, 240); border-radius: 2px;");
        streak->hide(); // 初始隐藏，等待延迟触发

        int streakThick = 2 + (rand() % 4);          // 随机极细的射线
        int streakLength = 80 + (rand() % 80);       // 随机射线长度
        int delay = rand() % 150;                    // 错开它们发射的时间
        int duration = 200 + (rand() % 150);         // 随机飞行速度

        // 在主激光的横截面内随机做一点偏移，不那么死板
        int offset = (rand() % (laserThick / 2)) - (laserThick / 4);

        QPropertyAnimation *moveAnim = new QPropertyAnimation(streak, "pos", streak);
        moveAnim->setDuration(duration);
        moveAnim->setEasingCurve(QEasingCurve::InCubic);

        if (isHorizontal) {
            int yPos = index * cellSize + cellSize / 2 + offset - streakThick / 2;
            // 一半往左飞，一半往右飞
            int startX = (i % 2 == 0) ? centerX - streakLength : centerX;
            int endX   = (i % 2 == 0) ? width() : -streakLength;

            streak->setGeometry(startX, yPos, streakLength, streakThick);
            moveAnim->setStartValue(QPoint(startX, yPos));
            moveAnim->setEndValue(QPoint(endX, yPos));
        } else {
            int xPos = index * cellSize + cellSize / 2 + offset - streakThick / 2;
            // 一半往上飞，一半往下飞
            int startY = (i % 2 == 0) ? centerY - streakLength : centerY;
            int endY   = (i % 2 == 0) ? height() : -streakLength;

            streak->setGeometry(xPos, startY, streakThick, streakLength);
            moveAnim->setStartValue(QPoint(xPos, startY));
            moveAnim->setEndValue(QPoint(xPos, endY));
        }

        // 延迟触发能量流动画
        QTimer::singleShot(delay, [streak, moveAnim]() {
            streak->show();
            streak->raise();
            moveAnim->start(QAbstractAnimation::DeleteWhenStopped);
            connect(moveAnim, &QPropertyAnimation::finished, streak, &QWidget::deleteLater);
        });
    }

    // ==========================================
    // 🌍 终极反馈：调用你的代码进行屏幕抖动
    // ==========================================
    shakeScreen();
}
void GamePanel::onAiMoveDecided(QPoint p1, QPoint p2) {
    performSwapAnimation(p1, p2, false); // 完美复用你的动画！
}



void GamePanel::playBombEffect(int row, int col) {
    int blockSize = 80;
    int centerX = col * blockSize + blockSize / 2;
    int centerY = row * blockSize + blockSize / 2;

    // 随机选择特效 (可以根据喜好调整概率)
    int randomChance = QRandomGenerator::global()->bounded(100);
    QString effectImagePath = (randomChance < 50) ? ":/res/bomb/effect_neon.png" : ":/res/bomb/effect_glitch.png";

    QLabel *effectLabel = new QLabel(this);
    effectLabel->setPixmap(QPixmap(effectImagePath));
    effectLabel->setScaledContents(true);
    effectLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

    // 👇【核心修复 1】：必须置于顶层，防止被其他方块盖住！
    effectLabel->raise();
    effectLabel->show();

    QGraphicsOpacityEffect *opacityEffect = new QGraphicsOpacityEffect(effectLabel);
    effectLabel->setGraphicsEffect(opacityEffect);

    QParallelAnimationGroup *animGroup = new QParallelAnimationGroup(effectLabel);

    // 👇【核心修复 2】：加大尺寸对抗透明边缘，500像素大概能覆盖 6x6 的范围
    double ratio = 1.833;
    int targetWidth = 500;
    int targetHeight = static_cast<int>(targetWidth / ratio);

    // 动画 A：缩放动画
    QPropertyAnimation *scaleAnim = new QPropertyAnimation(effectLabel, "geometry");
    scaleAnim->setDuration(800); // 稍微延长一点时间，看清细节
    scaleAnim->setStartValue(QRect(centerX, centerY, 0, 0));
    scaleAnim->setEndValue(QRect(centerX - targetWidth / 2, centerY - targetHeight / 2, targetWidth, targetHeight));
    // 👇【核心修复 3】：换成 OutBack 曲线！它会“轰”地一下猛然放大甚至超出一点，再缩回来，极具爆炸的爽感！
    scaleAnim->setEasingCurve(QEasingCurve::OutBack);

    // 动画 B：渐隐动画
    QPropertyAnimation *fadeAnim = new QPropertyAnimation(opacityEffect, "opacity");
    fadeAnim->setDuration(800);
    // 👇【核心修复 4】：使用关键帧！前 60% 的时间保持绝对清晰，最后 40% 的时间再瞬间消散！
    fadeAnim->setKeyValueAt(0.0, 1.0);
    fadeAnim->setKeyValueAt(0.6, 1.0);
    fadeAnim->setKeyValueAt(1.0, 0.0);

    animGroup->addAnimation(scaleAnim);
    animGroup->addAnimation(fadeAnim);

    connect(animGroup, &QParallelAnimationGroup::finished, effectLabel, &QLabel::deleteLater);
    animGroup->start(QAbstractAnimation::DeleteWhenStopped);
}

void GamePanel::paintEvent(QPaintEvent *event) {
    // 调用父类，保留原有透明背景属性
    QWidget::paintEvent(event);

    // 绘制魔力鸟的黑洞光晕
    if (m_drawMagicBirdHalo && m_haloOpacity > 0.01) {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        // 设置具有透明度的画刷
        QRadialGradient gradient(m_magicBirdCenter, m_haloCurrentSize / 2.0);
        gradient.setColorAt(0, QColor(255, 255, 255, 255 * m_haloOpacity));
        gradient.setColorAt(0.3, QColor(138, 43, 226, 220 * m_haloOpacity));
        gradient.setColorAt(1, QColor(0, 0, 0, 0));

        painter.setBrush(gradient);
        painter.setPen(Qt::NoPen);

        // 在指定中心绘制光晕
        painter.drawEllipse(m_magicBirdCenter, m_haloCurrentSize / 2, m_haloCurrentSize / 2);
    }
}


void GamePanel::onMagicBirdTriggered(int row, int col, QList<QPoint> targets) {
    m_isAnimating = true;

    BlockWidget* magicBird = m_blocks[row][col];
    if (!magicBird || targets.isEmpty()) {
        m_isAnimating = false;
        return;
    }

    if (m_matchSound) m_matchSound->play();

    QSequentialAnimationGroup* sequenceGroup = new QSequentialAnimationGroup(this);

    // 1. 初始化底层绘制所需的数据
    int targetHaloSize = magicBird->width() * 2.5;
    m_magicBirdCenter = magicBird->pos() + QPoint(magicBird->width() / 2, magicBird->height() / 2);
    m_drawMagicBirdHalo = true; // 开启绘制
    m_haloCurrentSize = 0;
    m_haloOpacity = 1.0;

    // ==========================================
    // 阶段一：黑洞展开与鸟震颤
    // ==========================================
    QParallelAnimationGroup* chargeGroup = new QParallelAnimationGroup(this);

    // 黑洞展开动画：通过改变变量并 update() 触发底层绘制
    QVariantAnimation* haloExpand = new QVariantAnimation(this);
    haloExpand->setDuration(450);
    haloExpand->setStartValue(0);
    haloExpand->setEndValue(targetHaloSize);
    haloExpand->setEasingCurve(QEasingCurve::OutCubic);
    connect(haloExpand, &QVariantAnimation::valueChanged, this, [=](const QVariant& val) {
        m_haloCurrentSize = val.toInt();
        this->update(); // 每一帧都通知系统重绘面板
    });

    QPropertyAnimation* birdShake = new QPropertyAnimation(magicBird, "pos");
    birdShake->setDuration(450);
    QPoint originPos = magicBird->pos();
    birdShake->setStartValue(originPos);
    birdShake->setKeyValueAt(0.15, originPos + QPoint(-4, -2));
    birdShake->setKeyValueAt(0.45, originPos + QPoint(4, 3));
    birdShake->setKeyValueAt(0.75, originPos + QPoint(-3, 4));
    birdShake->setEndValue(originPos);

    chargeGroup->addAnimation(haloExpand);
    chargeGroup->addAnimation(birdShake);
    sequenceGroup->addAnimation(chargeGroup);

    // ==========================================
    // 阶段二：吸附方块与黑洞消散
    // ==========================================
    QParallelAnimationGroup* absorbGroup = new QParallelAnimationGroup(this);

    // 黑洞渐隐动画
    QVariantAnimation* haloFade = new QVariantAnimation(this);
    haloFade->setDuration(350);
    haloFade->setStartValue(1.0);
    haloFade->setEndValue(0.0);
    connect(haloFade, &QVariantAnimation::valueChanged, this, [=](const QVariant& val) {
        m_haloOpacity = val.toDouble();
        this->update(); // 每一帧刷新透明度
    });
    absorbGroup->addAnimation(haloFade);

    QPoint birdPos = magicBird->pos();
    for (const QPoint& p : targets) {
        BlockWidget* block = m_blocks[p.x()][p.y()];
        if (!block || block == magicBird) continue;

        block->raise(); // 保证飞行的方块在最上面

        QPropertyAnimation* flyAnim = new QPropertyAnimation(block, "pos");
        flyAnim->setDuration(350);
        flyAnim->setStartValue(block->pos());
        flyAnim->setEndValue(birdPos);
        flyAnim->setEasingCurve(QEasingCurve::InBack);

        QPropertyAnimation* fadeAnim = new QPropertyAnimation(block, "opacity");
        fadeAnim->setDuration(350);
        fadeAnim->setStartValue(1.0);
        fadeAnim->setEndValue(0.0);

        QParallelAnimationGroup* singleBlockGroup = new QParallelAnimationGroup(this);
        singleBlockGroup->addAnimation(flyAnim);
        singleBlockGroup->addAnimation(fadeAnim);

        absorbGroup->addAnimation(singleBlockGroup);
    }
    sequenceGroup->addAnimation(absorbGroup);

    // ==========================================
    // 阶段三：安全清理
    // ==========================================
    connect(sequenceGroup, &QSequentialAnimationGroup::finished, this, [=]() {
        // 恢复方块透明度
        for (const QPoint& p : targets) {
            if (m_blocks[p.x()][p.y()]) {
                // ❌ 删掉或注释掉下面这行！不要在这里恢复透明度，让它们保持隐身！
                // m_blocks[p.x()][p.y()]->setOpacity(1.0);

                // 👇 核心修复：将被吸走的方块瞬间拽回它们原本的逻辑坐标位！
                m_blocks[p.x()][p.y()]->move(BlockWidget::logicalToPixel(p.x(), p.y()));
            }
        }

        // 👇 彻底关闭黑洞特效绘制
        m_drawMagicBirdHalo = false;

        sequenceGroup->deleteLater();
        shakeScreen();
        m_isAnimating = false;

        // 执行最后一次干净的重绘
        this->update();
    });

    sequenceGroup->start();
}




void GamePanel::keyPressEvent(QKeyEvent *event) {
    // 确保有逻辑指针且不在动画锁定期间
    if (!m_isAnimating && m_logic) {
        switch (event->key()) {
        case Qt::Key_M:
            m_logic->debugSpawnSpecialBlock(SpecialType::MagicBird);
            qDebug() << "🛸 开发者外挂：生成了 [魔力鸟]";
            return; // 直接 return，不传给父类

        case Qt::Key_H:
            m_logic->debugSpawnSpecialBlock(SpecialType::LineHorizontal);
            qDebug() << "🛸 开发者外挂：生成了 [横向特效]";
            return;

        case Qt::Key_V:
            m_logic->debugSpawnSpecialBlock(SpecialType::LineVertical);
            qDebug() << "🛸 开发者外挂：生成了 [纵向特效]";
            return;

        case Qt::Key_B:
            m_logic->debugSpawnSpecialBlock(SpecialType::Bomb);
            qDebug() << "🛸 开发者外挂：生成了 [爆炸炸弹]";
            return;
        }
    }

    // 如果按下的不是外挂键，交还给父类正常处理
    QWidget::keyPressEvent(event);
}

