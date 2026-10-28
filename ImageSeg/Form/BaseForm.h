#ifndef BASEFORM_H
#define BASEFORM_H

#include <QWidget>

namespace Ui {
class BaseForm;
}

class BaseForm : public QWidget
{
    Q_OBJECT

public:
    explicit BaseForm(QWidget *parent = nullptr);
    ~BaseForm();

protected:
    /// 界面重绘事件
    void paintEvent(QPaintEvent *) override;

    /// 鼠标移动事件
    bool eventFilter(QObject *obj, QEvent *event) override;

    /// 窗体变化事件
    void changeEvent(QEvent *event) override;


private slots:
    void on_Min_Button_clicked();

    void on_Max_Button_clicked();

    void on_Close_Button_clicked();

private:
    Ui::BaseForm *ui;

    bool IsMaxShow;         // 当前是否最大化显示
    QPointF m_dragPosition;  // 记录鼠标按下时的全局位置
    bool m_dragging = false;
};

#endif // BASEFORM_H
