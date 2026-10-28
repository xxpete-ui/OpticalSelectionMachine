#ifndef BLURREDFRAME_H
#define BLURREDFRAME_H

#include <QFrame>
#include <QPixmap>

class BlurredFrame : public QFrame
{
    Q_OBJECT
public:
    explicit BlurredFrame(QWidget *parent = nullptr);

    // 设置模糊半径，数值越大越模糊
    void setBlurRadius(int radius);

    // 手动刷新背景（当父窗口内容变化时调用）
    void updateBlurredBackground();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    int m_blurRadius;
    QPixmap m_blurredPixmap;
};

#endif // BLURREDFRAME_H