#include "TestDBHelper.h"

void TestDBHelper::initTestCase() {
    qDebug() << ">>> 初始化测试环境：连接数据库...";
    DBHelper& db = DBHelper::getInstance();

    // QVERIFY 是 Qt 测试宏，如果括号内为 false，则当前测试直接标红失败
    // 请替换为你真实的数据库账号密码
    bool isConnected = db.connectDB("127.0.0.1", 3306, "mengqing", "8176445027aA,.", "match3_game_db");
    QVERIFY(isConnected);
}

void TestDBHelper::testRegister() {
    qDebug() << ">>> 测试: 注册新用户...";

    // 先尝试清理一下，防止上一次测试异常中断导致脏数据残留
    QSqlQuery cleanQuery;
    cleanQuery.exec(QString("DELETE FROM user_info WHERE username = '%1'").arg(testUser));

    bool isRegSuccess = DBHelper::getInstance().registerUser(testUser, testPwd, "自动化测试机器人");
    QVERIFY(isRegSuccess); // 断言：注册必须成功
}

void TestDBHelper::testLogin() {
    qDebug() << ">>> 测试: 验证登录...";
    QString nickname;
    // 👇【新增】：定义两个变量来接收新加的参数
    int avatarId;
    int totalScore;

    // 👇【修改】：把这 6 个参数完整地传给 login 函数
    bool isLoginSuccess = DBHelper::getInstance().login(testUser, testPwd, currentUid, nickname, avatarId, totalScore);

    QVERIFY(isLoginSuccess); // 断言：登录必须成功
    QVERIFY(currentUid > 0); // 断言：必须成功获取到了合法的 UID
    QCOMPARE(nickname, QString("自动化测试机器人")); // 断言：数据库查出的昵称必须匹配
}

void TestDBHelper::testRecordGame() {
    qDebug() << ">>> 测试: 写入游戏战绩与积分...";
    // 断言：必须确保先拿到了 UID 才能测这一步
    QVERIFY(currentUid > 0);

    // 模拟赢了一局，得 500 分，用时 90 秒
    bool isRecordSuccess = DBHelper::getInstance().recordGameResult(currentUid, 500, GameMode::Single, true, 90);
    QVERIFY(isRecordSuccess); // 断言：战绩写入必须成功
}

void TestDBHelper::testLogout() {
    qDebug() << ">>> 测试: 用户登出...";
    QVERIFY(currentUid > 0);

    bool isLogoutSuccess = DBHelper::getInstance().logout(currentUid);
    QVERIFY(isLogoutSuccess); // 断言：登出操作必须成功
}

void TestDBHelper::cleanupTestCase() {
    qDebug() << ">>> 清理测试环境：删除测试账号及关联战绩...";

    // 单元测试的核心原则：测试结束后，数据库必须恢复到测试前的干净状态！
    if (currentUid > 0) {
        QSqlQuery query;
        // 因为我们建表时用了 ON DELETE CASCADE，删除了 user_info，对应的 game_history 也会自动删除
        query.prepare("DELETE FROM user_info WHERE user_id = :uid");
        query.bindValue(":uid", currentUid);
        query.exec();
    }

    DBHelper::getInstance().disconnectDB();
    qDebug() << ">>> 所有测试完毕，断开连接。";
}
