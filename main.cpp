#include <QApplication>
#include "LoginWidget.h"
#include "MainWindow.h"
#include "DBHelper.h"

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);

    // 1. 连接数据库 (根据你的 MySQL 环境修改)
    bool ok = DBHelper::getInstance().connectDB("127.0.0.1", 3306, "mengqing", "8176445027aA,.", "match3_game_db");
    if (!ok) return -1;

    // 2. 显示登录窗口
    LoginWidget *loginWin = new LoginWidget();
    loginWin->show();

    // 3. 处理登录成功后的“切场”
    // 使用指针是为了在切换后手动管理窗口生命周期
    QObject::connect(loginWin, &LoginWidget::loginSuccess, [=](UserSession session) {
        MainWindow *mainWin = new MainWindow(session);
        mainWin->show();

        loginWin->hide();
        loginWin->deleteLater();
    });

    return a.exec();
}
