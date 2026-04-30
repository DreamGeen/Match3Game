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
    // 静态方法：随时随地，直接调用！
    static void showCustomPopup(QWidget* parent, const QString& title, const QString& msg, bool isQuestion, std::function<void()> onConfirm = nullptr);
};

#endif // UIHELPER_H
