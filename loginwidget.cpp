#include "LoginWidget.h"
#include <QHBoxLayout>
#include <QFrame>

LoginWidget::LoginWidget(QWidget *parent) : QWidget(parent) {
    // 允许顶层窗口绘制样式表背景
    this->setAttribute(Qt::WA_StyledBackground, true);

    initUI();
    setWindowTitle("Bocchi Clear! - 摇滚消消乐");


}

void LoginWidget::initUI() {
    // 1. 设置全屏背景
    this->setObjectName("loginMain");
    this->setStyleSheet("#loginMain { border-image: url(:/res/backgrounds/stage_bg.png); }");

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // 2. 放大中间的登录卡片，适应大屏幕排版
    QFrame *panel = new QFrame(this);
    panel->setObjectName("glassPanel");
    panel->setFixedSize(450, 480); // 从 380x360 放大到 450x480
    panel->setStyleSheet(R"(
        #glassPanel {
            background-color: rgba(20, 20, 30, 210);
            border-radius: 20px;
            border: 2px solid rgba(255, 105, 180, 100);
        }
    )");

    auto *panelLayout = new QVBoxLayout(panel);
    panelLayout->setSpacing(25); // 拉开组件间距
    panelLayout->setContentsMargins(50, 40, 50, 40);

    // 3. 标题文字也跟着放大
    QLabel *titleLabel = new QLabel("🎸 Bocchi Clear!", panel);
    titleLabel->setStyleSheet("font-size: 32px; font-weight: 900; color: #ffffff; background: transparent;");
    titleLabel->setAlignment(Qt::AlignCenter);

    QLabel *subTitleLabel = new QLabel("Livehouse 通行证验证", panel);
    subTitleLabel->setStyleSheet("font-size: 16px; color: #FFD700; background: transparent;");
    subTitleLabel->setAlignment(Qt::AlignCenter);

    // 4. 输入框加高，字号加大
    QString editStyle = R"(
        QLineEdit {
            background-color: rgba(255, 255, 255, 15);
            border: 2px solid rgba(255, 255, 255, 40);
            border-radius: 10px;
            padding: 10px 15px;
            color: white;
            font-size: 16px;
        }
        QLineEdit:focus {
            border: 2px solid #ff69b4;
            background-color: rgba(255, 255, 255, 30);
        }
    )";

    m_userEdit = new QLineEdit(panel);
    m_userEdit->setPlaceholderText("请输入乐手账号");
    m_userEdit->setFixedHeight(50); // 加高到 50
    m_userEdit->setStyleSheet(editStyle);

    m_pwdEdit = new QLineEdit(panel);
    m_pwdEdit->setPlaceholderText("请输入密码");
    m_pwdEdit->setEchoMode(QLineEdit::Password);
    m_pwdEdit->setFixedHeight(50); // 加高到 50
    m_pwdEdit->setStyleSheet(editStyle);

    // 5. 登录按钮更显眼
    m_loginBtn = new QPushButton("开启演奏", panel);
    m_loginBtn->setFixedHeight(55);
    m_loginBtn->setCursor(Qt::PointingHandCursor);
    m_loginBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #ff69b4;
            color: white;
            border-radius: 10px;
            font-size: 20px;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: transparent;
            color: #ff69b4;
            border: 2px solid #ff69b4;
        }
    )");

    m_regBtn = new QPushButton("没有门票？快速注册新乐手", panel);
    m_regBtn->setFlat(true);
    m_regBtn->setCursor(Qt::PointingHandCursor);
    m_regBtn->setStyleSheet("color: #aaaaaa; font-size: 14px; background: transparent;");

    // 6. 组装
    panelLayout->addWidget(titleLabel);
    panelLayout->addWidget(subTitleLabel);
    panelLayout->addSpacing(15);
    panelLayout->addWidget(m_userEdit);
    panelLayout->addWidget(m_pwdEdit);
    panelLayout->addSpacing(15);
    panelLayout->addWidget(m_loginBtn);
    panelLayout->addWidget(m_regBtn);
    panelLayout->addStretch();

    // 7. 核心：将面板在全屏窗口中正居中
    mainLayout->addWidget(panel, 0, Qt::AlignCenter);

    connect(m_loginBtn, &QPushButton::clicked, this, &LoginWidget::onLoginClicked);
    connect(m_regBtn, &QPushButton::clicked, this, &LoginWidget::onRegisterClicked);
}


// ... 下面的 onLoginClicked 和 onRegisterClicked 保持原样不动 ...
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

    if (DBHelper::getInstance().login(username, password, uid, nickname)) {
        UserSession session = { uid, username, nickname, 1, 0 };
        //QMessageBox::information(this, "登录成功", QString("欢迎来到摇滚世界，%1！").arg(nickname));
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

    if (DBHelper::getInstance().registerUser(username, password, "新玩家")) {
        QMessageBox::information(this, "注册成功", "账户已创建，请直接点击登录进入游戏。");
    } else {
        QMessageBox::critical(this, "注册失败", "用户名已存在或数据库异常。");
    }
}
