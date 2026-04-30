#include "networkmanager.h"
#include <QJsonParseError>
#include <QDebug>
#include <QNetworkDatagram>

NetworkManager::NetworkManager(QObject *parent) : QObject(parent) {
    m_server = new QTcpServer(this);
    m_socket = nullptr;

    // 初始化 UDP 组件
    m_udpSocket = new QUdpSocket(this);
    m_broadcastTimer = new QTimer(this);
    connect(m_broadcastTimer, &QTimer::timeout, this, &NetworkManager::broadcastRoomInfo);

    connect(m_server, &QTcpServer::newConnection, this, &NetworkManager::onNewConnection);
}

// ==========================================
// 🎵 房主逻辑：建房并开始大喇叭广播
// ==========================================
bool NetworkManager::hostGame(const QString& roomName, quint16 tcpPort) {
    m_isHost = true;
    m_myRoomName = roomName;

    // 1. 开启 TCP 监听实际的游戏数据
    if (!m_server->listen(QHostAddress::Any, tcpPort)) return false;

    // 2. 开启 UDP 广播（每秒喊一次）
    m_broadcastTimer->start(1000);
    return true;
}


void NetworkManager::broadcastRoomInfo() {
    // 打包房间信息
    QJsonObject json;
    json["type"] = "room_broadcast";
    json["roomName"] = m_myRoomName;

    QByteArray data = QJsonDocument(json).toJson(QJsonDocument::Compact);

    // 👇 修改这里：采用“双轨广播机制”

    // 1. 向真实的局域网内广播（用于两台不同电脑联机）
    m_udpSocket->writeDatagram(data, QHostAddress::Broadcast, m_udpPort);

    // 2. 强制向本机回环地址广播（专为毕业设计单机双开展示准备！）
    m_udpSocket->writeDatagram(data, QHostAddress::LocalHost, m_udpPort);
}

// 房主等到有人连进来后，立刻闭嘴，不再广播
void NetworkManager::onNewConnection() {
    if (m_server->hasPendingConnections()) {
        m_broadcastTimer->stop(); // 👈 关键修复：有人进房间了，赶紧关掉大喇叭！

        m_socket = m_server->nextPendingConnection();
        connect(m_socket, &QTcpSocket::readyRead, this, &NetworkManager::onReadyRead);
        connect(m_socket, &QTcpSocket::disconnected, this, &NetworkManager::disconnected);

        m_server->close(); // 停止监听，只允许1V1
        emit connectedToOpponent();
    }
}



// ==========================================
// 🔍 客机逻辑：开启雷达搜索房间
// ==========================================
void NetworkManager::startSearchingRooms() {
    // 绑定 UDP 端口开始听声音 (ShareAddress 允许多个程序同听)
    m_udpSocket->bind(m_udpPort, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
    connect(m_udpSocket, &QUdpSocket::readyRead, this, &NetworkManager::onUdpReadyRead);
}

void NetworkManager::stopSearchingRooms() {
    m_udpSocket->close();
    disconnect(m_udpSocket, &QUdpSocket::readyRead, this, &NetworkManager::onUdpReadyRead);
}

void NetworkManager::onUdpReadyRead() {
    while (m_udpSocket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = m_udpSocket->receiveDatagram();
        QByteArray data = datagram.data();

        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(data, &error);

        if (error.error == QJsonParseError::NoError && doc.isObject()) {
            QJsonObject json = doc.object();
            if (json["type"].toString() == "room_broadcast") {
                // 提取房间名，并抓取发送这个包的源 IP 地址！
                QString roomName = json["roomName"].toString();
                QString hostIp = datagram.senderAddress().toString();

                // 去除 IPv6 的前缀，使其看起来更清爽（可选）
                hostIp.replace("::ffff:", "");

                // 把 IP 和 房间名 扔给 UI 界面去显示
                emit roomFound(hostIp, roomName);
            }
        }
    }
}
void NetworkManager::joinGame(const QString &ip, quint16 port) {
    m_isHost = false;
    stopSearchingRooms(); // 开始连 TCP 了，就不搜了

    m_socket = new QTcpSocket(this);
    connect(m_socket, &QTcpSocket::connected, this, &NetworkManager::connectedToOpponent);
    connect(m_socket, &QTcpSocket::readyRead, this, &NetworkManager::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &NetworkManager::disconnected);
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred), this, &NetworkManager::onSocketError);

    m_socket->connectToHost(ip, port);
}


void NetworkManager::sendJson(const QJsonObject &json) {
    if (m_socket && m_socket->state() == QAbstractSocket::ConnectedState) {
        QJsonDocument doc(json);
        QByteArray data = doc.toJson(QJsonDocument::Compact) + "\n"; // 以换行符作为包边界
        m_socket->write(data);
    }
}

void NetworkManager::sendSwapAction(QPoint p1, QPoint p2) {
    QJsonObject json;
    json["type"] = "swap";
    json["x1"] = p1.x(); json["y1"] = p1.y();
    json["x2"] = p2.x(); json["y2"] = p2.y();
    sendJson(json);
}

void NetworkManager::sendGameStart(quint32 seed,const QString &name) {
    QJsonObject json;
    json["type"] = "start";
    json["seed"] = (qint64)seed;
    json["nickname"] = name; // 👈 加上这一行
    sendJson(json);
}

void NetworkManager::sendScoreUpdate(int score) {
    QJsonObject json;
    json["type"] = "score";
    json["score"] = score;
    sendJson(json);
}

void NetworkManager::onReadyRead() {
    // 简单处理粘包，按行读取
    while (m_socket->canReadLine()) {
        QByteArray data = m_socket->readLine();
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(data, &error);

        if (error.error == QJsonParseError::NoError && doc.isObject()) {
            QJsonObject json = doc.object();
            QString type = json["type"].toString();

            if (type == "swap") {
                emit opponentSwapped(QPoint(json["x1"].toInt(), json["y1"].toInt()),
                                     QPoint(json["x2"].toInt(), json["y2"].toInt()));
            } else if (type == "start") {
                quint32 seed = json["seed"].toInt();
                QString oppName = json["nickname"].toString(); // 👈 提取昵称
                emit gameStartReceived(seed, oppName); // 👈 传给信号
            } else if (type == "score") {
                emit opponentScoreUpdated(json["score"].toInt());
            }else if (type == "surrender") { // 👈 新增认输解析
                emit opponentSurrendered();
            }else if (type == "like") {  // 👇 新增：如果收到对方的点赞指令
                emit opponentLiked();
            }
        }
    }
}

void NetworkManager::onSocketError(QAbstractSocket::SocketError error) {
    qDebug() << "Network Error:" << error << m_socket->errorString();
    // 弹个窗让你一眼看出死因
    // QMessageBox::warning(nullptr, "连接异常", QString("网络报错：%1").arg(m_socket->errorString()));
}

void NetworkManager::sendSurrender() {
    QJsonObject json;
    json["type"] = "surrender";
    sendJson(json);
}

void NetworkManager::sendLike() {
    QJsonObject json;
    json["type"] = "like";
    sendJson(json);
}
