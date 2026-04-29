#include "GameLogic.h"
#include <QRandomGenerator>
#include <algorithm>
#include <QEventLoop>
#include <QTimer>
#include <QRandomGenerator>
#include <QTimer>
#include <QQueue> // 确保你的 cpp 文件顶部包含了队列头文件

GameLogic::GameLogic(int rows, int cols, QObject *parent)
    : QObject(parent), m_rows(rows), m_cols(cols), m_currentScore(0) {
    m_board.resize(rows, std::vector<Tile>(cols));
}


void GameLogic::startLevel(int userId, GameMode mode, int targetScore, int maxMoves) {
    m_targetScore = targetScore;
    m_remainingMoves = maxMoves;
    m_isGameOver = false;

    // 调用原有的初始化棋盘逻辑
    startNewGame(userId, mode);

    emit movesUpdated(m_remainingMoves);
}


void GameLogic::startNewGame(int userId, GameMode mode) {
    m_currentUserId = userId;
    m_currentMode = mode;
    m_currentScore = 0;
    m_startTime = QDateTime::currentDateTime();
    m_hasReachedTarget = false; // 【新增】：新游戏重置状态

    // 初始化棋盘（防止初始三连）
    for(int r=0; r<m_rows; ++r) {
        for(int c=0; c<m_cols; ++c) {
            int color;
            do {
                color = QRandomGenerator::global()->bounded(1, 6);
            } while ((r >= 2 && m_board[r-1][c].color == color && m_board[r-2][c].color == color) ||
                     (c >= 2 && m_board[r][c-1].color == color && m_board[r][c-2].color == color));
            m_board[r][c] = {color, SpecialType::None};
        }
    }
    emit boardChanged();
}

void GameLogic::handleSwap(QPoint p1, QPoint p2) {
    // 👇 1. 补回：游戏结束或步数不足时，禁止操作
    if (m_isGameOver || m_remainingMoves <= 0) return;

    Tile &t1 = m_board[p1.x()][p1.y()];
    Tile &t2 = m_board[p2.x()][p2.y()];

    std::swap(t1, t2); // 物理交换

    QSet<QPoint> killList;
    bool isBird1 = (t1.special == SpecialType::MagicBird);
    bool isBird2 = (t2.special == SpecialType::MagicBird);

    if (isBird1 || isBird2) {
        if (isBird1 && isBird2) {
            for(int r = 0; r < m_rows; ++r)
                for(int c = 0; c < m_cols; ++c)
                    killList.insert(QPoint(r, c));
        } else {
            int targetColor = isBird1 ? t2.color : t1.color;
            killList.insert(p1);
            killList.insert(p2);
            for(int r = 0; r < m_rows; ++r) {
                for(int c = 0; c < m_cols; ++c) {
                    if (m_board[r][c].color == targetColor) killList.insert(QPoint(r, c));
                }
            }
        }
    } else {
        killList = findMatches();
    }

    if (killList.isEmpty()) {
        std::swap(t1, t2); // 换回来
        emit invalidSwap(p1, p2);
        return;
    }

    // 👇 2. 补回：既然交换是合法的，扣减步数，并重置连击数！
    m_remainingMoves--;
    emit movesUpdated(m_remainingMoves);
    m_currentCombo = 1;

    // 👇 【核心修复】：如果是魔力鸟主动引爆，传入 (-1, -1) 剥夺其合成新特效的资格
    QPoint activePoint = (isBird1 || isBird2) ? QPoint(-1, -1) : p1;

    // 错误代码：executeElimination(killList, p1);
    // 正确修复：👇 把 p1 改成 activePoint ！！！
    executeElimination(killList, activePoint);
}

