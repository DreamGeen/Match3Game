#ifndef GAMELOGIC_H
#define GAMELOGIC_H

#include <QObject>
#include <vector>
#include <set>
#include <QDateTime>
#include "GameElement.h"
#include "DBHelper.h"
#include "Global.h"

class GameLogic : public QObject {
    Q_OBJECT
public:
    explicit GameLogic(int rows = 8, int cols = 8, QObject *parent = nullptr);

    // 游戏控制
    void startNewGame(int userId, GameMode mode);
    void endAndSaveGame(bool isWin);

    // 核心操作
    bool swapTiles(QPoint p1, QPoint p2);

    // 数据接口
    const Tile& getTile(int r, int c) const { return m_board[r][c]; }
    int getCurrentScore() const { return m_currentScore; }

signals:
    void boardChanged();
    void scoreUpdated(int newScore);
    void specialEffectTriggered(QPoint pos, SpecialType type);
    void gameOver(int finalScore);

    // 【新增】：确保有这个信号
    void comboUpdated(int combo);

private:
    // 核心算法模块
    void triggerTileEffect(int r, int c, std::set<std::pair<int, int>>& toRemove);
    bool checkSpecialCombo(QPoint p1, QPoint p2); // 处理 5+5 等强力组合
    void applyGravity(); // 消除后的下落逻辑

    // --- 新增声明：解决报错的核心 ---
    QSet<QPoint> findMatches();          // 补全的横纵向扫描算法

    void removeMatches(const QSet<QPoint>& matches);

    void refillBoard(); // 随机生成新的波奇/虹夏方块

    QSet<QPoint> triggerSpecialEffect(QPoint pos, SpecialType type);



    // ... 其他私有变量 ...
    int m_currentCombo = 0; // 【新增】：记录当前连击数

    // 我们还需要调整一下 handleMatchesAndRefill 的签名，
    // 让它能知道当前是不是由玩家点击触发的第一波消除。
    // 注意：默认值 = true 只能写在 .h 头文件里
    void processMatches(QPoint triggerPos, bool triggerRefill = true);

    // 【新增这一行】：声明处理下落和填充的函数，并给一个默认参数 true
    void handleMatchesAndRefill(bool isFirstMatchInTurn = true);



    // 内部状态
    int m_rows, m_cols;
    int m_currentUserId;
    int m_currentScore;
    GameMode m_currentMode;
    QDateTime m_startTime;
    std::vector<std::vector<Tile>> m_board;
};

#endif
