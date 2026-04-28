#include "LoginWidget.h"
#include <QHBoxLayout>

LoginWidget::LoginWidget(QWidget *parent) : QWidget(parent) {
    this->setAttribute(Qt::WA_StyledBackground, true);

    initUI();
    setWindowTitle("Bocchi Clear! - 摇滚消消乐");
}

void LoginWidget::initUI() {
    this->setObjectName("loginMain");
    this->setStyleSheet("#loginMain { border-image: url(:/res/backgrounds/stage_bg.png); }");

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // 1. 半透明面板 (默认是登录模式的高度 480)
    m_panel = new QFrame(this);
    m_panel->setObjectName("glassPanel");
    m_panel->setFixedSize(450, 480);
    m_panel->setStyleSheet(R"(
        #glassPanel {
            background-color: rgba(20, 20, 30, 210);
            border-radius: 20px;
            border: 2px solid rgba(255, 105, 180, 100);
        }
    )");

    auto *panelLayout = new QVBoxLayout(m_panel);
    panelLayout->setSpacing(20);
    panelLayout->setContentsMargins(50, 40, 50, 40);

    // 2. 标题区
    QLabel *titleLabel = new QLabel("🎸 Bocchi Clear!", m_panel);
    titleLabel->setStyleSheet("font-size: 32px; font-weight: 900; color: #ffffff; background: transparent;");
    titleLabel->setAlignment(Qt::AlignCenter);

    m_subTitleLabel = new QLabel("Livehouse 通行证验证", m_panel);
    m_subTitleLabel->setStyleSheet("font-size: 16px; color: #FFD700; background: transparent;");
    m_subTitleLabel->setAlignment(Qt::AlignCenter);

    // 3. 通用输入框样式
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

    // --- 初始化 4 个输入框 ---
    m_userEdit = new QLineEdit(m_panel);
    m_userEdit->setPlaceholderText("请输入乐手账号 (至少3位)");
    m_userEdit->setFixedHeight(50);
    m_userEdit->setStyleSheet(editStyle);

    m_pwdEdit = new QLineEdit(m_panel);
    m_pwdEdit->setPlaceholderText("请输入密码 (至少6位)");
    m_pwdEdit->setEchoMode(QLineEdit::Password);
    m_pwdEdit->setFixedHeight(50);
    m_pwdEdit->setStyleSheet(editStyle);

    // 注册专用：确认密码
    m_pwdConfirmEdit = new QLineEdit(m_panel);
    m_pwdConfirmEdit->setPlaceholderText("请再次输入密码");
    m_pwdConfirmEdit->setEchoMode(QLineEdit::Password);
    m_pwdConfirmEdit->setFixedHeight(50);
    m_pwdConfirmEdit->setStyleSheet(editStyle);
    m_pwdConfirmEdit->hide(); // 默认隐藏

    // 注册专用：游戏昵称
    m_nicknameEdit = new QLineEdit(m_panel);
    m_nicknameEdit->setPlaceholderText("请输入乐手昵称 (可选)");
    m_nicknameEdit->setFixedHeight(50);
    m_nicknameEdit->setStyleSheet(editStyle);
    m_nicknameEdit->hide(); // 默认隐藏

    // 4. 按钮区
    m_mainBtn = new QPushButton("开启演奏", m_panel);
    m_mainBtn->setFixedHeight(55);
    m_mainBtn->setCursor(Qt::PointingHandCursor);
    m_mainBtn->setStyleSheet(R"(
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

    m_switchBtn = new QPushButton("没有门票？快速注册新乐手", m_panel);
    m_switchBtn->setFlat(true);
    m_switchBtn->setCursor(Qt::PointingHandCursor);
    m_switchBtn->setStyleSheet("color: #aaaaaa; font-size: 14px; background: transparent; border: none;");

    // 5. 组装
    panelLayout->addWidget(titleLabel);
    panelLayout->addWidget(m_subTitleLabel);
    panelLayout->addSpacing(5);
    panelLayout->addWidget(m_userEdit);
    panelLayout->addWidget(m_pwdEdit);
    panelLayout->addWidget(m_pwdConfirmEdit); // 添加进去，由于 hide()，并不会占空间
    panelLayout->addWidget(m_nicknameEdit);   // 添加进去，同上
    panelLayout->addSpacing(10);
    panelLayout->addWidget(m_mainBtn);
    panelLayout->addWidget(m_switchBtn);
    panelLayout->addStretch();

    mainLayout->addWidget(m_panel, 0, Qt::AlignCenter);

    // 6. 信号绑定
    connect(m_mainBtn, &QPushButton::clicked, this, &LoginWidget::onMainBtnClicked);
    connect(m_switchBtn, &QPushButton::clicked, this, &LoginWidget::toggleMode);
}