void GameLogic::executeElimination(QSet<QPoint> rawKillList, QPoint activePoint) {
    QSet<QPoint> finalKillList = expandKillList(rawKillList);

    // 👇 3. 补回：连击加分机制 (Combo)
    // 2. 加分逻辑
    int scoreGained = finalKillList.size() * 10 * m_currentCombo;
    m_currentScore += scoreGained;          // 👈 修改：换成 m_currentScore
    emit scoreUpdated(m_currentScore);      // 👈 修改：换成 scoreUpdated 信号

    // 👇 【核心修复】：细化特效合成逻辑
    if (activePoint.x() != -1 && rawKillList.size() >= 4) {
        SpecialType newSpecial = SpecialType::None;

        if (rawKillList.size() >= 5) {
            // 统计是否在同一行或同一列达到 5 个
            int sameRow = 0, sameCol = 0;
            for (const QPoint& p : rawKillList) {
                if (p.x() == activePoint.x()) sameRow++;
                if (p.y() == activePoint.y()) sameCol++;
            }

            if (sameRow >= 5 || sameCol >= 5) {
                newSpecial = SpecialType::MagicBird; // 觉醒波奇
            } else {
                newSpecial = SpecialType::Bomb;      // L型或T型 -> 鼓点爆炸
            }
        } else if (rawKillList.size() == 4) {
            // 4连消除，判断是横向连还是纵向连
            int sameRow = 0;
            for (const QPoint& p : rawKillList) {
                if (p.x() == activePoint.x()) sameRow++;
            }
            // 如果同一行有 4 个，说明是横排消除，通常生成纵向特效（或横向，可按需对调）
            newSpecial = (sameRow >= 4) ? SpecialType::LineVertical : SpecialType::LineHorizontal;
        }

        finalKillList.remove(activePoint);
        m_board[activePoint.x()][activePoint.y()].special = newSpecial;
        if(newSpecial == SpecialType::MagicBird) m_board[activePoint.x()][activePoint.y()].color = 0;
    }

    // 物理清除
    for (const QPoint& p : finalKillList) {
        m_board[p.x()][p.y()].color = 0;
        m_board[p.x()][p.y()].special = SpecialType::None;
    }

    emit boardExploded(finalKillList);
    applyGravityAndRefill();
    emit boardChanged();

    // Combo 连击检测引擎
    QSet<QPoint> comboMatches = findMatches();
    if (!comboMatches.isEmpty()) {
        // 👇 4. 补回：触发连击，Combo数+1
        m_currentCombo++;

        QTimer::singleShot(300, this, [=]() {
            executeElimination(comboMatches, QPoint(-1, -1));
        });
    } else {
        // ============== 回合彻底结束 ==============
        if (!hasValidMoves()) {
            shuffleBoard();
        }

        // 👇 5. 补回：一切尘埃落定后，检查是过关了还是步数耗尽了！
        checkGameStatus();

        emit turnEnded();
    }
}


// 连环引爆引擎：展开所有被卷入的特效
QSet<QPoint> GameLogic::expandKillList(QSet<QPoint> initialKillList) {
    QSet<QPoint> finalKillList;
    QQueue<QPoint> processQueue;

    // 将初始名单加入队列
    for (const QPoint& p : initialKillList) {
        processQueue.enqueue(p);
        finalKillList.insert(p);
    }

    // 广度优先遍历（BFS）处理连环爆炸
    while (!processQueue.isEmpty()) {
        QPoint p = processQueue.dequeue();
        Tile t = m_board[p.x()][p.y()];

        // 如果引爆的是横向特效
        if (t.special == SpecialType::LineHorizontal) {
            emit specialEffectTriggered(p, t.special); // 👈 补上这行通知UI
            for (int c = 0; c < m_cols; ++c) {
                QPoint newP(p.x(), c);
                if (!finalKillList.contains(newP)) {
                    finalKillList.insert(newP);
                    processQueue.enqueue(newP);
                }
            }
        }
        // 如果引爆的是纵向特效
        else if (t.special == SpecialType::LineVertical) {
            emit specialEffectTriggered(p, t.special); // 👈 补上这行通知UI
            for (int r = 0; r < m_rows; ++r) {
                QPoint newP(r, p.y());
                if (!finalKillList.contains(newP)) {
                    finalKillList.insert(newP);
                    processQueue.enqueue(newP);
                }
            }
        }
        // 如果引爆的是炸弹 (3x3 范围)
        else if (t.special == SpecialType::Bomb) {
            emit specialEffectTriggered(p, t.special); // 👈 补上这行通知UI
            for (int r = std::max(0, p.x() - 1); r <= std::min(m_rows - 1, p.x() + 1); ++r) {
                for (int c = std::max(0, p.y() - 1); c <= std::min(m_cols - 1, p.y() + 1); ++c) {
                    QPoint newP(r, c);
                    if (!finalKillList.contains(newP)) {
                        finalKillList.insert(newP);
                        processQueue.enqueue(newP);
                    }
                }
            }
        }
    }

    return finalKillList;
}



