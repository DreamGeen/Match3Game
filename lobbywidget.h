#ifndef LOBBYWIDGET_H
#define LOBBYWIDGET_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include "Global.h"
#include <QComboBox>
#include <QStackedLayout> // 【新增】：引入栈布局
#include <QListWidget>        // 👈 新增：房间列表控件
#include <QUdpSocket>         // 👈 新增：UDP 监听雷达
#include <QNetworkDatagram>
#include <QJsonDocument>
#include <QJsonObject>


class LobbyWidget : public QWidget {
    Q_OBJECT
public:
    explicit LobbyWidget(UserSession session, QWidget *parent = nullptr);
    void refreshScore();

    // 👇 新增：外部调用此函数，可直接切回雷达页面并开启扫描
    void openRadar();

signals:
    // 当玩家选择某种模式时发出信号
    // 【修改】：增加 AIDifficulty 参数，默认值为 Normal
    void modeSelected(UserSession session, GameMode mode, AIDifficulty diff = AIDifficulty::Normal);

    // 👈 新增：网络对战专属信号（告诉主窗口是建房还是加入，加入的话IP是多少）
    void onlineGameRequested(UserSession session, bool isHost, QString targetIp = "");

private slots:
    void onSingleClicked();
    void onAIClicked();
    void onOnlineClicked();

    // 【新增】：难度选择与返回的槽函数
    void onEasyClicked();
    void onNormalClicked();
    void onHardClicked();
    void onBackToMenuClicked();

    // 👈 新增：联机大厅专属槽函数
    void onHostRoomClicked();             // 点击“创建 Livehouse”
    void onRoomSelected(QListWidgetItem *item); // 点击列表里的某个房间加入
    void onUdpReadyRead();                // 雷达收到 UDP 广播

private:
    UserSession m_session;
    QLabel *m_scoreLabel;

    // 【新增】：控制内嵌页面切换的核心组件
    QStackedLayout *m_stackedLayout;
    QWidget *m_mainMenuWidget; // 第一页：主菜单
    QWidget *m_diffMenuWidget; // 第二页：难度选择

    // 👈 新增：第 3 页联机大厅的组件
    QWidget *m_onlineMenuWidget;
    QListWidget *m_roomList;
    QUdpSocket *m_udpSocket;

    void initUI();
    void startScanning(); // 开启雷达
    void stopScanning();  // 关闭雷达
};

#endif // LOBBYWIDGET_H
