#ifndef GAMEPANEL_H
#define GAMEPANEL_H

#include <QWidget>
#include <QVector>
#include <QPoint>
#include <QParallelAnimationGroup>
#include "BlockWidget.h"
#include "GameLogic.h"
#include "Global.h" // 包含 GameMode 和 UserSession
#include <QSoundEffect>
#include <QPixmap>       // 【新增】：用于处理背景图片
#include <QPaintEvent>   // 【新增】：用于重写绘图事件
#include <QLabel>

class GamePanel : public QWidget {
    Q_OBJECT
public:
    explicit GamePanel(GameLogic *logic, QWidget *parent = nullptr);

    void setInteractive(bool interactive) { m_isInteractive = interactive; }



public slots:
    // 监听逻辑层信号
    void onBoardChanged();
    void onScoreUpdated(int score);
    void onSpecialEffect(QPoint pos, SpecialType type);

    // 接收逻辑层发来的引爆信号
    void onSpecialEffectTriggered(QPoint pos, SpecialType type);

    void onAiMoveDecided(QPoint p1, QPoint p2);

    void onMagicBirdTriggered(int row, int col, QList<QPoint> targets);

signals:
    void actionFinished(); // 这一波消除动画彻底结束的信号


protected:
    // 处理玩家点击
    void mousePressEvent(QMouseEvent *event) override;

    void keyPressEvent(QKeyEvent *event) override;

    void paintEvent(QPaintEvent *event) override;


private:
    GameLogic *m_logic;
    BlockWidget* m_blocks[GameConfig::BOARD_ROWS][GameConfig::BOARD_COLS];

    // 交互状态
    QPoint m_firstSelected = QPoint(-1, -1);
    bool m_isAnimating = false; // 动画期间屏蔽操作


    // 内部功能
    void initPanel();
    void syncUiWithLogic(); // 强制刷新全图（非动画路径）
    void performSwapAnimation(QPoint p1, QPoint p2, bool isBack = false);
    bool isNeighbor(QPoint p1, QPoint p2);

    // 辅助函数：专门用来画激光
    void showLaserAnimation(int index, bool isHorizontal);

    QSoundEffect *m_selectSound;
    QSoundEffect *m_matchSound;
    void initAudio();


    // 专门用来抖动屏幕的辅助函数
    void shakeScreen();


    bool m_isInteractive = true;

    void playBombEffect(int row, int col);


    // 👇 新增：用于魔力鸟黑洞特效的底层绘制控制
    bool m_drawMagicBirdHalo = false;  // 是否开启黑洞特效
    QPoint m_magicBirdCenter;          // 黑洞中心点
    int m_haloCurrentSize = 0;         // 当前光晕大小
    double m_haloOpacity = 0.0;        // 当前光晕透明度


};

#endif