void GameLogic::applyGravityAndRefill() {
    // 1. 让悬空的方块掉落
    for (int c = 0; c < m_cols; ++c) {
        int emptyRow = m_rows - 1; // 从底部开始找空位
        for (int r = m_rows - 1; r >= 0; --r) {
            // ⬇️ 修改判断条件：只要不是空方块（包括魔力鸟），就受重力影响下落
            if (!m_board[r][c].isEmpty()) {
                // 如果发现实体方块，把它拉到最底部的空位上
                if (emptyRow != r) {
                    std::swap(m_board[emptyRow][c], m_board[r][c]);
                }
                emptyRow--;
            }
        }
    }

    // 2. 在顶部的空位生成新方块
    for (int r = 0; r < m_rows; ++r) {
        for (int c = 0; c < m_cols; ++c) {
            // ⬇️ 修改判断条件：只对真正的空方块进行填充
            if (m_board[r][c].isEmpty()) {
                // 使用之前建议的 QRandomGenerator
                m_board[r][c].color = QRandomGenerator::global()->bounded(1, GameConfig::COLOR_COUNT + 1);
                m_board[r][c].special = SpecialType::None;
            }
        }
    }
}


QSet<QPoint> GameLogic::triggerSpecialEffect(QPoint pos, SpecialType type) {
    QSet<QPoint> affected;
    int r = pos.x();
    int c = pos.y();

    switch (type) {
    case SpecialType::LineHorizontal: // 横向吉他扫弦
        for (int i = 0; i < GameConfig::BOARD_COLS; ++i) affected.insert(QPoint(r, i));
        break;

    case SpecialType::LineVertical: // 纵向吉他扫弦
        for (int i = 0; i < GameConfig::BOARD_ROWS; ++i) affected.insert(QPoint(i, c));
        break;

    case SpecialType::Bomb: // 鼓点爆炸（3x3 范围）
        for (int i = r - 1; i <= r + 1; ++i) {
            for (int j = c - 1; j <= c + 1; ++j) {
                if (i >= 0 && i < GameConfig::BOARD_ROWS && j >= 0 && j < GameConfig::BOARD_COLS)
                    affected.insert(QPoint(i, j));
            }
        }
        break;

    case SpecialType::MagicBird: // 觉醒波奇：同色全消
        // 这里通常消除与交换方块同色的所有方块
        int targetColor = m_board[r][c].color;
        for (int i = 0; i < GameConfig::BOARD_ROWS; ++i) {
            for (int j = 0; j < GameConfig::BOARD_COLS; ++j) {
                if (m_board[i][j].color == targetColor) affected.insert(QPoint(i, j));
            }
        }
        break;
    }

    // 通知 UI 播放对应的动画（比如画一道粉色激光）
    emit specialEffectTriggered(pos, type);
    return affected;
}



