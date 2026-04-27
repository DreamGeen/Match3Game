#include <QApplication>
#include <QStackedWidget> // 【新增】引入堆栈窗口模块
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
    mainStage->setWindowState(Qt::WindowMaximized); // 让大舞台全局最大化

    // 把背景图贴在最底层的舞台上，这样无论怎么切换，背景永远是静止且连贯的！
    mainStage->setObjectName("mainStage");
    mainStage->setStyleSheet("#mainStage { border-image: url(:/res/backgrounds/stage_bg.png); }");
    mainStage->setAttribute(Qt::WA_StyledBackground, true);

    // 3. 创建登录界面并放入舞台
    LoginWidget *loginWin = new LoginWidget();
    mainStage->addWidget(loginWin);

    // 4. 流程控制：登录成功 -> 无缝切换到大厅
    QObject::connect(loginWin, &LoginWidget::loginSuccess, [=](UserSession session) {
        LobbyWidget *lobbyWin = new LobbyWidget(session);
        mainStage->addWidget(lobbyWin);

        // 【核心魔法】：瞬间将当前显示的画面切换为大厅，没有任何系统窗口的闪烁！
        mainStage->setCurrentWidget(lobbyWin);

        // 流程控制：大厅选择模式 -> 无缝切换到游戏主场景
        QObject::connect(lobbyWin, &LobbyWidget::modeSelected, [=](UserSession session, GameMode mode) {
            MainWindow *gameWin = new MainWindow(session, mode);
            mainStage->addWidget(gameWin);

            // 再次瞬间无缝切换！
            mainStage->setCurrentWidget(gameWin);
        });
    });

    // 5. 拉开帷幕，显示大舞台
    mainStage->show();

    return a.exec();
}
