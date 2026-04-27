#ifndef BLOCKWIDGET_H
#define BLOCKWIDGET_H

#include <QWidget>
#include <QPainter>
#include <QPropertyAnimation>
#include "GameElement.h" // 引用你提供的 GameElement.h

class BlockWidget : public QWidget {
    Q_OBJECT

    // --- 动画系统核心属性 ---
    // 1. 位置属性：QPropertyAnimation 将通过 move() 实现方块滑动
    Q_PROPERTY(QPoint pos READ pos WRITE move)
    // 2. 透明度属性：用于实现消除时的淡出效果
    Q_PROPERTY(qreal opacity READ opacity WRITE setOpacity)

    // --- 新增：暴露给动画系统的缩放属性 ---
    Q_PROPERTY(qreal scale READ scale WRITE setScale)

public:
    // 构造函数：需要初始逻辑坐标 (r, c) 和方块数据
    explicit BlockWidget(int r, int c, Tile tileData, QWidget *parent = nullptr);

    // 数据操作接口
    void setTileData(Tile newData);
    Tile getTileData() const { return m_data; }

    // 属性访问器
    qreal opacity() const { return m_opacity; }
    void setOpacity(qreal v) { m_opacity = v; update(); }

    // 静态工具：逻辑坐标转像素坐标
    static QPoint logicalToPixel(int r, int c);

    // 在 public 下增加：
    void setSelected(bool selected);

    // 缩放属性的访问器
    qreal scale() const { return m_scale; }
    void setScale(qreal s) { m_scale = s; update(); } // 改变缩放值时触发重绘

protected:
    // 绘制方块及其特殊效果
    void paintEvent(QPaintEvent *event) override;

private:
    Tile m_data;           // 当前方块的颜色和类型
    qreal m_opacity = 1.0; // 当前透明度
    qreal m_scale = 1.0; // 默认缩放比例为 1.0

    // 缓存贴图，避免在 paintEvent 中反复加载
    QPixmap m_mainPixmap;     // 底层色块
    QPixmap m_overlayPixmap;  // 特效叠加层

    // 在 private 下增加：
    bool m_isSelected = false;

    void loadResources();     // 根据 Tile 数据加载对应的图片
};

#endif
