#ifndef GLOBAL_H
#define GLOBAL_H

#include <QString>

// 模式枚举：全项目仅在此处定义一次
enum class GameMode {
    Single = 0,
    AI = 1,
    Online = 2
};

// 登录 Session 信息：全项目共享
struct UserSession {
    int uid;
    QString username;
    QString nickname;
    int avatarId;
    int totalScore;
};

#endif