// 替换原有的 checkGameStatus
void GameLogic::checkGameStatus() {
    if (m_isGameOver) return;

    // 1. 检查是否刚刚达到目标分数 (触发狂欢时间 MV)
    if (!m_hasReachedTarget && m_currentScore >= m_targetScore) {
        m_hasReachedTarget = true;
        emit targetReached();
    }

    // 2. 真正的结束条件：无论分数多高，步数耗尽才结算！
    if (m_remainingMoves <= 0) {
        m_isGameOver = true;
        bool isWin = (m_currentScore >= m_targetScore);
        endAndSaveGame(isWin);
        emit levelFinished(isWin); // 通知 UI 弹出漂亮的结果面板
    }
}

QSet<QPoint> GameLogic::findMatches() {
    QSet<QPoint> matchSet;

    // 依然是你的核心魔法：定义赖子
    auto isWildcard = [](const Tile& t) {
        return t.special == SpecialType::Bomb ||
               t.special == SpecialType::LineHorizontal ||
               t.special == SpecialType::LineVertical;
    };

    // 👇 【核心优化】：提取一维滑动窗口扫描逻辑
    // getTile: 用于获取对应索引的方块 (抹平横向和纵向的差异)
    // addPoint: 用于将符合条件的坐标加入集合
    auto scanLine = [&](int length, auto getTile, auto addPoint) {
        for (int i = 0; i < length - 2; ) {
            int targetColor = -1;
            int j = i;

            // 1. 向前探索，找到最长的同色（或赖子）连续序列
            while (j < length) {
                Tile t = getTile(j);
                if (t.color == 0) break; // 遇到空格断开

                if (!isWildcard(t)) {
                    if (targetColor == -1) {
                        targetColor = t.color; // 确立基准颜色
                    } else if (targetColor != t.color) {
                        break; // 颜色冲突，序列结束
                    }
                }
                j++;
            }

            // 2. 结算这个序列：如果长度 >= 3，视为有效消除
            if (j - i >= 3) {
                for (int k = i; k < j; ++k) {
                    addPoint(k);
                }

                // 3. 【高阶性能优化 & 赖子桥接保留】
                // 优化：跳过已经扫描过的方块，避免重复计算。
                // 细节：如果序列末尾是赖子，它可能是下一个颜色的桥梁 (例如: 红-红-赖子-蓝-蓝)
                // 所以我们让下一次扫描的起点退回到序列末尾的连续赖子处。
                int next_i = j;
                while (next_i > i && isWildcard(getTile(next_i - 1))) {
                    next_i--;
                }

                // 兜底：如果全是赖子，强制前进一步防止死循环
                if (next_i == i) next_i++;

                i = next_i; // 直接让外层循环跳跃
            } else {
                i++; // 没凑够3个，正常前进一步
            }
        }
    };

    // ==========================================
    // 优雅调用：横向扫描 (Row Scan)
    // ==========================================
    for (int r = 0; r < GameConfig::BOARD_ROWS; ++r) {
        scanLine(GameConfig::BOARD_COLS,
                 [&](int c) { return m_board[r][c]; },                 // 怎么取方块
                 [&](int c) { matchSet.insert(QPoint(r, c)); });       // 怎么存坐标
    }

    // ==========================================
    // 优雅调用：纵向扫描 (Column Scan)
    // ==========================================
    for (int c = 0; c < GameConfig::BOARD_COLS; ++c) {
        scanLine(GameConfig::BOARD_ROWS,
                 [&](int r) { return m_board[r][c]; },                 // 怎么取方块
                 [&](int r) { matchSet.insert(QPoint(r, c)); });       // 怎么存坐标
    }

    return matchSet;
}

void GameLogic::refillBoard() {
    for (int r = 0; r < GameConfig::BOARD_ROWS; ++r) {
        for (int c = 0; c < GameConfig::BOARD_COLS; ++c) {
            if (m_board[r][c].color == 0) {
                // 随机填充波奇、虹夏等角色颜色
               m_board[r][c].color = QRandomGenerator::global()->bounded(1, 6);

                // 【核心修复】：使用你定义的 SpecialType::None
                m_board[r][c].special = SpecialType::None;

                m_board[r][c].isMatched = false;
            }
        }
    }
}


