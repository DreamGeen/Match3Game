#include "GamePanel.h"
#include <QMouseEvent>
#include <QDebug>
#include <QPainter>                // 【新增】：绘图必需
#include <QGraphicsOpacityEffect>  // 【新增】：修复激光透明度动画必需
#include <QPropertyAnimation>
#include <QSequentialAnimationGroup> // <--- 加上这个

GamePanel::GamePanel(GameLogic *logic, QWidget *parent)
    : QWidget(parent), m_logic(logic)
{
    // 设置舞台大小：80*8=640
    setFixedSize(GameConfig::BOARD_COLS * GameConfig::TILE_SIZE,
                 GameConfig::BOARD_ROWS * GameConfig::TILE_SIZE);


    initPanel();

    // 让 GamePanel 背景全透明，露出底下的主窗口背景
    this->setAttribute(Qt::WA_TranslucentBackground);

    // 绑定逻辑层信号
    connect(m_logic, &GameLogic::boardChanged, this, &GamePanel::onBoardChanged);

    initAudio();

    // 确保有这一行！没有它，信号发出来也会消散在空气中
    connect(m_logic, &GameLogic::specialEffectTriggered,
            this, &GamePanel::onSpecialEffectTriggered);
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
        // 物理指针交换
        std::swap(m_blocks[p1.x()][p1.y()], m_blocks[p2.x()][p2.y()]);

        if (!isBack) {
            // 调用 GameLogic 判定是否能消除
            if (!m_logic->swapTiles(p1, p2)) {
                // 如果不能消除，自动播放“换回来”的动画
                performSwapAnimation(p1, p2, true);
            }else{
                // 【新增】：消除成功，播放吉他扫弦
                m_matchSound->play();
                emit actionFinished(); // 【新增】操作成功，通知AI继续
            }
        }else {
          emit actionFinished(); // 【新增】AI没消成换回去了，回合也算结束
        }

        m_isAnimating = false;
        group->deleteLater();
    });

    group->start();
}


void GamePanel::onBoardChanged() {
    // 当逻辑层发生消除或下落后，同步 UI 数据
    for(int r = 0; r < GameConfig::BOARD_ROWS; ++r) {
        for(int c = 0; c < GameConfig::BOARD_COLS; ++c) {
            m_blocks[r][c]->setTileData(m_logic->getTile(r, c));
            // 找到对应的 Widget 并更新它
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
    QWidget *laser = new QWidget(this);

    // 设置“波奇粉”激光样式
    QString style = isHorizontal ?
                        "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(255,105,180,0), stop:0.5 rgba(255,182,193,255), stop:1 rgba(255,105,180,0));" :
                        "background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 rgba(255,105,180,0), stop:0.5 rgba(255,182,193,255), stop:1 rgba(255,105,180,0));";

    laser->setStyleSheet(style);

    int cellSize = width() / 8;
    if (isHorizontal) {
        laser->setGeometry(0, index * cellSize + cellSize/3, width(), cellSize/3);
    } else {
        laser->setGeometry(index * cellSize + cellSize/3, 0, cellSize/3, height());
    }

    laser->show();

    // 【关键修复 1】：强制将激光提到 UI 最上层，防止被 BlockWidget 挡住
    laser->raise();

    // 【关键修复 2】：使用 QGraphicsOpacityEffect 解决普通 QWidget 无法执行透明度动画的问题
    QGraphicsOpacityEffect *opacityEffect = new QGraphicsOpacityEffect(laser);
    laser->setGraphicsEffect(opacityEffect);

    // 动画目标改为 opacityEffect 的 "opacity" 属性
    QPropertyAnimation *ani = new QPropertyAnimation(opacityEffect, "opacity");
    ani->setDuration(250);
    ani->setStartValue(1.0);
    ani->setEndValue(0.0);
    ani->setEasingCurve(QEasingCurve::OutCubic);

    connect(ani, &QPropertyAnimation::finished, laser, &QWidget::deleteLater);
    ani->start(QAbstractAnimation::DeleteWhenStopped);
}

void GamePanel::onAiMoveDecided(QPoint p1, QPoint p2) {
    performSwapAnimation(p1, p2, false); // 完美复用你的动画！
}
