#include "DBHelper.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QEventLoop>
#include <QDebug>

DBHelper& DBHelper::getInstance() {
    static DBHelper instance;
    return instance;
}

DBHelper::DBHelper() {
    // 默认指向你的本地 Python 服务器
    m_serverUrl = "http://127.0.0.1:5000";
}

DBHelper::~DBHelper() {
    disconnectDB();
}

// 🌟 核心封装函数：同步发送 HTTP POST 请求
QJsonObject DBHelper::sendPostRequest(const QString& endpoint, const QJsonObject& payload) {
    QNetworkAccessManager manager;
    QNetworkRequest request(QUrl(m_serverUrl + endpoint));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QByteArray postData = QJsonDocument(payload).toJson();
    QNetworkReply *reply = manager.post(request, postData);

    // 使用 QEventLoop 挂起，实现同步请求，避免去改各种 UI 的异步逻辑
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    QJsonObject result;
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray response = reply->readAll();
        result = QJsonDocument::fromJson(response).object();
    } else {
        qDebug() << ">>> [Error] HTTP 请求失败:" << reply->errorString();
        result["status"] = "error";
    }

    reply->deleteLater();
    return result;
}

/**
 * @brief 测试后端连通性
 * 现在这里不需要任何账号密码了，只是为了不打破 main.cpp 里的原有逻辑
 */
bool DBHelper::connectDB(const QString& /*host*/, int /*port*/, const QString& /*user*/, const QString& /*pwd*/, const QString& /*dbName*/) {
    QJsonObject res = sendPostRequest("/api/ping", QJsonObject());
    if (res["status"].toString() == "success") {
        qDebug() << ">>> [Success] 已成功连接到 Python 后端网关！";
        return true;
    }
    qDebug() << ">>> [Error] 无法连接到后端，请确保 server.py 正在运行！";
    return false;
}

void DBHelper::disconnectDB() {
    // API 是无状态的，这里无需执行任何操作
}

bool DBHelper::registerUser(const QString& username, const QString& password, const QString& nickname) {
    QJsonObject payload;
    payload["username"] = username;
    payload["password"] = password;
    payload["nickname"] = nickname;

    QJsonObject res = sendPostRequest("/api/register", payload);
    return res["status"].toString() == "success";
}

bool DBHelper::login(const QString& username, const QString& password, int& outUserId, QString& outNickname, int& outAvatarId, int& outTotalScore) {
    QJsonObject payload;
    payload["username"] = username;
    payload["password"] = password;

    QJsonObject res = sendPostRequest("/api/login", payload);

    if (res["status"].toString() == "success") {
        QJsonObject data = res["data"].toObject();
        outUserId = data["uid"].toInt();
        outNickname = data["nickname"].toString();
        outAvatarId = data["avatarId"].toInt();
        outTotalScore = data["totalScore"].toInt();
        return true;
    }
    return false;
}

bool DBHelper::logout(int userId) {
    QJsonObject payload;
    payload["userId"] = userId;
    QJsonObject res = sendPostRequest("/api/logout", payload);
    return res["status"].toString() == "success";
}

bool DBHelper::recordGameResult(int userId, int score, GameMode mode, bool isWin, int durationSec) {
    QJsonObject payload;
    payload["userId"] = userId;
    payload["score"] = score;
    payload["gameMode"] = static_cast<int>(mode);
    payload["isWin"] = isWin ? 1 : 0;
    payload["durationSec"] = durationSec;

    QJsonObject res = sendPostRequest("/api/record", payload);
    return res["status"].toString() == "success";
}

int DBHelper::getUserTotalScore(int userId) {
    QJsonObject payload;
    payload["userId"] = userId;
    QJsonObject res = sendPostRequest("/api/get_score", payload);

    if (res["status"].toString() == "success") {
        return res["score"].toInt();
    }
    return 0;
}
