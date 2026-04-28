#ifndef LOBBYWIDGET_H
#define LOBBYWIDGET_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include "Global.h"

class LobbyWidget : public QWidget {
    Q_OBJECT
public:
    explicit LobbyWidget(UserSession session, QWidget *parent = nullptr);
    void refreshScore();

signals:
    // 当玩家选择某种模式时发出信号
    void modeSelected(UserSession session, GameMode mode);

private slots:
    void onSingleClicked();
    void onAIClicked();
    void onOnlineClicked();

private:
    UserSession m_session;
    QLabel *m_scoreLabel;
    void initUI();
};

#endif // LOBBYWIDGET_H
