#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QVBoxLayout>
#include <QResizeEvent>
#include <QVector>
#include "GamePanel.h"
#include "Global.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(UserSession session, GameMode mode, QWidget *parent = nullptr);

protected:
    // 重写 resizeEvent，用于全屏动态背景缩放自适应
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void updateScoreLabel(int score);
    void updateComboLabel(int combo);

private:
    UserSession m_session;
    GameMode m_currentMode;
    GamePanel *m_gamePanel;
    GameLogic *m_logic;

    QLabel *m_scoreLabel;
    QLabel *m_infoLabel;
    QLabel *m_levelTargetLabel;
    QLabel *m_movesLabel;

    // 关卡数据结构定义
    struct LevelConfig {
        int targetScore;
        int maxMoves;
        QString bgPath;
    };
    QVector<LevelConfig> m_levels;
    int m_currentLevelIndex = 0;


    // 【核心新增】：双层背景机制
    QLabel *m_baseBgLabel;    // 底层背景（显示上一关的图）
    QPixmap m_baseBgPixmap;

    // 背景点亮进度的核心组件
    QLabel *m_bgRevealLabel;
    QPixmap m_targetBgPixmap;
    QPixmap m_scaledBg;

    // 星星标签与动态分数线
    QLabel *m_starLabels[3];
    int m_starThresholds[3];

    void setupUI();
    void initConnections();
    void loadLevel(int index); // 加载关卡的核心函数
};

#endif // MAINWINDOW_H
