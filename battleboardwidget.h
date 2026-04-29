#ifndef BATTLEBOARDWIDGET_H
#define BATTLEBOARDWIDGET_H

#include <QFrame>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGraphicsDropShadowEffect>
#include "GameLogic.h"
#include "GamePanel.h"

class BattleBoardWidget : public QFrame {
    Q_OBJECT
public:
    explicit BattleBoardWidget(QString playerName, bool isBot, QWidget *parent = nullptr);

    void startLevel(int targetScore, int maxMoves, int uid, GameMode mode);

    GameLogic* getLogic() const { return m_logic; }
    GamePanel* getPanel() const { return m_panel; }

signals:
    void targetReached();          // 通知 MainWindow 达到目标分数了
    void levelFinished(bool isWin);// 通知 MainWindow 游戏结束（步数耗尽等）

public slots:
    void executeMove(QPoint p1, QPoint p2); // 接收 AI 或网络传来的移动指令

private:
    GameLogic *m_logic;
    GamePanel *m_panel;

    QLabel *m_nameLabel;
    QLabel *m_scoreLabel;
    bool m_isBot;

    void setupUI(QString playerName);
    void initConnections();
    void applyTextShadow(QWidget* widget);
};

#endif // BATTLEBOARDWIDGET_H
