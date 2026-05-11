#ifndef DBHELPER_H
#define DBHELPER_H

#include <QString>
#include <QJsonObject>
#include "Global.h"

class DBHelper {
public:
    static DBHelper& getInstance();
    DBHelper(const DBHelper&) = delete;
    DBHelper& operator=(const DBHelper&) = delete;

    // 连接测试 (现在它只负责发个请求问问 Python 后端在不在)
    [[nodiscard]] bool connectDB(const QString& host, int port, const QString& user,
                                 const QString& pwd, const QString& dbName);

    void disconnectDB();

    // 业务接口保持原样，无需修改上层逻辑
    [[nodiscard]] bool registerUser(const QString& username, const QString& password,
                                    const QString& nickname = "新玩家");

    [[nodiscard]] bool login(const QString& username, const QString& password,
                             int& outUserId, QString& outNickname,
                             int& outAvatarId, int& outTotalScore);

    bool logout(int userId);

    [[nodiscard]] bool recordGameResult(int userId, int score, GameMode mode,
                                        bool isWin, int durationSec);

    int getUserTotalScore(int userId);

private:
    DBHelper();
    ~DBHelper();

    // 🌟 核心封装：负责把所有的操作转换为发给 Python 的 POST 请求
    QJsonObject sendPostRequest(const QString& endpoint, const QJsonObject& payload);

    QString m_serverUrl; // 后端 API 的基础地址
};

#endif // DBHELPER_H
