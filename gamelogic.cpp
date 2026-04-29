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

    // 1. 物理交换
    std::swap(m_board[p1.x()][p1.y()], m_board[p2.x()][p2.y()]);

    Tile &t1 = m_board[p1.x()][p1.y()];
    Tile &t2 = m_board[p2.x()][p2.y()];

    // 检测是不是魔力鸟
    bool isBirdT1 = (t1.special == SpecialType::MagicBird);
    bool isBirdT2 = (t2.special == SpecialType::MagicBird);

    // ===============================================
    // 💥 特权机制：只有“魔力鸟”可以直接免检放行并引爆全屏！
    // ===============================================
    if (isBirdT1 || isBirdT2) {
        m_remainingMoves--;
        emit movesUpdated(m_remainingMoves);
        m_currentCombo = 1;

        QSet<QPoint> toRemove;

        if (isBirdT1) {
            t1.color = t2.color; // 魔力鸟吸收对方的颜色
            toRemove.insert(p1);
        }
        if (isBirdT2) {
            t2.color = t1.color; // 魔力鸟吸收对方的颜色
            toRemove.insert(p2);
        }

        removeMatches(toRemove);
        handleMatchesAndRefill();
        checkGameStatus();
        return true;
    }

    // ===============================================
    // 其他方块 (包括炸弹、激光)：必须老老实实去测有没有形成三消！
    // ===============================================
    QSet<QPoint> matches = findMatches();
    if (matches.isEmpty()) {
        // 失败回滚：没凑齐三连消，退回去
        std::swap(m_board[p1.x()][p1.y()], m_board[p2.x()][p2.y()]);
        return false;
    }

    m_remainingMoves--;
    emit movesUpdated(m_remainingMoves);
    m_currentCombo = 1;

    processMatches(p1, false);
    processMatches(p2, false);

    handleMatchesAndRefill();
    checkGameStatus();
    return true;
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

    // 👇【核心魔法】：Lambda 表达式，定义哪些方块是“赖子（万能牌）”
    // 注意：故意把 MagicBird 排除在外，因为它走的是 swapTiles 里的免检引爆特权，不需要凑三消！
    auto isWildcard = [](const Tile& t) {
        return t.special == SpecialType::Bomb ||
               t.special == SpecialType::LineHorizontal ||
               t.special == SpecialType::LineVertical;
    };

    // 1. 横向扫描 (Row Scan)
    for (int r = 0; r < GameConfig::BOARD_ROWS; ++r) {
        for (int c = 0; c < GameConfig::BOARD_COLS - 2; ++c) {
            Tile t1 = m_board[r][c];
            Tile t2 = m_board[r][c+1];
            Tile t3 = m_board[r][c+2];

            // 遇到空格直接跳过
            if (t1.color == 0 || t2.color == 0 || t3.color == 0) continue;

            int targetColor = -1; // -1 表示当前还没有确立基准颜色
            bool match = true;
            Tile tiles[3] = {t1, t2, t3};

            // 🌟 赖子判定逻辑：检查连续三个方块是否能匹配
            for(int i = 0; i < 3; i++) {
                if(!isWildcard(tiles[i])) { // 如果不是赖子
                    if(targetColor == -1) {
                        targetColor = tiles[i].color; // 确立基准颜色 (比如第一个遇到的是红色)
                    } else if(targetColor != tiles[i].color) {
                        match = false; // 颜色冲突 (比如红-赖子-蓝)，匹配失败！
                        break;
                    }
                }
            }

            // 如果匹配成功
            if(match) {
                matchSet.insert(QPoint(r, c));
                matchSet.insert(QPoint(r, c+1));
                matchSet.insert(QPoint(r, c+2));

                // 向右继续检查 4 连或 5 连，赖子同样有效！
                int nextC = c + 3;
                while (nextC < GameConfig::BOARD_COLS) {
                    Tile nextT = m_board[r][nextC];
                    if (nextT.color == 0) break; // 遇到空格断开

                    // 如果是赖子，或者是基准颜色，继续连！
                    if (isWildcard(nextT) || targetColor == -1 || nextT.color == targetColor) {
                        matchSet.insert(QPoint(r, nextC));

                        // 如果之前全是赖子，现在终于遇到普通方块了，赶紧把基准颜色定下来
                        if (!isWildcard(nextT) && targetColor == -1) {
                            targetColor = nextT.color;
                        }
                        nextC++;
                    } else {
                        break; // 颜色不匹配，延伸结束
                    }
                }
            }
        }
    }

    // 2. 纵向扫描 (Column Scan) - 逻辑与横向完全一致
    for (int c = 0; c < GameConfig::BOARD_COLS; ++c) {
        for (int r = 0; r < GameConfig::BOARD_ROWS - 2; ++r) {
            Tile t1 = m_board[r][c];
            Tile t2 = m_board[r+1][c];
            Tile t3 = m_board[r+2][c];

            if (t1.color == 0 || t2.color == 0 || t3.color == 0) continue;

            int targetColor = -1;
            bool match = true;
            Tile tiles[3] = {t1, t2, t3};

            for(int i = 0; i < 3; i++) {
                if(!isWildcard(tiles[i])) {
                    if(targetColor == -1) {
                        targetColor = tiles[i].color;
                    } else if(targetColor != tiles[i].color) {
                        match = false;
                        break;
                    }
                }
            }

            if(match) {
                matchSet.insert(QPoint(r, c));
                matchSet.insert(QPoint(r+1, c));
                matchSet.insert(QPoint(r+2, c));

                int nextR = r + 3;
                while (nextR < GameConfig::BOARD_ROWS) {
                    Tile nextT = m_board[nextR][c];
                    if (nextT.color == 0) break;

                    if (isWildcard(nextT) || targetColor == -1 || nextT.color == targetColor) {
                        matchSet.insert(QPoint(nextR, c));
                        if (!isWildcard(nextT) && targetColor == -1) {
                            targetColor = nextT.color;
                        }
                        nextR++;
                    } else {
                        break;
                    }
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
void GameLogic::processMatches(QPoint triggerPos, bool triggerRefill) {
    if (triggerPos.x() < 0 || triggerPos.x() >= m_rows || triggerPos.y() < 0 || triggerPos.y() >= m_cols) return;

    int r = triggerPos.x();
    int c = triggerPos.y();
    if (m_board[r][c].color == 0) return; // 空方块跳过

    auto isWildcard = [](const Tile& t) {
        return t.special == SpecialType::Bomb ||
               t.special == SpecialType::LineHorizontal ||
               t.special == SpecialType::LineVertical;
    };

    // ==========================================
    // 1. 横向扫描 (智能滑动窗口推导法)
    // ==========================================
    int hColor = -1;
    bool hasHMatchWindow = false;
    // 扫描包含当前方块的 3 个可能的 3 连窗口
    for (int startC = c - 2; startC <= c; ++startC) {
        if (startC < 0 || startC + 2 >= m_cols) continue;
        Tile t1 = m_board[r][startC], t2 = m_board[r][startC+1], t3 = m_board[r][startC+2];
        if (t1.color == 0 || t2.color == 0 || t3.color == 0) continue;

        int tempColor = -1;
        bool match = true;
        Tile tiles[3] = {t1, t2, t3};
        for(int i=0; i<3; i++) {
            if(!isWildcard(tiles[i])) {
                if(tempColor == -1) tempColor = tiles[i].color;
                else if(tempColor != tiles[i].color) { match = false; break; }
            }
        }
        if (match) {
            hColor = tempColor;
            hasHMatchWindow = true; // 确认存在消除！
            break;
        }
    }

    std::vector<QPoint> horizontalMatches;
    if (hasHMatchWindow) {
        horizontalMatches.push_back(triggerPos);
        // 有了确定的 hColor，安全地向左收集
        for (int i = c - 1; i >= 0; --i) {
            Tile& t = m_board[r][i];
            if (t.color == 0) break;
            if (isWildcard(t) || hColor == -1 || t.color == hColor) {
                horizontalMatches.push_back(QPoint(r, i));
                if (!isWildcard(t) && hColor == -1) hColor = t.color; // 全赖子推导
            } else break;
        }
        // 安全地向右收集
        for (int i = c + 1; i < m_cols; ++i) {
            Tile& t = m_board[r][i];
            if (t.color == 0) break;
            if (isWildcard(t) || hColor == -1 || t.color == hColor) {
                horizontalMatches.push_back(QPoint(r, i));
                if (!isWildcard(t) && hColor == -1) hColor = t.color;
            } else break;
        }
    }

    // ==========================================
    // 2. 纵向扫描 (智能滑动窗口推导法)
    // ==========================================
    int vColor = -1;
    bool hasVMatchWindow = false;
    for (int startR = r - 2; startR <= r; ++startR) {
        if (startR < 0 || startR + 2 >= m_rows) continue;
        Tile t1 = m_board[startR][c], t2 = m_board[startR+1][c], t3 = m_board[startR+2][c];
        if (t1.color == 0 || t2.color == 0 || t3.color == 0) continue;

        int tempColor = -1;
        bool match = true;
        Tile tiles[3] = {t1, t2, t3};
        for(int i=0; i<3; i++) {
            if(!isWildcard(tiles[i])) {
                if(tempColor == -1) tempColor = tiles[i].color;
                else if(tempColor != tiles[i].color) { match = false; break; }
            }
        }
        if (match) {
            vColor = tempColor;
            hasVMatchWindow = true;
            break;
        }
    }

    std::vector<QPoint> verticalMatches;
    if (hasVMatchWindow) {
        verticalMatches.push_back(triggerPos);
        for (int i = r - 1; i >= 0; --i) {
            Tile& t = m_board[i][c];
            if (t.color == 0) break;
            if (isWildcard(t) || vColor == -1 || t.color == vColor) {
                verticalMatches.push_back(QPoint(i, c));
                if (!isWildcard(t) && vColor == -1) vColor = t.color;
            } else break;
        }
        for (int i = r + 1; i < m_rows; ++i) {
            Tile& t = m_board[i][c];
            if (t.color == 0) break;
            if (isWildcard(t) || vColor == -1 || t.color == vColor) {
                verticalMatches.push_back(QPoint(i, c));
                if (!isWildcard(t) && vColor == -1) vColor = t.color;
            } else break;
        }
    }

    bool isHMatch = horizontalMatches.size() >= 3;
    bool isVMatch = verticalMatches.size() >= 3;

    // 【死循环终结者】：不满足三连，直接退出
    if (!isHMatch && !isVMatch) return;

    // ==========================================
    // 3. 判定生成的特殊类型
    // ==========================================
    SpecialType generatedType = SpecialType::None;
    if (horizontalMatches.size() >= 5 || verticalMatches.size() >= 5) generatedType = SpecialType::MagicBird;
    else if (isHMatch && isVMatch) generatedType = SpecialType::Bomb;
    else if (horizontalMatches.size() == 4) generatedType = SpecialType::LineHorizontal;
    else if (verticalMatches.size() == 4) generatedType = SpecialType::LineVertical;

    // ==========================================
    // 4. 收集点并处理连锁反应 (特效引爆)
    // ==========================================
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

            // 让视觉效果跟上
            QEventLoop loop;
            QTimer::singleShot(200, &loop, &QEventLoop::quit);
            loop.exec();

            QSet<QPoint> affected = triggerSpecialEffect(p, sType);
            for(const QPoint& ap : affected){
                if(!totalToRemove.contains(ap)){
                    totalToRemove.insert(ap);
                    toProcess.append(ap);
                }
            }
            m_board[p.x()][p.y()].special = SpecialType::None; // 防止重复引爆
        }
    }

    // ==========================================
    // 5. 执行物理消除与计分
    // ==========================================
    for (const QPoint& pos : totalToRemove) {
        if (generatedType != SpecialType::None && pos.x() == r && pos.y() == c) continue;

        m_board[pos.x()][pos.y()].color = 0;
        m_board[pos.x()][pos.y()].special = SpecialType::None;

        int baseScore = 10;
        int comboBonus = (m_currentCombo > 1) ? (m_currentCombo - 1) * 5 : 0;
        m_currentScore += (baseScore + comboBonus);
    }

    // ==========================================
    // 6. 生成新的特殊方块
    // ==========================================
    if (generatedType != SpecialType::None) {
        int finalColor = 9; // 魔力鸟专属底色
        if (generatedType != SpecialType::MagicBird) {
            finalColor = isHMatch ? hColor : vColor;
            if (finalColor == -1) finalColor = (rand() % 5) + 1; // 全赖子消除的兜底
        }
        m_board[r][c].color = finalColor;
        m_board[r][c].special = generatedType;
    }

    emit scoreUpdated(m_currentScore);

    if (triggerRefill) {
        handleMatchesAndRefill(false);
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

// AI 模拟交换并评分（权重）
bool GameLogic::simulateSwapAndCheck(QPoint p1, QPoint p2, int &scoreOut) {
    std::swap(m_board[p1.x()][p1.y()], m_board[p2.x()][p2.y()]);
    QSet<QPoint> matches = findMatches();
    scoreOut = matches.size() * 10;

    // 如果能合成特效，大幅加分
    if (matches.size() >= 5) scoreOut += 500;
    else if (matches.size() == 4) scoreOut += 200;

    // 魔力鸟特权
    if (m_board[p1.x()][p1.y()].special == SpecialType::MagicBird ||
        m_board[p2.x()][p2.y()].special == SpecialType::MagicBird) scoreOut += 1000;

    std::swap(m_board[p1.x()][p1.y()], m_board[p2.x()][p2.y()]);
    return !matches.isEmpty() || scoreOut >= 1000;
}

void GameLogic::triggerNextBotMove() {
    if (!m_isBot || m_isGameOver) return;

    struct Move { QPoint p1, p2; int weight; };
    std::vector<Move> moves;

    for (int r = 0; r < m_rows; ++r) {
        for (int c = 0; c < m_cols; ++c) {
            int w = 0;
            if (c < m_cols - 1 && simulateSwapAndCheck({r, c}, {r, c + 1}, w)) moves.push_back({{r, c}, {r, c + 1}, w});
            if (r < m_rows - 1 && simulateSwapAndCheck({r, c}, {r + 1, c}, w)) moves.push_back({{r, c}, {r + 1, c}, w});
        }
    }

    if (!moves.empty()) {
        // 根据智商排序
        std::sort(moves.begin(), moves.end(), [](const Move& a, const Move& b){ return a.weight > b.weight; });

        int idx = 0;
        if (m_aiDifficulty == AIDifficulty::Easy) idx = rand() % moves.size(); // 随机乱点
        else if (m_aiDifficulty == AIDifficulty::Normal) idx = rand() % (std::max(1, (int)moves.size()/2)); // 挑凑合的
        else idx = 0; // 永远选最强的走法

        emit aiMoveDecided(moves[idx].p1, moves[idx].p2);
    }
}



void GameLogic::setRemainingMoves(int moves) {
    m_remainingMoves = moves;
    emit movesUpdated(m_remainingMoves);
}
