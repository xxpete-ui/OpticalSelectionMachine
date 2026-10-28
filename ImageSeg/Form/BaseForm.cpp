#include "BaseForm.h"
#include "ui_BaseForm.h"

#include <QGraphicsDropShadowEffect>
#include <QStyleOption>
#include <QPainter>
#include <QMouseEvent>


BaseForm::BaseForm(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::BaseForm)
{
    ui->setupUi(this);


    // 设置无标题框，保留右键菜单
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowSystemMenuHint);

    this->setAttribute(Qt::WA_TranslucentBackground, true);

    //int margin = ui->frame_tital->height();

    setContentsMargins(10, 10, 10, 10);

    // 为标题栏 frame 安装事件过滤器
    ui->frame_tital->installEventFilter(this);

    // 设置阴影
    QGraphicsDropShadowEffect *effect = new QGraphicsDropShadowEffect();
    effect->setOffset(0, 0);                    //设置向哪个方向产生阴影效果(dx,dy)，特别地，(0,0)代表向四周发散
    effect->setColor(QColor(0, 0, 0));       //设置阴影颜色，也可以setColor(QColor(220,220,220))
    //effect->setColor(QColor(10, 31, 57));       //设置阴影颜色，也可以setColor(QColor(220,220,220))
    effect->setBlurRadius(15);                  //设定阴影的模糊半径，数值越大越模糊
    ui->MainFrame->setGraphicsEffect(effect);

    QGraphicsDropShadowEffect *menuEffect = new QGraphicsDropShadowEffect();
    menuEffect->setOffset(2, 0);                    //设置向哪个方向产生阴影效果(dx,dy)，特别地，(0,0)代表向四周发散
    menuEffect->setColor(QColor(0, 0, 0, 100));       //设置阴影颜色，也可以setColor(QColor(220,220,220))
    //effect->setColor(QColor(10, 31, 57));       //设置阴影颜色，也可以setColor(QColor(220,220,220))
    menuEffect->setBlurRadius(15);                  //设定阴影的模糊半径，数值越大越模糊
    ui->MenuFrame->setGraphicsEffect(menuEffect);

}

BaseForm::~BaseForm()
{
    delete ui;
}

void BaseForm::on_Min_Button_clicked()
{
    this->showMinimized();
}


void BaseForm::on_Max_Button_clicked()
{
    if(!IsMaxShow)
    {
        //this->showFullScreen();
        setContentsMargins(0, 0, 0, 0);
        this->showMaximized();
        IsMaxShow = true;
    }
    else
    {
        setContentsMargins(10, 10, 10, 10);
        this->showNormal();
        IsMaxShow = false;
    }
}


void BaseForm::on_Close_Button_clicked()
{
    this->close();
}


void BaseForm::paintEvent(QPaintEvent *)
{
    // 让自定义控件样式表生效
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

bool BaseForm::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == ui->frame_tital && !IsMaxShow) {
        // 鼠标按下
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                m_dragPosition = mouseEvent->globalPosition();
                m_dragging = true;
                event->accept();
                return true;  // 事件已处理
            }
        }
        // 鼠标移动
        else if (event->type() == QEvent::MouseMove && m_dragging) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->buttons() & Qt::LeftButton) {
                QPointF delta = mouseEvent->globalPosition() - m_dragPosition;
                move((pos() + delta).toPoint());               // 移动窗口
                m_dragPosition = mouseEvent->globalPosition(); // 更新起点
                event->accept();
                return true;
            }
        }
        // 鼠标释放
        else if (event->type() == QEvent::MouseButtonRelease) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                m_dragging = false;
                event->accept();
                return true;
            }
        }
    }
    // 其他事件交给基类处理
    return QWidget::eventFilter(obj, event);
}

void BaseForm::changeEvent(QEvent *event)
{
    if(event->type() == QEvent::WindowStateChange)
    {
        auto statePtr = dynamic_cast<QWindowStateChangeEvent*>(event);
        if(statePtr != nullptr)
        {
            auto state = this->windowState();
            if(state == Qt::WindowMaximized && statePtr->oldState() == Qt::WindowNoState)
            {
                ui->MainFrame->setStyleSheet("#MainFrame{ \
                        border-image: url(:/images/back1.png); \
                        border-radius:0px; \
                        }");
                ui->MenuFrame -> setStyleSheet("#MenuFrame{\
                        color: rgb(255, 255, 255);\
                        border-top-left-radius: 0px;\
                        border-bottom-left-radius: 0px;\
                        background-color: rgb(10, 31, 57);\
                        }");
                ui->Max_Button->setIcon(QIcon(":/images/icon-copy.png"));
            }
            else if(state == Qt::WindowNoState && statePtr->oldState() == Qt::WindowMaximized)
            {
                ui->MainFrame->setStyleSheet("#MainFrame{ \
                        border-image: url(:/images/back1.png); \
                        border-radius:10px; \
                        }");
                ui->MenuFrame -> setStyleSheet("#MenuFrame{\
                        color: rgb(255, 255, 255);\
                        border-top-left-radius: 10px;\
                        border-bottom-left-radius: 10px;\
                        background-color: rgb(10, 31, 57);\
                        }");
                ui->Max_Button->setIcon(QIcon(":/images/Icon_max4.png"));

            }
        }
    }
}


