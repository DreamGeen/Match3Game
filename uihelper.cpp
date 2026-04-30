#include "uihelper.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QGraphicsDropShadowEffect> // 👈 新增：用于制造弹窗的霓虹发光效果

#include "uihelper.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QGraphicsDropShadowEffect>

// =========================================================================
// 🌟 1. 核心干活的新版本：彻底解耦了颜色和按钮数量
// =========================================================================
void UIHelper::showCustomPopup(QWidget* parent, const QString& title, const QString& msg, PopupType type, std::function<void()> onConfirm) {
    if (!parent) return;

    // 1. 全屏遮罩
    QFrame *overlay = new QFrame(parent);
    overlay->setObjectName("popupOverlay");
    overlay->setGeometry(0, 0, parent->width(), parent->height());
    overlay->setStyleSheet("QFrame#popupOverlay { background-color: rgba(0, 0, 0, 160); }");

    parent->installEventFilter(new OverlayResizeFilter(overlay, overlay));

    QVBoxLayout *overlayLayout = new QVBoxLayout(overlay);

    // 2. 核心面板
    QFrame *panel = new QFrame(overlay);
    panel->setObjectName("popupPanel");
    panel->setFixedSize(450, 260);

    // 👇 颜色解耦：Error 和 Question 都使用红色警告主题，Info 使用青色
    bool isRedTheme = (type == PopupType::Error || type == PopupType::Question);
    QString themeColor = isRedTheme ? "#FF4500" : "#00FFFF";
    QString themeColorRgba = isRedTheme ? "rgba(255, 69, 0, 180)" : "rgba(0, 255, 255, 180)";

    panel->setStyleSheet(QString(R"(
        QFrame#popupPanel {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(40, 30, 45, 230), stop:1 rgba(15, 10, 20, 240));
            border-radius: 16px;
            border: 2px solid %1;
        }
        QLabel { border: none; background: transparent; font-family: 'Microsoft YaHei'; }
    )").arg(themeColorRgba));

    // 🌟 光学重塑
    QGraphicsDropShadowEffect *glow = new QGraphicsDropShadowEffect(panel);
    glow->setOffset(0, 0);
    glow->setBlurRadius(60);
    glow->setColor(QColor(isRedTheme ? QColor(255, 69, 0, 150) : QColor(0, 255, 255, 150)));
    panel->setGraphicsEffect(glow);

    QVBoxLayout *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(35, 35, 35, 35);
    layout->setSpacing(20);

    // 3. 标题与文字
    QLabel *titleLabel = new QLabel(title, panel);
    titleLabel->setStyleSheet(QString("font-size: 26px; font-weight: 900; color: %1; letter-spacing: 3px;").arg(themeColor));
    titleLabel->setAlignment(Qt::AlignCenter);

    QLabel *msgLabel = new QLabel(msg, panel);
    msgLabel->setStyleSheet("font-size: 16px; color: #E0E0E0; line-height: 1.5; font-weight: bold;");
    msgLabel->setAlignment(Qt::AlignCenter);

    // 4. 按钮区
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(25);

    // 👇 按钮解耦：明确是 Question 类型才生成 2 个按钮
    if (type == PopupType::Question) {
        QPushButton *yesBtn = new QPushButton("🎸 确认操作", panel);
        yesBtn->setCursor(Qt::PointingHandCursor);
        yesBtn->setStyleSheet(R"(
            QPushButton {
                background-color: rgba(255, 69, 0, 20);
                color: #ff4500;
                border-radius: 8px;
                border: 2px solid #ff4500;
                font-size: 16px; font-weight: bold; padding: 10px 20px;
            }
            QPushButton:hover { background-color: #ff4500; color: white; }
            QPushButton:pressed { background-color: #cc3700; border: 2px solid #cc3700; }
        )");

        QPushButton *noBtn = new QPushButton("🔙 取消", panel);
        noBtn->setCursor(Qt::PointingHandCursor);
        noBtn->setStyleSheet(R"(
            QPushButton {
                background-color: rgba(255, 255, 255, 10);
                color: #cccccc;
                border-radius: 8px;
                border: 2px solid rgba(255, 255, 255, 50);
                font-size: 16px; font-weight: bold; padding: 10px 20px;
            }
            QPushButton:hover { background-color: rgba(255, 255, 255, 40); color: white; border: 2px solid white; }
            QPushButton:pressed { background-color: rgba(255, 255, 255, 20); }
        )");

        btnLayout->addWidget(yesBtn);
        btnLayout->addWidget(noBtn);

        QObject::connect(yesBtn, &QPushButton::clicked, [overlay, onConfirm]() {
            overlay->deleteLater();
            if (onConfirm) onConfirm();
        });
        QObject::connect(noBtn, &QPushButton::clicked, overlay, &QObject::deleteLater);

    } else {
        // 👇 Info 和 Error 都只生成 1 个确认按钮
        QPushButton *okBtn = new QPushButton("✅ 明白", panel);
        okBtn->setCursor(Qt::PointingHandCursor);

        if (type == PopupType::Error) {
            // Error 状态：单按钮也变成红色警告样式
            okBtn->setStyleSheet(R"(
                QPushButton {
                    background-color: rgba(255, 69, 0, 20);
                    color: #ff4500;
                    border-radius: 8px; border: 2px solid #ff4500;
                    font-size: 16px; font-weight: bold; padding: 10px 20px;
                }
                QPushButton:hover { background-color: #ff4500; color: white; }
                QPushButton:pressed { background-color: #cc3700; border: 2px solid #cc3700; }
            )");
        } else {
            // Info 状态：原有的青色提示样式
            okBtn->setStyleSheet(R"(
                QPushButton {
                    background-color: rgba(0, 255, 255, 20);
                    color: #00FFFF;
                    border-radius: 8px; border: 2px solid #00FFFF;
                    font-size: 16px; font-weight: bold; padding: 10px 20px;
                }
                QPushButton:hover { background-color: #00FFFF; color: black; }
                QPushButton:pressed { background-color: #00cccc; border: 2px solid #00cccc; }
            )");
        }

        btnLayout->addStretch();
        btnLayout->addWidget(okBtn);
        btnLayout->addStretch();

        QObject::connect(okBtn, &QPushButton::clicked, [overlay, onConfirm]() {
            overlay->deleteLater();
            if (onConfirm) onConfirm();
        });
    }

    layout->addStretch();
    layout->addWidget(titleLabel);
    layout->addWidget(msgLabel);
    layout->addStretch();
    layout->addLayout(btnLayout);

    overlayLayout->addWidget(panel, 0, Qt::AlignCenter);
    overlay->show();
    overlay->raise();
}

// =========================================================================
// 🤝 2. 给旧代码用的兼容版本（适配器模式），防止旧代码报错
// =========================================================================
void UIHelper::showCustomPopup(QWidget* parent, const QString& title, const QString& msg, bool isQuestion, std::function<void()> onConfirm) {
    // 悄悄把以前的 true 翻译成 Question，false 翻译成 Info，然后丢给新函数处理！
    PopupType mappedType = isQuestion ? PopupType::Question : PopupType::Info;
    showCustomPopup(parent, title, msg, mappedType, onConfirm);
}
