#include "BlurredFrame.h"
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QGraphicsBlurEffect>
#include <QPainter>
#include <QResizeEvent>

BlurredFrame::BlurredFrame(QWidget *parent)
    : QFrame(parent), m_blurRadius(15)
{
    // 使控件自身背景透明，只显示我们绘制的模糊图像
    setAttribute(Qt::WA_TranslucentBackground);
    setStyleSheet("background: transparent; border: none;");
}

void BlurredFrame::setBlurRadius(int radius)
{
    m_blurRadius = radius;
    updateBlurredBackground();
}

void BlurredFrame::updateBlurredBackground()
{
    QWidget *parent = parentWidget();
    if (!parent) return;

    // 获取当前控件在父窗口中的矩形区域（相对于父窗口坐标）
    QRect rectInParent = geometry();
    // 从父窗口截取该区域的图像
    QPixmap original = parent->grab(rectInParent);
    if (original.isNull()) return;

    // 创建临时场景对图像应用模糊效果
    QGraphicsScene scene;
    QGraphicsPixmapItem item(original);
    scene.addItem(&item);

    QGraphicsBlurEffect blur;
    blur.setBlurRadius(m_blurRadius);
    item.setGraphicsEffect(&blur);

    // 渲染模糊后的图像
    QPixmap blurred(original.size());
    blurred.fill(Qt::transparent);
    QPainter painter(&blurred);
    scene.render(&painter);
    painter.end();

    m_blurredPixmap = blurred;
    update();  // 触发重绘
}

void BlurredFrame::resizeEvent(QResizeEvent *event)
{
    QFrame::resizeEvent(event);
    updateBlurredBackground();
}

void BlurredFrame::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    if (!m_blurredPixmap.isNull()) {
        painter.drawPixmap(rect(), m_blurredPixmap);
    }
    // 注意：不调用 QFrame::paintEvent，因为我们自己绘制了背景
    // 如果需要保留子控件，应在上层布局中放置子控件，这个类仅作为容器
}