// ======================== 核心交互逻辑 ========================

void LoginWidget::toggleMode() {
    m_isLoginMode = !m_isLoginMode; // 翻转状态

    // 清空所有输入框残余内容
    m_userEdit->clear();
    m_pwdEdit->clear();
    m_pwdConfirmEdit->clear();
    m_nicknameEdit->clear();

    if (m_isLoginMode) {
        // --- 切换回登录模式 ---
        m_panel->setFixedSize(450, 480); // 缩小面板
        m_subTitleLabel->setText("Livehouse 通行证验证");

        m_pwdConfirmEdit->hide();
        m_nicknameEdit->hide();

        m_mainBtn->setText("开启演奏");
        m_switchBtn->setText("没有门票？快速注册新乐手");
    } else {
        // --- 切换到注册模式 ---
        m_panel->setFixedSize(450, 640); // 撑大面板，留出输入框的空间
        m_subTitleLabel->setText("新乐手档案登记");

        m_pwdConfirmEdit->show();
        m_nicknameEdit->show();

        m_mainBtn->setText("提交注册");
        m_switchBtn->setText("已有门票？返回检票口");
    }
}

void LoginWidget::onMainBtnClicked() {
    // 路由分发：根据当前状态，决定主按钮是执行登录还是注册
    if (m_isLoginMode) {
        performLogin();
    } else {
        performRegister();
    }
}

void LoginWidget::performLogin() {
    QString username = m_userEdit->text().trimmed();
    QString password = m_pwdEdit->text();

    if (username.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, "校验", "账号或密码不能为空！");
        return;
    }

    int uid, avatarId, totalScore;
    QString nickname;

    // 👇【关键修复】：调用更新后的登录函数
    if (DBHelper::getInstance().login(username, password, uid, nickname, avatarId, totalScore)) {
        // 👇【关键修复】：使用数据库查出的真实 avatarId 和 totalScore，不再硬编码为 1 和 0
        UserSession session = { uid, username, nickname, avatarId, totalScore };
        emit loginSuccess(session);
    } else {
        QMessageBox::critical(this, "登录失败", "账号或密码不正确，请重试。");
    }
}

void LoginWidget::performRegister() {
    QString username = m_userEdit->text().trimmed();
    QString password = m_pwdEdit->text();
    QString confirmPwd = m_pwdConfirmEdit->text();
    QString nickname = m_nicknameEdit->text().trimmed();

    // 1. 基础校验
    if (username.length() < 3 || password.length() < 6) {
        QMessageBox::warning(this, "规范错误", "账号至少3位，密码至少6位！");
        return;
    }

    // 2. 确认密码校验
    if (password != confirmPwd) {
        QMessageBox::warning(this, "规范错误", "两次输入的密码不一致！");
        return;
    }

    // 3. 昵称默认值处理 (如果玩家没填，就利用上你数据库里设置的默认昵称概念)
    if (nickname.isEmpty()) {
        nickname = "新玩家";
    }

    // 4. 调用 DBHelper 进行注册
    if (DBHelper::getInstance().registerUser(username, password, nickname)) {
        QMessageBox::information(this, "注册成功", "乐手档案已创建，请凭门票入场！");

        // 【顶级体验】：注册成功后，自动切回登录页面，并帮玩家填好刚才注册的账号
        toggleMode();
        m_userEdit->setText(username);
        m_pwdEdit->setFocus(); // 光标自动跳到密码框
    } else {
        QMessageBox::critical(this, "注册失败", "用户名可能已被占用，或数据库通讯异常。");
    }
}
