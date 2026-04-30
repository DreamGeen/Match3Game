#include "uihelper.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QGraphicsDropShadowEffect> // 👈 新增：用于制造弹窗的霓虹发光效果

void UIHelper::showCustomPopup(QWidget* parent, const QString& title, const QString& msg, bool isQuestion, std::function<void()> onConfirm) {
    if (!parent) return;

    // 1. 全屏遮罩：稍微调亮一点，不要把整个屏幕压得死黑 (210 -> 160)
    QFrame *overlay = new QFrame(parent);
    overlay->setObjectName("popupOverlay");
    overlay->setGeometry(0, 0, parent->width(), parent->height());
    overlay->setStyleSheet("QFrame#popupOverlay { background-color: rgba(0, 0, 0, 160); }");

    parent->installEventFilter(new OverlayResizeFilter(overlay, overlay));

    QVBoxLayout *overlayLayout = new QVBoxLayout(overlay);

    // 2. 核心面板：采用“高级紫灰渐变” + “2px 实心霓虹边框”
    QFrame *panel = new QFrame(overlay);
    panel->setObjectName("popupPanel"); // 加上 ID 选择器防止样式污染子控件

    // 👇 修改这里：把原本的 450, 260 放大！
    // 推荐比例：宽 540，高 300 (或者 560x320，你可以根据你的屏幕感觉微调)
    panel->setFixedSize(540, 300);


    // 根据是警告还是提示，决定主题色
    QString themeColor = isQuestion ? "#FF4500" : "#00FFFF";
    QString themeColorRgba = isQuestion ? "rgba(255, 69, 0, 180)" : "rgba(0, 255, 255, 180)";

    panel->setStyleSheet(QString(R"(
        QFrame#popupPanel {
            /* 顶部稍微泛紫亮一点，底部暗下去，质感瞬间就出来了 */
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(40, 30, 45, 230), stop:1 rgba(15, 10, 20, 240));
            border-radius: 16px;
            border: 2px solid %1; /* 加粗到 2px，这样发光才有灯管的实体 */
        }
        QLabel { border: none; background: transparent; font-family: 'Microsoft YaHei'; }
    )").arg(themeColorRgba));

    // 🌟 光学重塑：加大扩散半径，降低不透明度，去掉脏乱感
    QGraphicsDropShadowEffect *glow = new QGraphicsDropShadowEffect(panel);
    glow->setOffset(0, 0);
    glow->setBlurRadius(60); // 扩散半径加大 (30 -> 60)，让光晕更柔和
    glow->setColor(QColor(isQuestion ? QColor(255, 69, 0, 150) : QColor(0, 255, 255, 150))); // 带一点透明度的光
    panel->setGraphicsEffect(glow);

    QVBoxLayout *layout = new QVBoxLayout(panel);
    // 👇 把内边距稍微加大一点，从 35 改成 40，让它离发光边缘远一点，更有留白感
    layout->setContentsMargins(40, 40, 40, 40);
    // 👇 控件之间的间距也可以稍微拉开一点，从 20 改成 25
    layout->setSpacing(25);


    // 3. 标题与文字：加大字间距
    QLabel *titleLabel = new QLabel(title, panel);
    titleLabel->setStyleSheet(QString("font-size: 26px; font-weight: 900; color: %1; letter-spacing: 3px;").arg(themeColor));
    titleLabel->setAlignment(Qt::AlignCenter);

    QLabel *msgLabel = new QLabel(msg, panel);
    msgLabel->setStyleSheet("font-size: 16px; color: #E0E0E0; line-height: 1.5; font-weight: bold;");
    msgLabel->setAlignment(Qt::AlignCenter);

    // 4. 按钮区：改为“空心发光灯管”风格
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(25);

    if (isQuestion) {
        QPushButton *yesBtn = new QPushButton("🎸 确认操作", panel);
        yesBtn->setCursor(Qt::PointingHandCursor);
        yesBtn->setStyleSheet(R"(
            QPushButton {
                background-color: rgba(255, 69, 0, 20); /* 极低的透明底 */
                color: #ff4500;
                border-radius: 8px;
                border: 2px solid #ff4500; /* 纯色灯管边框 */
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
        QPushButton *okBtn = new QPushButton("✅ 明白", panel);
        okBtn->setCursor(Qt::PointingHandCursor);
        okBtn->setStyleSheet(R"(
            QPushButton {
                background-color: rgba(0, 255, 255, 20);
                color: #00FFFF;
                border-radius: 8px;
                border: 2px solid #00FFFF;
                font-size: 16px; font-weight: bold; padding: 10px 20px;
            }
            QPushButton:hover { background-color: #00FFFF; color: black; }
            QPushButton:pressed { background-color: #00cccc; border: 2px solid #00cccc; }
        )");

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
