#ifndef LOGINWIDGET_H
#define LOGINWIDGET_H

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QMessageBox>
#include "DBHelper.h"



class LoginWidget : public QWidget {
    Q_OBJECT
public:
    explicit LoginWidget(QWidget *parent = nullptr);

signals:
    // 登录成功信号，携带完整的用户 Session
    void loginSuccess(UserSession session);

private slots:
    void onLoginClicked();
    void onRegisterClicked();

private:
    QLineEdit *m_userEdit;
    QLineEdit *m_pwdEdit;
    QPushButton *m_loginBtn;
    QPushButton *m_regBtn;

    void initUI();
};

#endif
