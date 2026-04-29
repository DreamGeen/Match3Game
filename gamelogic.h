#ifndef GAMELOGIC_H
#define GAMELOGIC_H

#include <QObject>
#include <vector>
#include <set>
#include <QDateTime>
#include "GameElement.h"
#include "DBHelper.h"
#include "Global.h"
#include <QQueue> // 记得加上队列头文件
#include <QRandomGenerator>

class GameLogic : public QObject {
    Q_OBJECT
public:
    explicit GameLogic(int rows = 8, int cols = 8, QObject *parent = nullptr);

    // 更新后的启动函数：传入目标分数和限制步数
    void startLevel(int userId, GameMode mode, int targetScore, int maxMoves);

    // 游戏控制
    void startNewGame(int userId, GameMode mode);
    void endAndSaveGame(bool isWin);

    // 核心操作
    bool swapTiles(QPoint p1, QPoint p2);

    // 数据接口
    const Tile& getTile(int r, int c) const { return m_board[r][c]; }
    int getCurrentScore() const { return m_currentScore; }
    // 👇【新增这一行】：让外界能获取引擎里最真实的目标分数！
    int getTargetScore() const { return m_targetScore; }


    // 【修改】：加入难度参数，让大脑知道自己有多聪明
    void setBotMode(bool isBot, AIDifficulty diff = AIDifficulty::Normal) {
        m_isBot = isBot;
        m_aiDifficulty = diff;
    }
    void triggerNextBotMove();// 触发 AI 思考
    bool isGameOver() const { return m_isGameOver; }



    void setRemainingMoves(int moves); // 强制修改剩余步数

    bool hasValidMoves(); // 检查是否有可行的移动
    void shuffleBoard();  // 打乱棋盘


    // 玩家或 AI 交换方块的统一入口
    void handleSwap(QPoint p1, QPoint p2);


    // 👈 2. 新增：允许外部（特别是房主）强制同步随机数种子
    void setRandomSeed(quint32 seed);

signals:
    void scoreUpdated(int newScore);
    void specialEffectTriggered(QPoint pos, SpecialType type);
    void gameOver(int finalScore);

    // 【新增】：确保有这个信号
    void comboUpdated(int combo);

    void movesUpdated(int remainingMoves); // 步数变化信号
    void levelFinished(bool isWin);        // 关卡结束信号

    void targetReached(); // 目标达成，通知播放 MV
    void aiMoveDecided(QPoint p1, QPoint p2); // AI算好的坐标

    // 👇 添加这行：声明无效交换信号
    void invalidSwap(QPoint p1, QPoint p2);
    // 👇 顺便检查一下，确保我们在新代码里 emit 的这几个信号也都声明了：
    void boardExploded(QSet<QPoint> killList); // 通知UI哪些方块爆炸了
    void boardChanged();                       // 通知UI盘面发生了改变（比如掉落、洗牌）
    void turnEnded();                          // 通知大厅回合结束（可以触发AI了）

    void playerSwapped(QPoint p1, QPoint p2); // 新增这个

private:
    // 核心算法模块

    // 流水线引擎的核心三剑客
    QSet<QPoint> expandKillList(QSet<QPoint> initialKillList);
    void executeElimination(QSet<QPoint> rawKillList, QPoint activePoint);
    SpecialType determineNewSpecial(const QSet<QPoint>& matchSet);

    // 基础的重力下落与补充新方块（保留你原有的填充逻辑，稍作修改）
    void applyGravityAndRefill();


    bool checkSpecialCombo(QPoint p1, QPoint p2); // 处理 5+5 等强力组合


    // --- 新增声明：解决报错的核心 ---
    QSet<QPoint> findMatches();          // 补全的横纵向扫描算法



    void refillBoard(); // 随机生成新的波奇/虹夏方块

    QSet<QPoint> triggerSpecialEffect(QPoint pos, SpecialType type);

    AIDifficulty m_aiDifficulty = AIDifficulty::Normal; // 记录 AI 的智商级别



    // ... 其他私有变量 ...
    int m_currentCombo = 0; // 【新增】：记录当前连击数

    // void triggerTileEffect(int r, int c, std::set<std::pair<int, int>>& toRemove);
    // void processMatches(QPoint triggerPos, bool triggerRefill = true);
    // void handleMatchesAndRefill(bool isFirstMatchInTurn = true);// 连击逻辑我们将用新的引擎替代
    // void removeMatches(const QSet<QPoint>& matches);



    void checkGameStatus(); // 判定胜负的核心逻辑

    // 关卡属性
    int m_targetScore = 0;   // 目标分数
    int m_remainingMoves = 0; // 剩余步数
    bool m_isGameOver = false;

    bool m_hasReachedTarget = false; // 是否已经触发过目标达成


    // 内部状态
    int m_rows, m_cols;
    int m_currentUserId;
    int m_currentScore;
    GameMode m_currentMode;
    QDateTime m_startTime;
    std::vector<std::vector<Tile>> m_board;



    bool m_isBot = false;
    bool simulateSwapAndCheck(QPoint p1, QPoint p2, int &scoreOut); // 脑内推演函数


    QRandomGenerator m_random;
};

#endif
