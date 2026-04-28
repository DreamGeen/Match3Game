#include "BlockWidget.h"
#include <QDebug>

BlockWidget::BlockWidget(int r, int c, Tile tileData, QWidget *parent)
    : QWidget(parent), m_data(tileData)
{
    // 1. 初始化 UI 大小 (使用 GameConfig 中的标准)
    setFixedSize(GameConfig::TILE_SIZE, GameConfig::TILE_SIZE);

    // 2. 初始摆放位置 (逻辑转像素)
    move(logicalToPixel(r, c));

    // 3. 加载资源并开启透明支持
    loadResources();
    setAttribute(Qt::WA_TranslucentBackground);
    show();
}

void BlockWidget::setTileData(Tile newData) {
    m_data = newData;
    loadResources();
    update(); // 触发重绘
}

QPoint BlockWidget::logicalToPixel(int r, int c) {
    // 核心转换公式：X 对应 Column(c), Y 对应 Row(r)
    return QPoint(c * GameConfig::TILE_SIZE, r * GameConfig::TILE_SIZE);
}

void BlockWidget::loadResources() {
    if (m_data.isEmpty()) {
        m_mainPixmap = QPixmap();
        m_overlayPixmap = QPixmap();
        return;
    }

    // 👇【核心修改】：所有特殊方块直接无视底色，加载完整大图！
    if (m_data.special != SpecialType::None) {
        QString imgPath;
        switch (m_data.special) {
        case SpecialType::MagicBird:      imgPath = ":/res/items/magic_bird.png"; break;
        case SpecialType::LineHorizontal: imgPath = ":/res/items/eff_line_h.png"; break;
        case SpecialType::LineVertical:   imgPath = ":/res/items/eff_v.png"; break;
        case SpecialType::Bomb:           imgPath = ":/res/items/eff_bomb.png"; break;
        default: break;
        }

        if (!m_mainPixmap.load(imgPath)) {
            qDebug() << "!!! [错误] 特殊特效图加载失败:" << imgPath;
        }
        m_overlayPixmap = QPixmap(); // 清空覆盖层
        return; // 💥 关键：直接 return，彻底跳过底色加载！
    }

    // 如果是普通角色方块，才正常加载底色
    QString charPath = QString(":/res/items/color_%1.png").arg(m_data.color);
    if (!m_mainPixmap.load(charPath)) {
        qDebug() << "!!! [错误] 角色底图加载失败:" << charPath;
    }
    m_overlayPixmap = QPixmap();
}

void BlockWidget::paintEvent(QPaintEvent *) {
    QPainter painter(this);

    // 开启平滑缩放，防止动画移动时出现像素锯齿
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.setOpacity(m_opacity);


    // --- 【关键点】：执行中心缩放变换 ---
    if (m_scale != 1.0) {
        // 1. 将原点移到方块中心
        painter.translate(rect().center());
        // 2. 执行缩放
        painter.scale(m_scale, m_scale);
        // 3. 将原点移回左上角进行后续绘制
        painter.translate(-rect().center());
    }

    // 第一层：绘制基础色块
    if (!m_mainPixmap.isNull()) {
        painter.drawPixmap(rect(), m_mainPixmap);
    }

    // 第二层：绘制特殊效果 (如激光条纹或炸弹火花)
    if (!m_overlayPixmap.isNull()) {
        painter.drawPixmap(rect(), m_overlayPixmap);
    }

    // 3. 【核心】：绘制选中特效
    if (m_isSelected) {
        painter.setRenderHint(QPainter::Antialiasing); // 开启抗锯齿

        // 设置画笔：波奇粉色或亮黄色虚线框
        QPen pen(QColor(255, 105, 180), 4, Qt::DashLine);
        painter.setPen(pen);

        // 绘制一个比方块稍微小一点的圆角矩形边框
        painter.drawRoundedRect(2, 2, width() - 4, height() - 4, 10, 10);

        // 可选：给选中的方块加一层淡淡的发光遮罩
        painter.fillRect(rect(), QColor(255, 255, 255, 50));
    }
}




void BlockWidget::setSelected(bool selected) {
    m_isSelected = selected;

    if (m_isSelected) {
        // 创建“惊吓”缩放动画
        QPropertyAnimation *anim = new QPropertyAnimation(this, "scale");
        anim->setDuration(100); // 100ms 快速跳变

        // 设置动画关键帧：1.0 -> 1.1 -> 1.0
        anim->setStartValue(1.0);
        anim->setKeyValueAt(0.5, 1.1); // 50ms 时放大到 1.1
        anim->setEndValue(1.0);

        // 设置缓动曲线，让跳动更有弹性感
        anim->setEasingCurve(QEasingCurve::OutQuad);

        anim->start(QAbstractAnimation::DeleteWhenStopped);
    }

    update(); // 确保选中框的绘制
}
