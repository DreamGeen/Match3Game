#ifndef UIHELPER_H
#define UIHELPER_H

#include <QString>
#include <QWidget>
#include <QFrame>
#include <functional>
#include <QEvent>
#include <QResizeEvent>

// 这是一个事件过滤器，用于让遮罩自动跟随父窗口改变大小（黑科技）
class OverlayResizeFilter : public QObject {
public:
    OverlayResizeFilter(QFrame* overlay, QObject* parent = nullptr)
        : QObject(parent), m_overlay(overlay) {}

protected:
    bool eventFilter(QObject* obj, QEvent* event) override {
        if (event->type() == QEvent::Resize && m_overlay) {
            QResizeEvent* re = static_cast<QResizeEvent*>(event);
            m_overlay->setGeometry(0, 0, re->size().width(), re->size().height());
        }
        return QObject::eventFilter(obj, event);
    }
private:
    QFrame* m_overlay;
};


// 霓虹弹窗工具类
class UIHelper {
public:
    // 👇 新增：定义弹窗的三种核心状态
    enum class PopupType {
        Info,     // 提示类型：青色主题，1个“明白”按钮
        Error,    // 错误类型：红色主题，1个“明白”按钮
        Question  // 询问类型：红色主题，2个操作按钮
    };

    // 👇 新版核心函数：使用 PopupType 枚举来控制状态
    static void showCustomPopup(QWidget* parent, const QString& title, const QString& msg, PopupType type, std::function<void()> onConfirm = nullptr);

    // 👇 兼容旧版的桥梁函数：保留 bool isQuestion 参数，防止旧代码报错
    static void showCustomPopup(QWidget* parent, const QString& title, const QString& msg, bool isQuestion, std::function<void()> onConfirm = nullptr);
};




#endif // UIHELPER_H
