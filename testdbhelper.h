#ifndef TESTDBHELPER_H
#define TESTDBHELPER_H

#include <QObject>
#include <QtTest/QtTest>
#include "DBHelper.h"

// 继承自 QObject，这是 Qt 测试框架的要求
class TestDBHelper : public QObject {
    Q_OBJECT

public:
    explicit TestDBHelper(QObject *parent = nullptr) : QObject(parent) {}

private slots: // 必须是 private slots，QTest 框架会自动识别并按顺序执行这些函数
    void initTestCase();    // 在所有测试开始前执行（用于初始化，如连接数据库）

    void testRegister();    // 测试：注册功能
    void testLogin();       // 测试：登录功能
    void testRecordGame();  // 测试：写入战绩功能
    void testLogout();      // 测试：登出功能

    void cleanupTestCase(); // 在所有测试结束后执行（用于清理环境，如删除测试产生的垃圾数据）

private:
    // 测试用的虚拟数据
    QString testUser = "auto_test_bot_001";
    QString testPwd = "bot_password";
    int currentUid = -1;
};

#endif // TESTDBHELPER_H
