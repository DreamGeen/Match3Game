#include <QApplication>
#include <QStackedWidget>
#include <QSoundEffect> // 【新增】引入音效模块
#include "LoginWidget.h"
#include "LobbyWidget.h"
#include "MainWindow.h"
#include "DBHelper.h"

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);

    // 1. 连接数据库
    bool ok = DBHelper::getInstance().connectDB("127.0.0.1", 3306, "mengqing", "8176445027aA,.", "match3_game_db");
    if (!ok) return -1;

    // 2. 创建全局唯一的“舞台” (堆栈窗口容器)
    QStackedWidget *mainStage = new QStackedWidget();
    mainStage->setWindowTitle("Bocchi Clear! - 摇滚消消乐");
    mainStage->setWindowState(Qt::WindowMaximized);

    mainStage->setObjectName("mainStage");
    mainStage->setStyleSheet("#mainStage { border-image: url(:/res/backgrounds/stage_bg.png); }");
    mainStage->setAttribute(Qt::WA_StyledBackground, true);

    // =======================================================
    // 【新增】：创建并启动全局背景音乐
    QSoundEffect *bgm = new QSoundEffect(mainStage); // 将音乐挂载到 mainStage，同生共死
    bgm->setSource(QUrl::fromLocalFile(":/res/audio/bgm.wav")); // 替换为你实际的音乐文件名
    bgm->setLoopCount(QSoundEffect::Infinite); // 设置无限循环播放
    bgm->setVolume(0.6); // 背景音乐音量稍微调小点（0.0 ~ 1.0），别盖过消除的爽快音效
    bgm->play(); // 开播！
    // =======================================================

    // 3. 创建登录界面并放入舞台
    LoginWidget *loginWin = new LoginWidget();
    mainStage->addWidget(loginWin);


    // 4. 流程控制
    QObject::connect(loginWin, &LoginWidget::loginSuccess, [=](UserSession session) {
        // 创建大厅
        LobbyWidget *lobbyWin = new LobbyWidget(session);
        mainStage->addWidget(lobbyWin);
        mainStage->setCurrentWidget(lobbyWin);

        // 👇 【关键修改】：Lambda 中增加 AIDifficulty diff 参数
        // 监听大厅的模式选择信号
        QObject::connect(lobbyWin, &LobbyWidget::modeSelected, [=](UserSession session, GameMode mode,AIDifficulty diff) {
            // 创建游戏关卡
            MainWindow *gameWin = new MainWindow(session, mode,diff);
            mainStage->addWidget(gameWin);
            mainStage->setCurrentWidget(gameWin);

            // ===== 【新增】：监听返回大厅信号 =====
            QObject::connect(gameWin, &MainWindow::returnToLobbyRequested, [=]() {
                lobbyWin->refreshScore(); // 👇【加上这行】：切回大厅前，强制刷新一次积分！

                mainStage->setCurrentWidget(lobbyWin); // 切回大厅
                mainStage->removeWidget(gameWin);      // 从舞台堆栈中移除当前关卡
                gameWin->deleteLater();                // 释放掉游戏窗口的内存，防止越玩越卡
            });
        });
    });

    // 5. 拉开帷幕
    mainStage->show();

    return a.exec();
}
