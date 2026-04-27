#ifndef DBHELPER_H
#define DBHELPER_H

#include <QString>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDebug>
#include "Global.h"

class DBHelper {
public:
    // 获取单例实例
    static DBHelper& getInstance();

    // 禁止拷贝和赋值
    DBHelper(const DBHelper&) = delete;
    DBHelper& operator=(const DBHelper&) = delete;

    /**
     * @brief 初始化并连接数据库
     * @note 默认使用 "MySQL ODBC 8.0 Unicode Driver"，请确保系统已安装该驱动
     */
    [[nodiscard]] bool connectDB(const QString& host, int port, const QString& user,
                                 const QString& pwd, const QString& dbName);

    // 关闭数据库连接
    void disconnectDB();

    // --- 业务接口 ---

    // 用户注册
    [[nodiscard]] bool registerUser(const QString& username, const QString& password,
                                    const QString& nickname = "新玩家");

    // 用户登录
    [[nodiscard]] bool login(const QString& username, const QString& password,
                             int& outUserId, QString& outNickname);

    // 退出登录
    bool logout(int userId);

    /**
     * @brief 记录游戏战绩
     * @param gameMode 使用枚举类 GameMode
     */
    [[nodiscard]] bool recordGameResult(int userId, int score, GameMode mode,
                                        bool isWin, int durationSec);

private:
    // 私有构造函数 (单例模式要求)
    DBHelper();
    ~DBHelper();

    QSqlDatabase db;
};

#endif // DBHELPER_H
