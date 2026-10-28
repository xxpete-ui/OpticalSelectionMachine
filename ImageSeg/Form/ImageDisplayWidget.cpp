#include "ImageDisplayWidget.h"
#include "ui_ImageDisplayWidget.h"

#include <QWheelEvent>


QPixmap cvMatToQPixmap(const cv::Mat& mat) {
    if (mat.empty()) {
        return QPixmap();
    }

    switch (mat.type()) {
    case CV_8UC3: { // 彩色图像（BGR）
        cv::Mat rgb;
        cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
        QImage img(rgb.data, rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888);
        return QPixmap::fromImage(img);
    }
    case CV_8UC1: { // 灰度图像
        QImage img(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_Grayscale8);
        return QPixmap::fromImage(img);
    }
    default: {
        // 其他类型（如浮点图像）可以酌情转换，此处返回空
        qWarning() << "Unsupported cv::Mat type for conversion to QPixmap";
        return QPixmap();
    }
    }
}


ImageDisplayWidget::ImageDisplayWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ImageDisplayWidget)
{
    ui->setupUi(this);

    // 1. 创建场景
    scene = new QGraphicsScene(this);

    // 2. 应用到视图
    ui->graphicsView->setScene(scene);

    // 存储原始图片
    QPixmap outsidePixmap;
    // 关键配置：按比例缩放，保持宽高比
    ui->graphicsView->setAlignment(Qt::AlignCenter); // 内容居中
    ui->graphicsView->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    ui->graphicsView->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    // 可选：允许用户用鼠标拖动图片
    ui->graphicsView->setDragMode(QGraphicsView::ScrollHandDrag);

    // 设置鼠标为缩放中心
    ui->graphicsView->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);  // 关键：以鼠标为中心缩放
    ui->graphicsView->installEventFilter(this);

    ui->graphicsView->setStyleSheet("background-color: rgba(255, 255, 255, 20)");

    // 3. 创建图元项
    item = new QGraphicsPixmapItem();

    // 4. 将图元项添加到场景
    scene->addItem(item);
}

ImageDisplayWidget::~ImageDisplayWidget()
{
    delete ui;
}

void ImageDisplayWidget::updateImage(const QPixmap &pixmap) {
    // 在外部维护 QPixmap 是性能优化的关键,
    // 可直接调用 setPixmap 来更新显示
    item->setPixmap(pixmap);
    scene->setSceneRect(pixmap.rect());
    // 核心：使图片适应视图大小，保持宽高比，居中
    ui->graphicsView->fitInView(item, Qt::KeepAspectRatio);
}


void ImageDisplayWidget::updateImage(const cv::Mat& mat)
{
    QPixmap pix = cvMatToQPixmap(mat);
    if (!pix.isNull())
    {
        updateImage(pix);
    }
}

void ImageDisplayWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (!item->pixmap().isNull()) {
        ui->graphicsView->fitInView(item, Qt::KeepAspectRatio);
    }
}

bool ImageDisplayWidget::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == ui->graphicsView && event->type() == QEvent::Wheel) {
        QWheelEvent* wheelEvent = static_cast<QWheelEvent*>(event);

        // 滚轮向上放大，向下缩小
        if (wheelEvent->angleDelta().y() > 0) {
            m_zoomFactor *= 1.1;
        } else {
            m_zoomFactor /= 1.1;
        }

        // 限制缩放范围
        m_zoomFactor = qBound(0.2, m_zoomFactor, 5.0);

        // 应用缩放
        QTransform transform;
        transform.scale(m_zoomFactor, m_zoomFactor);
        ui->graphicsView->setTransform(transform);

        event->accept();
        return true;
    }
    return QWidget::eventFilter(obj, event);
}
