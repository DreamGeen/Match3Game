// --- mainwindow.h ---
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QVBoxLayout>
#include <QResizeEvent>
#include <QVector>
#include <QPushButton>      // 【新增】
#include <QMediaPlayer>     // 【新增】视频播放器
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsVideoItem>
#include "GamePanel.h"
#include "Global.h"
#include "BattleBoardWidget.h"


class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    // 增加 AIDifficulty 参数
    explicit  MainWindow(UserSession session, GameMode mode, AIDifficulty diff = AIDifficulty::Normal, QWidget *parent = nullptr);

protected:
    // 重写 resizeEvent，用于全屏动态背景缩放自适应
    void resizeEvent(QResizeEvent *event) override;

signals:
    void returnToLobbyRequested(); // 【新增】返回大厅信号

private slots:
    void updateScoreLabel(int score);
    void updateComboLabel(int combo);

    // 【新增】结算界面的槽函数
    void onLevelFinished(bool isWin);
    void onNextLevelClicked();
    void onRestartClicked();
    void onReturnClicked();

private:

    BattleBoardWidget *m_player1Board;
    BattleBoardWidget *m_player2Board; // 单人模式时为 nullptr

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
        QString mvPath; // 【新增】：存放本地 MV 视频的相对路径
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


    // 【新增】：多媒体播放组件
    QMediaPlayer *m_mediaPlayer;
    QGraphicsView *m_videoView;
    QGraphicsScene *m_videoScene;
    QGraphicsVideoItem *m_videoItem;


    bool m_isBonusTime = false; // 【新增】：记录是否进入了狂欢时间

    // 【新增】：自定义结算面板组件
    QFrame *m_resultOverlay;
    QLabel *m_resultTitle;
    QLabel *m_resultScore;
    QPushButton *m_nextBtn;
    QPushButton *m_restartBtn;
    QPushButton *m_returnBtn;

    void setupUI();
    void setupCommonBackground(QWidget *centralWidget); // 抽离公共背景
    void setupSinglePlayerUI(QWidget *centralWidget);   // 单人专属UI
    void setupAIBattleUI(QWidget *centralWidget);       // AI对战专属UI

    // AI模式新增的指针
    GameLogic *m_aiLogic;
    GamePanel *m_aiPanel;
    QLabel *m_aiScoreLabel;


    void initConnections();
    void loadLevel(int index); // 加载关卡的核心函数
    void setupResultPanel(); // 【新增】

    int m_aiDelay = 2000; // AI 思考延迟



};

#endif // MAINWINDOW_H
