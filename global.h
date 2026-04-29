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

// 在 Global.h 中添加
enum class AIDifficulty {
    Easy = 0,    // 呆萌：思考 2.5秒
    Normal = 1,  // 普通：思考 1.8秒
    Hard = 2     // 疯狂：思考 0.6秒
};

#endif
