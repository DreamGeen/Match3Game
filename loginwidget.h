#ifndef LOGINWIDGET_H
#define LOGINWIDGET_H

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QFrame>
#include "DBHelper.h"

class LoginWidget : public QWidget {
    Q_OBJECT
public:
    explicit LoginWidget(QWidget *parent = nullptr);

signals:
    // 登录成功信号，携带完整的用户 Session
    void loginSuccess(UserSession session);

private slots:
    void onMainBtnClicked(); // 处理点击主按钮 (登录/注册 状态复用)
    void toggleMode();       // 处理登录与注册模式的切换

private:
    QFrame *m_panel;         // 半透明玻璃面板
    QLabel *m_subTitleLabel; // 副标题

    QLineEdit *m_userEdit;       // 账号输入框
    QLineEdit *m_pwdEdit;        // 密码输入框
    QLineEdit *m_pwdConfirmEdit; // 确认密码输入框 (注册专用)
    QLineEdit *m_nicknameEdit;   // 昵称输入框 (注册专用)

    QPushButton *m_mainBtn;   // 核心大按钮
    QPushButton *m_switchBtn; // 底部切换模式小按钮

    bool m_isLoginMode = true; // 状态记录：当前是否在登录模式

    void initUI();
    void performLogin();
    void performRegister();
};

#endif // LOGINWIDGET_H