// 组合逻辑：特殊方块碰特殊方块
bool GameLogic::checkSpecialCombo(QPoint p1, QPoint p2) {
    Tile &t1 = m_board[p1.x()][p1.y()], &t2 = m_board[p2.x()][p2.y()];

    // 5+5 强力组合：全屏消除
    if (t1.special == SpecialType::MagicBird && t2.special == SpecialType::MagicBird) {
        for(int r=0; r<m_rows; ++r)
            for(int c=0; c<m_cols; ++c) m_board[r][c].color = 0;
        m_currentScore += 1000;
        return true;
    }

    // 魔力鸟 + 直线：全屏该颜色的方块全变直线并引爆
    if (t1.special == SpecialType::MagicBird && t2.special != SpecialType::None) {
        int colorToTransform = t2.color;
        for(int r=0; r<m_rows; ++r) {
            for(int c=0; c<m_cols; ++c) {
                if(m_board[r][c].color == colorToTransform) {
                    m_board[r][c].special = t2.special;
                    // 这里可以触发连锁...
                }
            }
        }
        return true;
    }
    return false;
}

/**
 * @brief 游戏结束结算
 * 核心：在这里调用你 DBHelper 里的 recordGameResult
 */
void GameLogic::endAndSaveGame(bool isWin) {
    // 👇 1. 关键修复：彻底锁死游戏状态，强制引擎停转！
    m_isGameOver = true;

    // 👇 2. 关键修复：如果是 AI 机器人，绝对不能存数据库，直接下班！
    if (m_isBot) return;

    int duration = m_startTime.secsTo(QDateTime::currentDateTime());

    // 直接使用你代码中的单例和事务逻辑
    bool success = DBHelper::getInstance().recordGameResult(
        m_currentUserId,
        m_currentScore,
        m_currentMode,
        isWin,
        duration
        );

    if (success) {
        qDebug() << ">>> 战绩已成功同步至 MySQL";
    }
    emit gameOver(m_currentScore);
}


// AI 模拟交换并评分（权重）
bool GameLogic::simulateSwapAndCheck(QPoint p1, QPoint p2, int &scoreOut) {
    Tile &t1 = m_board[p1.x()][p1.y()];
    Tile &t2 = m_board[p2.x()][p2.y()];

    // ---------------------------------------------------------
    // 优化 1：魔力鸟“免检”特权与动态高智商评分
    // ---------------------------------------------------------
    bool isBirdT1 = (t1.special == SpecialType::MagicBird);
    bool isBirdT2 = (t2.special == SpecialType::MagicBird);

    if (isBirdT1 || isBirdT2) {
        // 如果两个都是魔力鸟，那是毁天灭地的全屏消除，直接给最高权重
        if (isBirdT1 && isBirdT2) {
            scoreOut = 5000;
            return true;
        }

        // 如果只有一个魔力鸟，找出它要吸收的目标颜色
        int targetColor = isBirdT1 ? t2.color : t1.color;

        // 动态评估：盘面上有多少个这个颜色的方块？
        int colorCount = 0;
        for(int r = 0; r < m_rows; ++r) {
            for(int c = 0; c < m_cols; ++c) {
                if (m_board[r][c].color == targetColor) {
                    colorCount++;
                }
            }
        }
        // 基础高分 1000 + 动态消灭加成
        scoreOut = 1000 + (colorCount * 20);
        return true; // 魔力鸟直接放行，不需要调用耗时的 findMatches()
    }

    // ---------------------------------------------------------
    // 优化 2：普通方块的老实检测
    // ---------------------------------------------------------
    std::swap(t1, t2); // 物理交换
    QSet<QPoint> matches = findMatches();
    std::swap(t1, t2); // 恢复原状

    if (matches.isEmpty()) {
        return false;
    }

    scoreOut = matches.size() * 10;

    // 如果能合成特效，大幅加分
    if (matches.size() >= 5) scoreOut += 500;
    else if (matches.size() == 4) scoreOut += 200;

    return true;
}

