#include "GameLogic.h"
#include <QRandomGenerator>
#include <algorithm>
#include <QEventLoop>
#include <QTimer>

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




void GameLogic::removeMatches(const QSet<QPoint>& matches) {
    QSet<QPoint> totalMatches = matches; // 最终要消除的所有点
    QSet<QPoint> toProcess = matches;    // 待检查是否有特效的点
    QSet<QPoint> processed;              // 已处理过的特效点

    // 循环处理，直到没有新的特效被触发
    while (!toProcess.isEmpty()) {
        QPoint pos = *toProcess.begin();
        toProcess.remove(pos);
        processed.insert(pos);

        SpecialType type = m_board[pos.x()][pos.y()].special;

        if (type != SpecialType::None) {
            // --- 核心：触发特殊效果，获取波及范围 ---
            QSet<QPoint> affected = triggerSpecialEffect(pos, type);

            for (const QPoint& p : affected) {
                if (!totalMatches.contains(p)) {
                    totalMatches.insert(p);
                    if (!processed.contains(p)) {
                        toProcess.insert(p); // 如果波及到了另一个特殊方块，继续处理
                    }
                }
            }
        }
    }

    // 真正执行消除和计分
    for (const QPoint& pos : totalMatches) {
        m_board[pos.x()][pos.y()].color = 0;
        m_board[pos.x()][pos.y()].special = SpecialType::None; // 重置
    }

    m_currentScore += totalMatches.size() * 15; // 特效消除分数更高
    emit scoreUpdated(m_currentScore);
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


bool GameLogic::swapTiles(QPoint p1, QPoint p2) {
    if (m_isGameOver || m_remainingMoves <= 0) return false;

    // 1. 执行虚拟交换并检查匹配
    std::swap(m_board[p1.x()][p1.y()], m_board[p2.x()][p2.y()]);
    QSet<QPoint> matches = findMatches();

    if (matches.isEmpty()) {
        // 失败回滚
        std::swap(m_board[p1.x()][p1.y()], m_board[p2.x()][p2.y()]);
        return false;
    }

    // 2. 核心修改：交换成功扣除步数
    m_remainingMoves--;
    emit movesUpdated(m_remainingMoves);

    // 3. 执行消除逻辑
    m_currentCombo = 1;
    processMatches(p1);
    processMatches(p2);
    handleMatchesAndRefill();

    // 4. 检查胜负状态
    checkGameStatus();

    return true;
}


void GameLogic::checkGameStatus() {
    if (m_isGameOver) return;

    // 胜利判定：达到分数
    if (m_currentScore >= m_targetScore) {
        m_isGameOver = true;
        endAndSaveGame(true); // 物理保存战绩
        emit levelFinished(true);
    }
    // 失败判定：步数用尽且分数未达标
    else if (m_remainingMoves <= 0) {
        m_isGameOver = true;
        endAndSaveGame(false);
        emit levelFinished(false);
    }
}

QSet<QPoint> GameLogic::findMatches() {
    QSet<QPoint> matchSet;

    // 1. 横向扫描 (Row Scan)
    for (int r = 0; r < GameConfig::BOARD_ROWS; ++r) {
        for (int c = 0; c < GameConfig::BOARD_COLS - 2; ++c) {
            int color = m_board[r][c].color;
            if (color == 0) continue; // 跳过空格

            // 检查连续三个是否同色
            if (m_board[r][c+1].color == color && m_board[r][c+2].color == color) {
                matchSet.insert(QPoint(r, c));
                matchSet.insert(QPoint(r, c+1));
                matchSet.insert(QPoint(r, c+2));

                // 处理 4 连或 5 连的情况
                int nextC = c + 3;
                while (nextC < GameConfig::BOARD_COLS && m_board[r][nextC].color == color) {
                    matchSet.insert(QPoint(r, nextC));
                    nextC++;
                }
            }
        }
    }

    // 2. 纵向扫描 (Column Scan)
    for (int c = 0; c < GameConfig::BOARD_COLS; ++c) {
        for (int r = 0; r < GameConfig::BOARD_ROWS - 2; ++r) {
            int color = m_board[r][c].color;
            if (color == 0) continue;

            if (m_board[r+1][c].color == color && m_board[r+2][c].color == color) {
                matchSet.insert(QPoint(r, c));
                matchSet.insert(QPoint(r+1, c));
                matchSet.insert(QPoint(r+2, c));

                int nextR = r + 3;
                while (nextR < GameConfig::BOARD_ROWS && m_board[nextR][c].color == color) {
                    matchSet.insert(QPoint(nextR, c));
                    nextR++;
                }
            }
        }
    }

    return matchSet;
}


void GameLogic::refillBoard() {
    for (int r = 0; r < GameConfig::BOARD_ROWS; ++r) {
        for (int c = 0; c < GameConfig::BOARD_COLS; ++c) {
            if (m_board[r][c].color == 0) {
                // 随机填充波奇、虹夏等角色颜色
                m_board[r][c].color = (rand() % 5) + 1;

                // 【核心修复】：使用你定义的 SpecialType::None
                m_board[r][c].special = SpecialType::None;

                m_board[r][c].isMatched = false;
            }
        }
    }
}





// 递归处理消除效果
void GameLogic::triggerTileEffect(int r, int c, std::set<std::pair<int, int>>& toRemove) {
    if (r < 0 || r >= m_rows || c < 0 || c >= m_cols || toRemove.count({r, c})) return;

    toRemove.insert({r, c});
    Tile& t = m_board[r][c];

    if (t.special == SpecialType::LineHorizontal) {
        for (int j = 0; j < m_cols; ++j) triggerTileEffect(r, j, toRemove);
    }
    else if (t.special == SpecialType::LineVertical) {
        for (int i = 0; i < m_rows; ++i) triggerTileEffect(i, c, toRemove);
    }
    else if (t.special == SpecialType::Bomb) {
        for (int i = r - 1; i <= r + 1; ++i)
            for (int j = c - 1; j <= c + 1; ++j) triggerTileEffect(i, j, toRemove);
    }
    // 魔力鸟通常在交换时即时触发，不在此递归
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

void GameLogic::processMatches(QPoint triggerPos, bool triggerRefill) {
    qDebug() << ">>> [ProcessMatches] 启动！检查坐标: (" << triggerPos.x() << "," << triggerPos.y() << ")";

    if (triggerPos.x() < 0 || triggerPos.x() >= m_rows || triggerPos.y() < 0 || triggerPos.y() >= m_cols) return;

    int r = triggerPos.x();
    int c = triggerPos.y();
    int targetColor = m_board[r][c].color;
    if (targetColor == 0) return;

    // 1. 扫描匹配 (这部分保持不变)
    std::vector<QPoint> horizontalMatches;
    horizontalMatches.push_back(triggerPos);
    for (int i = c - 1; i >= 0 && m_board[r][i].color == targetColor; --i) horizontalMatches.push_back(QPoint(r, i));
    for (int i = c + 1; i < m_cols && m_board[r][i].color == targetColor; ++i) horizontalMatches.push_back(QPoint(r, i));

    std::vector<QPoint> verticalMatches;
    verticalMatches.push_back(triggerPos);
    for (int i = r - 1; i >= 0 && m_board[i][c].color == targetColor; --i) verticalMatches.push_back(QPoint(i, c));
    for (int i = r + 1; i < m_rows && m_board[i][c].color == targetColor; ++i) verticalMatches.push_back(QPoint(i, c));

    bool isHMatch = horizontalMatches.size() >= 3;
    bool isVMatch = verticalMatches.size() >= 3;

    // 【核心约束】：如果不满三连，不产生新方块也不引爆旧方块
    if (!isHMatch && !isVMatch) {
        // 【关键新增】：如果玩家交换了方块但没有形成任何匹配，
        // 说明这一回合结束了，需要重置连击。
        // 但注意，如果是被连锁反应调用到的空点，不能重置。
        // 通常玩家无效交换会在 swapTiles 里就被拦下来并退回，
        // 所以这里直接 return 影响不大，重置连击的主要逻辑放在后面。
        return;
    }

    // 2. 判定生成的特殊类型 (保持不变)
    SpecialType generatedType = SpecialType::None;
    if (horizontalMatches.size() >= 5 || verticalMatches.size() >= 5) generatedType = SpecialType::MagicBird;
    else if (isHMatch && isVMatch) generatedType = SpecialType::Bomb;
    else if (horizontalMatches.size() == 4) generatedType = SpecialType::LineHorizontal;
    else if (verticalMatches.size() == 4) generatedType = SpecialType::LineVertical;

    // 3. 收集点并处理连锁反应 (这部分保持不变)
    QSet<QPoint> totalToRemove;
    QList<QPoint> toProcess;

    if (isHMatch) for (auto p : horizontalMatches) { totalToRemove.insert(p); toProcess.append(p); }
    if (isVMatch) for (auto p : verticalMatches) { totalToRemove.insert(p); toProcess.append(p); }

    int index = 0;
    while(index < toProcess.size()){
        QPoint p = toProcess.at(index++);
        SpecialType sType = m_board[p.x()][p.y()].special;

        if(sType != SpecialType::None){
            emit specialEffectTriggered(p, sType);

            QEventLoop loop;
            QTimer::singleShot(250, &loop, &QEventLoop::quit);
            loop.exec();

            // 假设你有一个 triggerSpecialEffect 函数 (这里伪代码略过具体实现)
            QSet<QPoint> affected = triggerSpecialEffect(p, sType);
            for(const QPoint& ap : affected){
                if(!totalToRemove.contains(ap)){
                    totalToRemove.insert(ap);
                    toProcess.append(ap);
                }
            }
            m_board[p.x()][p.y()].special = SpecialType::None;
        }
    }

    qDebug() << "    [消除]: 连击成功，消除点数 =" << totalToRemove.size();


    // 4. 执行物理消除
    for (const QPoint& pos : totalToRemove) {
        if (generatedType != SpecialType::None && pos.x() == r && pos.y() == c) continue;

        m_board[pos.x()][pos.y()].color = 0;
        m_board[pos.x()][pos.y()].special = SpecialType::None;

        // 【新增】：分数加上连击加成！比如基础 10 分，每次连击多 5 分
        int baseScore = 10;
        int comboBonus = (m_currentCombo > 1) ? (m_currentCombo - 1) * 5 : 0;
        m_currentScore += (baseScore + comboBonus);
    }

    // 5. 写入新生成的特殊属性 (保持不变)
    if (generatedType != SpecialType::None) {
        m_board[r][c].color = (generatedType == SpecialType::MagicBird) ? 9 : targetColor;
        m_board[r][c].special = generatedType;
    }

    emit scoreUpdated(m_currentScore);

    // 根据传入的参数决定是否立即下落
    if (triggerRefill) {
        handleMatchesAndRefill(false);
    }
}

void GameLogic::handleMatchesAndRefill(bool isFirstMatchInTurn) {
    bool hasNewMatches = true;

    // 注意：isFirstMatchInTurn 相关重置逻辑已经移到了 swapTiles 中，
    // 这里不再需要重置连击。

    while (hasNewMatches) {
        applyGravity();   // 1. 方块下落
        refillBoard();    // 2. 补齐空位
        emit boardChanged(); // 3. 告诉 UI 更新界面

        auto newMatches = findMatches();
        if (!newMatches.isEmpty()) {

            // ✅ 【新增】：发现新的掉落连锁，这属于新的一波！连击数 +1
            m_currentCombo++;
            emit comboUpdated(m_currentCombo);

            // ✅ 【关键逻辑】：处理这一波中**所有的**匹配组，不要漏掉！
            // 比如左边一个3连，右边一个3连，要把它们在同一波里全消掉
            while (!newMatches.isEmpty()) {
                QPoint triggerPoint = *newMatches.begin();
                processMatches(triggerPoint, false); // false = 不要立即递归下落
                // 消除一组后，重新扫描棋盘，看这一波还有没有剩下的
                newMatches = findMatches();
            }

            hasNewMatches = true; // 这一波消完了，需要重新走下落流程
        } else {
            hasNewMatches = false; // 棋盘彻底安静
        }
    }
}


void GameLogic::applyGravity() {
    // 简单的下落逻辑：每一列从底向上扫描，将非空方块下压
    for (int c = 0; c < m_cols; ++c) {
        int emptyRow = m_rows - 1;
        for (int r = m_rows - 1; r >= 0; --r) {
            if (m_board[r][c].color != 0) {
                std::swap(m_board[r][c], m_board[emptyRow][c]);
                emptyRow--;
            }
        }
    }
}
