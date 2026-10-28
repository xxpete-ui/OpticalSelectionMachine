#pragma once
#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QLabel>       // QPointer<QLabel> 需要完整类型
//#include <QTimer>
//#include <QPointer>
//#include <QList>

#include "Form/ImageDisplayWidget.h"
#include "Server/Server.h"

QT_BEGIN_NAMESPACE
namespace Ui { class Widget; }
QT_END_NAMESPACE

class Widget : public QWidget
{
    Q_OBJECT

public:
    Widget(QWidget *parent = nullptr);
    ~Widget();

protected:
    /// 界面重绘事件
    void paintEvent(QPaintEvent *) override;

    /// 鼠标移动事件
    bool eventFilter(QObject *obj, QEvent *event) override;

    /// 窗体变化事件
    void changeEvent(QEvent *event) override;


private:
    /// 初始化图片显示控件
    void InitImageShow();

    /// 更新图像显示
    void onUpdateImageShow();

    /// 更新首页图像
    void onUpdateTab1();

private slots:
    void on_Min_Button_clicked();

    void on_Max_Button_clicked();

    void on_Close_Button_clicked();

    void on_toolButton_clicked();

    void on_HomeBtn_clicked();


    void on_Tab2_Path_Btn_clicked();

    void on_Tab2_PathEdit_textChanged(const QString &arg1);

    // QCheckBox 的信号名在 Qt 6.7 变了：之前是 stateChanged(int)，之后是 checkStateChanged(Qt::CheckState)。
    // 槽名必须是 on_<对象名>_<信号名> 才会被 connectSlotsByName 自动连上，写错就静默失效。
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    void on_Tab2_UseImg_CheckBox_checkStateChanged(Qt::CheckState state);
#else
    void on_Tab2_UseImg_CheckBox_stateChanged(int state);
#endif

    /// 统一切换「本地图片」模式：同步后端开关 + 上/下一张按钮可用性
    void ApplyLocalImageMode(bool on);

    void on_Tab2_UpImg_Btn_clicked();

    void on_Tab2_DownImg_Btn_clicked();

    void on_Setting_Btn_clicked();

private:
    Ui::Widget *ui;

    bool IsMaxShow = false;         // 当前是否最大化显示
    QPointF m_dragPosition;  // 记录鼠标按下时的全局位置
    bool m_dragging = false;

    ImageDisplayWidget* m_bigImgShow;   // 大图显示
    QList<QPointer<ImageDisplayWidget>> m_smallImgShow;  // 小图显示
    QList<QPointer<QLabel>> m_smallImgLabel;  // 小图显示
    int m_smallShowIndex = 0;
    QTimer* timer;          // 图像显示定时器
    QTimer* timer_Tab1;     // 首页数据更新

};
#endif // WIDGET_H