void GameLogic::triggerNextBotMove() {
    if (!m_isBot || m_isGameOver) return;

    struct Move { QPoint p1, p2; int weight; };
    std::vector<Move> moves;

    // 遍历收集所有可行步
    for (int r = 0; r < m_rows; ++r) {
        for (int c = 0; c < m_cols; ++c) {
            int w = 0;
            if (c < m_cols - 1 && simulateSwapAndCheck({r, c}, {r, c + 1}, w)) {
                moves.push_back({{r, c}, {r, c + 1}, w});
            }
            if (r < m_rows - 1 && simulateSwapAndCheck({r, c}, {r + 1, c}, w)) {
                moves.push_back({{r, c}, {r + 1, c}, w});
            }
        }
    }

    if (!moves.empty()) {
        // 根据智商排序 (权重从大到小)
        std::sort(moves.begin(), moves.end(), [](const Move& a, const Move& b){
            return a.weight > b.weight;
        });

        int idx = 0;
        // ---------------------------------------------------------
        // 优化 3：使用 QRandomGenerator 替代 rand()
        // ---------------------------------------------------------
        if (m_aiDifficulty == AIDifficulty::Easy) {
            // 呆萌模式：完全随机选一个合法的步子
            idx = QRandomGenerator::global()->bounded(static_cast<int>(moves.size()));
        }
        else if (m_aiDifficulty == AIDifficulty::Normal) {
            // 普通模式：在前 50% 较好的步子里随机挑一个 (保留一点操作瑕疵)
            int halfSize = std::max(1, static_cast<int>(moves.size()) / 2);
            idx = QRandomGenerator::global()->bounded(halfSize);
        }
        else {
            // 疯狂模式：永远拿 weight 最高的第一名
            idx = 0;
        }

        emit aiMoveDecided(moves[idx].p1, moves[idx].p2);
    }
}
// 检查当前盘面是否还有活路
bool GameLogic::hasValidMoves() {
    int dummyScore = 0;
    for (int r = 0; r < m_rows; ++r) {
        for (int c = 0; c < m_cols; ++c) {
            // 只要找到一种合法的交换，就说明不是死局，立刻返回 true
            if (c < m_cols - 1 && simulateSwapAndCheck({r, c}, {r, c + 1}, dummyScore)) return true;
            if (r < m_rows - 1 && simulateSwapAndCheck({r, c}, {r + 1, c}, dummyScore)) return true;
        }
    }
    return false; // 找遍全图也没有合法步子，死局！
}

// 洗牌逻辑（打乱现有方块，但不改变它们原本的颜色数量）
void GameLogic::shuffleBoard() {
    std::vector<QPoint> validPositions;
    for (int r = 0; r < m_rows; ++r) {
        for (int c = 0; c < m_cols; ++c) {
            // 仅打乱普通颜色的方块，特殊方块（炸弹/魔力鸟）位置保留
            if (m_board[r][c].special == SpecialType::None && m_board[r][c].color != 0) {
                validPositions.push_back({r, c});
            }
        }
    }

    // 随机交换位置
    for (int i = 0; i < validPositions.size(); ++i) {
        int swapIdx = rand() % validPositions.size();
        std::swap(m_board[validPositions[i].x()][validPositions[i].y()],
                  m_board[validPositions[swapIdx].x()][validPositions[swapIdx].y()]);
    }

    // 如果洗完牌还是死局，或者洗出了立刻爆炸的三连，递归重新洗牌
    if (!findMatches().isEmpty() || !hasValidMoves()) {
        shuffleBoard();
    } else {
        emit boardChanged(); // 通知 UI 刷新全图
    }
}


void GameLogic::setRemainingMoves(int moves) {
    m_remainingMoves = moves;
    emit movesUpdated(m_remainingMoves);
}
