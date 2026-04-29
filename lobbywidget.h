#ifndef LOBBYWIDGET_H
#define LOBBYWIDGET_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include "Global.h"
#include <QComboBox>
#include <QStackedLayout> // 【新增】：引入栈布局

class LobbyWidget : public QWidget {
    Q_OBJECT
public:
    explicit LobbyWidget(UserSession session, QWidget *parent = nullptr);
    void refreshScore();

signals:
    // 当玩家选择某种模式时发出信号
    // 【修改】：增加 AIDifficulty 参数，默认值为 Normal
    void modeSelected(UserSession session, GameMode mode, AIDifficulty diff = AIDifficulty::Normal);

private slots:
    void onSingleClicked();
    void onAIClicked();
    void onOnlineClicked();

    // 【新增】：难度选择与返回的槽函数
    void onEasyClicked();
    void onNormalClicked();
    void onHardClicked();
    void onBackToMenuClicked();

private:
    UserSession m_session;
    QLabel *m_scoreLabel;

    // 【新增】：控制内嵌页面切换的核心组件
    QStackedLayout *m_stackedLayout;
    QWidget *m_mainMenuWidget; // 第一页：主菜单
    QWidget *m_diffMenuWidget; // 第二页：难度选择

    void initUI();
};

#endif // LOBBYWIDGET_H
