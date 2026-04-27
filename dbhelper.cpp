#include "DBHelper.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

// 获取单例对象
DBHelper& DBHelper::getInstance() {
    static DBHelper instance;
    return instance;
}

// 私有构造函数
DBHelper::DBHelper() {
    // 关键点 1：彻底放弃 QMYSQL 字符串，直接初始化为 QODBC
    // 检查是否已经存在默认连接，避免重复添加导致 Qt 报 "Duplicate connection" 警告
    if (QSqlDatabase::contains(QSqlDatabase::defaultConnection)) {
        db = QSqlDatabase::database(QSqlDatabase::defaultConnection);
    } else {
        db = QSqlDatabase::addDatabase("QODBC");
    }
}

DBHelper::~DBHelper() {
    disconnectDB();
}

/**
 * @brief 连接数据库
 * 使用 ODBC 桥接方式解决驱动问题
 */
bool DBHelper::connectDB(const QString& host, int port, const QString& user, const QString& pwd, const QString& dbName) {
    // 如果当前连接已打开，先关闭以刷新 DSN 配置
    if (db.isOpen()) {
        db.close();
    }

    // 关键点 2：构造 ODBC 连接字符串
    // Driver 的名字必须与你电脑中“ODBC 数据源管理器”里安装的名字完全一致
    // CharSet=utf8mb4 确保支持中文，Option=3 是官方推荐的兼容性参数
    QString dsn = QString("Driver={MySQL ODBC 9.6 Unicode Driver};"
                          "Server=%1;"
                          "Port=%2;"
                          "Database=%3;"
                          "Uid=%4;"
                          "Pwd=%5;"
                         "CHARSET=UTF8;"
                    "STMT=SET NAMES utf8mb4;" // 某些版本支持在初始化语句中注入
                          "Option=3;")
                      .arg(host).arg(port).arg(dbName).arg(user).arg(pwd);

    db.setDatabaseName(dsn);

    // 尝试打开连接
    if (!db.open()) {
        qDebug() << ">>> [Error] ODBC 连接失败，请确认驱动名称正确且 MySQL 服务已开启。";
        qDebug() << ">>> 错误详情:" << db.lastError().text();
        return false;
    }

    qDebug() << ">>> [Success] 已通过 QODBC 成功挂载 MySQL 数据库！";
    return true;
}

void DBHelper::disconnectDB() {
    if (db.isOpen()) {
        db.close();
        qDebug() << ">>> 数据库连接已断开";
    }
}

/**
 * @brief 用户注册
 */
bool DBHelper::registerUser(const QString& username, const QString& password, const QString& nickname) {
    if (!db.isOpen()) return false;

    QSqlQuery query(db);
    query.prepare("INSERT INTO user_info (username, password, nickname) "
                  "VALUES (:username, :password, :nickname)");
    query.bindValue(":username", username);
    query.bindValue(":password", password); // 提示：实际项目中建议存入 QCryptographicHash 后的散列值
    query.bindValue(":nickname", nickname);

    if (!query.exec()) {
        qDebug() << ">>> 注册失败:" << query.lastError().text();
        return false;
    }
    return true;
}

/**
 * @brief 用户登录
 */
// 在 DBHelper.cpp 中更新 login 函数
bool DBHelper::login(const QString& username, const QString& password, int& outUserId, QString& outNickname) {
    if (!db.isOpen()) return false;

    QSqlQuery query(db);
    // 增加对 nickname 的获取
    query.prepare("SELECT user_id, nickname, avatar_id, total_score FROM user_info "
                  "WHERE username = :username AND password = :password");
    query.bindValue(":username", username);
    query.bindValue(":password", password);

    if (query.exec() && query.next()) {
        outUserId = query.value("user_id").toInt();
        outNickname = query.value("nickname").toString();

        // 更新在线状态
        QSqlQuery updateQuery(db);
        updateQuery.prepare("UPDATE user_info SET is_online = 1 WHERE user_id = :userId");
        updateQuery.bindValue(":userId", outUserId);
        updateQuery.exec();

        return true;
    }
    return false;
}

/**
 * @brief 登出
 */
bool DBHelper::logout(int userId) {
    if (!db.isOpen()) return false;

    QSqlQuery query(db);
    query.prepare("UPDATE user_info SET is_online = 0 WHERE user_id = :userId");
    query.bindValue(":userId", userId);
    return query.exec();
}

/**
 * @brief 记录战绩
 * 核心逻辑：使用事务保证“记录写入”和“积分更新”要么同时成功，要么同时失败
 */
bool DBHelper::recordGameResult(int userId, int score, GameMode mode, bool isWin, int durationSec) {
    if (!db.isOpen()) return false;

    // 关键点 3：开启事务
    if (!db.transaction()) {
        qDebug() << ">>> 事务开启失败:" << db.lastError().text();
        return false;
    }

    QSqlQuery query(db);

    // 1. 插入战绩历史
    query.prepare("INSERT INTO game_history (user_id, score, game_mode, is_win, duration_sec) "
                  "VALUES (:userId, :score, :gameMode, :isWin, :durationSec)");
    query.bindValue(":userId", userId);
    query.bindValue(":score", score);
    // 关键点 4：类型对齐，将 C++17 枚举类强转为 int 存入数据库
    query.bindValue(":gameMode", static_cast<int>(mode));
    query.bindValue(":isWin", isWin ? 1 : 0);
    query.bindValue(":durationSec", durationSec);

    if (!query.exec()) {
        qDebug() << ">>> 写入战绩失败:" << query.lastError().text();
        db.rollback(); // 发生错误，撤销之前的操作
        return false;
    }

    // 2. 更新用户总积分
    query.prepare("UPDATE user_info SET total_score = total_score + :score WHERE user_id = :userId");
    query.bindValue(":score", score);
    query.bindValue(":userId", userId);

    if (!query.exec()) {
        qDebug() << ">>> 更新积分失败:" << query.lastError().text();
        db.rollback();
        return false;
    }

    // 3. 提交事务
    return db.commit();
}
