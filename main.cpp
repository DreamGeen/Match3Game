#include <QApplication>
#include "LoginWidget.h"
#include "LobbyWidget.h"
#include "MainWindow.h"
#include "DBHelper.h"

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);

    if (!DBHelper::getInstance().connectDB("127.0.0.1", 3306, "mengqing", "8176445027aA,.", "match3_game_db")) {
        return -1;
    }

    LoginWidget *loginWin = new LoginWidget();
    loginWin->show();

    // 流程：登录成功 -> 进入大厅
    QObject::connect(loginWin, &LoginWidget::loginSuccess, [=](UserSession session) {
        LobbyWidget *lobbyWin = new LobbyWidget(session);
        lobbyWin->show();

        // 流程：大厅选择模式 -> 进入游戏主窗口
        QObject::connect(lobbyWin, &LobbyWidget::modeSelected, [=](UserSession session, GameMode mode) {
            MainWindow *mainWin = new MainWindow(session,mode);
            // 注意：你需要在 MainWindow 的构造或初始化中根据 mode 加载不同的逻辑
            mainWin->show();

            lobbyWin->hide();
            lobbyWin->deleteLater();
        });

        loginWin->hide();
        loginWin->deleteLater();
    });

    return a.exec();
}
