#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonObject>
#include <QJsonDocument>
#include <QPoint>
#include <QUdpSocket> // 👈 新增 UDP
#include <QTimer>     // 👈 新增定时器

class NetworkManager : public QObject {
    Q_OBJECT
public:
    explicit NetworkManager(QObject *parent = nullptr);

    bool isHost() const { return m_isHost; }

    // 房主建房时，传入房间名（比如玩家昵称）
    bool hostGame(const QString& roomName, quint16 tcpPort = 8888);

    // 客机加入（IP 在后台默默传递）
    void joinGame(const QString &ip, quint16 tcpPort = 8888);

    // 👈 新增：客机开启雷达，搜索附近的房间
    void startSearchingRooms();
    void stopSearchingRooms();

    // 发送游戏操作（坐标交换）
    void sendSwapAction(QPoint p1, QPoint p2);
    // 发送随机数种子（仅房主调用）
    void sendGameStart(quint32 seed, const QString &nickname);
    // 发送实时分数
    void sendScoreUpdate(int score);

    // 发送认输指令
    void sendSurrender();

signals:
    void connectedToOpponent(); // 联机成功
    void disconnected();        // 对方掉线

    // 接收到对方指令的信号
    void opponentSwapped(QPoint p1, QPoint p2);
    void opponentScoreUpdated(int score);
    void gameStartReceived(quint32 seed, const QString &opponentName); // 接收到房主的开局指令和种子

    // 👈 新增：当搜索到附近有房间时触发，抛出给 UI 显示
    void roomFound(const QString &ip, const QString &roomName);

    void opponentSurrendered(); // 👈 新增：对方认输信号

private slots:
    void onNewConnection();
    void onReadyRead();
    void onSocketError(QAbstractSocket::SocketError error);

    // 👈 新增：UDP 相关的槽函数
    void broadcastRoomInfo();
    void onUdpReadyRead();

private:
    QTcpServer *m_server;
    QTcpSocket *m_socket;
    bool m_isHost = false;

    // 👈 新增 UDP 成员
    QUdpSocket *m_udpSocket;
    QTimer *m_broadcastTimer;
    QString m_myRoomName;
    quint16 m_udpPort = 8889; // UDP 广播专用端口

    void sendJson(const QJsonObject &json);
};

#endif
