#include "LoginWidget.h"
#include <QHBoxLayout>

LoginWidget::LoginWidget(QWidget *parent) : QWidget(parent) {
    initUI();
    setWindowTitle("消消乐 - 账户验证");
    setFixedSize(380, 280);
}

void LoginWidget::initUI() {
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(50, 40, 50, 40);

    // 标题
    auto *titleLabel = new QLabel("Bocchi Clear! 登录", this);
    titleLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #333;");
    mainLayout->addWidget(titleLabel, 0, Qt::AlignCenter);

    // 输入框
    m_userEdit = new QLineEdit(this);
    m_userEdit->setPlaceholderText("请输入账号");
    m_userEdit->setFixedHeight(35);

    m_pwdEdit = new QLineEdit(this);
    m_pwdEdit->setPlaceholderText("请输入密码");
    m_pwdEdit->setEchoMode(QLineEdit::Password);
    m_pwdEdit->setFixedHeight(35);

    mainLayout->addWidget(m_userEdit);
    mainLayout->addWidget(m_pwdEdit);

    // 按钮组
    m_loginBtn = new QPushButton("进入游戏", this);
    m_loginBtn->setFixedHeight(40);
    m_loginBtn->setStyleSheet("QPushButton { background-color: #67C23A; color: white; border-radius: 4px; font-weight: bold; }"
                              "QPushButton:hover { background-color: #85ce61; }");

    m_regBtn = new QPushButton("快速注册新玩家", this);
    m_regBtn->setFlat(true);
    m_regBtn->setStyleSheet("color: #409EFF; font-size: 12px;");

    mainLayout->addWidget(m_loginBtn);
    mainLayout->addWidget(m_regBtn);

    connect(m_loginBtn, &QPushButton::clicked, this, &LoginWidget::onLoginClicked);
    connect(m_regBtn, &QPushButton::clicked, this, &LoginWidget::onRegisterClicked);
}

void LoginWidget::onLoginClicked() {
    QString username = m_userEdit->text().trimmed();
    QString password = m_pwdEdit->text();

    if (username.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, "校验", "账号或密码不能为空！");
        return;
    }

    // 临时变量用于接收数据库查询结果
    int uid, avatarId, totalScore;
    QString nickname;

    // 这里假设你已经更新了 DBHelper::login 以匹配 user_info 表字段
    if (DBHelper::getInstance().login(username, password, uid, nickname)) {
        // 注意：在真实的 DBHelper 实现中，你应该从数据库 SELECT 出 avatar_id 和 total_score
        // 这里模拟获取到的数据
        UserSession session = { uid, username, nickname, 1, 0 };

        QMessageBox::information(this, "登录成功", QString("欢迎来到摇滚世界，%1！").arg(nickname));
        emit loginSuccess(session);
    } else {
        QMessageBox::critical(this, "登录失败", "账号或密码不正确，请重试。");
    }
}

void LoginWidget::onRegisterClicked() {
    QString username = m_userEdit->text().trimmed();
    QString password = m_pwdEdit->text();

    if (username.length() < 3 || password.length() < 6) {
        QMessageBox::warning(this, "注册规范", "账号至少3位，密码至少6位！");
        return;
    }

    // 调用 registerUser，这里 nickname 默认为 "新玩家"
    if (DBHelper::getInstance().registerUser(username, password, "新玩家")) {
        QMessageBox::information(this, "注册成功", "账户已创建，请直接点击登录进入游戏。");
    } else {
        QMessageBox::critical(this, "注册失败", "用户名已存在或数据库异常。");
    }
}
