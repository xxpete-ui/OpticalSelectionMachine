#ifndef IMAGEDISPLAYWIDGET_H
#define IMAGEDISPLAYWIDGET_H

#include <QWidget>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>

#include <opencv2/opencv.hpp>

namespace Ui {
class ImageDisplayWidget;
}

class ImageDisplayWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ImageDisplayWidget(QWidget *parent = nullptr);
    ~ImageDisplayWidget();

    // 外部调用的方法，用于更新图像
    void updateImage(const QPixmap &pixmap);
    void updateImage(const cv::Mat &mat);

protected:
    void resizeEvent(QResizeEvent *event) override; // 尺寸大小变化事件
    bool eventFilter(QObject *obj, QEvent *event) override; // 鼠标缩放事件
private:
    Ui::ImageDisplayWidget *ui;
    QGraphicsScene *scene;       // 场景
    QGraphicsPixmapItem *item;   // 图元项9†L30-L31

    float m_zoomFactor = 1.0;  // 当前缩放因子
};

#endif // IMAGEDISPLAYWIDGET_H
