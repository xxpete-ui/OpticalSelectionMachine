#include "Widget.h"
#include "ui_widget.h"

#include <QGraphicsDropShadowEffect>
#include <QStyleOption>
#include <QPointer>          // 必须包含此头文件
#include <QMouseEvent>
#include <QFileDialog>
#include <QMessageBox>
#include <QString>
#include <QtConcurrent/QtConcurrent>
#include <QFutureWatcher>
#include <QLayout>

#include "Server/State.h"
#include "Form/TableForm.h"
#include "Form/SettingForm.h"

Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
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

    // 设置主页的Tab
    ui->HomeTabWidget->tabBar()->hide();

    // 本地图片模式默认关闭，先禁用上/下一张，勾选「使用本地图片」后才可用
    ui->Tab2_UpImg_Btn->setEnabled(false);
    ui->Tab2_DownImg_Btn->setEnabled(false);

    InitImageShow();

    // 使用 QtConcurrent::task 执行初始化
    auto future = QtConcurrent::task([this]() {
        Server::GetPtr()->Init();
        Server::GetPtr()->Start();
    }).spawn();
    QFutureWatcher<void> *watcher = new QFutureWatcher<void>(this);
    connect(watcher, &QFutureWatcher<void>::finished, [=]() {
        State::imgProcessInitSuccess = true;    // 声明初始化完成
        watcher->deleteLater();
    });
    watcher->setFuture(future);
    //m_server.Init();

    //ui->Tab2_Frame_2->setVisible(false);
    // 添加数据列表
    TableForm* tableForm = new TableForm(ui->Tab1_TableFrame);
    QVBoxLayout *layout = new QVBoxLayout(ui->Tab1_TableFrame);
    layout->setContentsMargins(0, 0, 0, 0); // 去除内边距，让自定义页面填满整个 QFrame
    layout->addWidget(tableForm);

    SettingForm* setForm = new SettingForm(this);
    ui->HomeTabWidget->addTab(setForm, "设置");

    timer_Tab1 = new QTimer(this);
    connect(timer_Tab1, &QTimer::timeout, this, &Widget::onUpdateTab1);
    // 启动定时器
    timer_Tab1->start(1000);
}

Widget::~Widget()
{
    timer->stop();
    timer_Tab1->stop();
    Server::GetPtr()->DeletePtr();
    delete ui;
}


void Widget::on_Min_Button_clicked()
{
    this->showMinimized();
}

void Widget::on_Max_Button_clicked()
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

void Widget::on_Close_Button_clicked()
{
    this->close();
}

void Widget::paintEvent(QPaintEvent *)
{
    // 让自定义控件样式表生效
    // QStyleOption opt;
    // opt.initFrom(this);
    // QPainter p(this);
    // style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
    QPainter p(this);
    p.fillRect(rect(), Qt::transparent);

}

bool Widget::eventFilter(QObject *obj, QEvent *event)
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

void Widget::changeEvent(QEvent *event)
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
                        border: none; \
                        /*border-image: url(:/images/back1.png); */\
                        border-image: url(:/images/img1.png); \
                        border-radius:0px; \
                        }");
                ui->MenuFrame -> setStyleSheet("#MenuFrame{\
                        color: rgb(255, 255, 255);\
                        border-top-left-radius: 0px;\
                        border-bottom-left-radius: 0px;\
                        /*background-color: rgb(10, 31, 57);*/\
                        background-color: rgba(41, 0, 52, 150);\
                        }");
                ui->Max_Button->setIcon(QIcon(":/images/icon-copy.png"));
            }
            else if(state == Qt::WindowNoState && statePtr->oldState() == Qt::WindowMaximized)
            {
                ui->MainFrame->setStyleSheet("#MainFrame{ \
                        border: none; \
                        /*border-image: url(:/images/back1.png); */\
                        border-image: url(:/images/img1.png); \
                        border-radius:10px; \
                        }");
                ui->MenuFrame -> setStyleSheet("#MenuFrame{\
                        color: rgb(255, 255, 255);\
                        border-top-left-radius: 10px;\
                        border-bottom-left-radius: 10px;\
                        /*background-color: rgb(10, 31, 57);*/\
                        background-color: rgba(41, 0, 52, 150);\
                        }");
                ui->Max_Button->setIcon(QIcon(":/images/Icon_max4.png"));

            }
        }
    }
}


/// 初始化图片显示控件
void Widget::InitImageShow()
{
    // 大图显示
    m_bigImgShow = new ImageDisplayWidget(this);
    ui->BigImgLayout->addWidget(m_bigImgShow);

    for(int i = 0; i < 10; i++)
    {
        int row = i / 5 * 2;
        int col = i % 5;
        QPointer<QLabel> label = new QLabel(this);
        label->setMaximumHeight(40);
        label->setMinimumHeight(25);
        label->setAlignment(Qt::AlignCenter); // 文字居中
        ui->small_gridLayout->addWidget(label, row, col, Qt::AlignCenter);

        QPointer<ImageDisplayWidget> smallImg = new ImageDisplayWidget(this);
        smallImg->setMinimumSize(100, 120);
        ui->small_gridLayout->addWidget(smallImg.data(), row + 1, col, Qt::AlignCenter);

        m_smallImgLabel.push_back(label);
        m_smallImgShow.push_back(smallImg);

    }

    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &Widget::onUpdateImageShow);
    // 启动定时器
    timer->start(50);

}


/// 更新图像显示
void Widget::onUpdateImageShow()
{
    auto imgData = State::showImgQueue.pop();
    if(imgData.has_value())
    {
        // 更新显示控件
        if (m_bigImgShow) {
            m_bigImgShow->updateImage(imgData->BigImage);
        }
        for(auto it : imgData->SmallImages)
        {
            if(m_smallShowIndex >= 10)
            {
                m_smallShowIndex = 0;
            }
            m_smallImgLabel[m_smallShowIndex]->setText(QString::fromStdString(it.first));
            m_smallImgShow[m_smallShowIndex]->updateImage(it.second);
            m_smallShowIndex++;
        }

    }
}

void Widget::onUpdateTab1()
{
    int totalCount = 0;
    int dateTotal = 0;
    int rangeCount = 0;
    auto runTypes = Server::GetPtr()->getCollectorPtr()->getRunTypeCount();
    auto dateTypes = Server::GetPtr()->getCollectorPtr()->getDateTypeCount();
    for(auto it = runTypes.begin(); it != runTypes.end(); it++)
    {
        totalCount += it.value();
        if(State::blowCounts.find(it.key().toStdString()) != State::blowCounts.end())
        {
            rangeCount += it.value();
        }
    }
    for(auto it : dateTypes)
    {
        dateTotal += it;
    }
    ui->Tab1_Text_RunCount->setText(QString::number(totalCount));
    ui->Tab1_Text_DateCount->setText(QString::number(totalCount + dateTotal));
    ui->Tab1_Text_RunRito->setText(QString::number(totalCount > 0 ? rangeCount * 100 / totalCount : 0) + "%");
    ui->Tab1_Text_RunTime->setText(State::getRunningTime(State::RunTime));
}



/// 图像页
void Widget::on_toolButton_clicked()
{
    ui->HomeTabWidget->setCurrentIndex(1);
}

/// 首页
void Widget::on_HomeBtn_clicked()
{
    ui->HomeTabWidget->setCurrentIndex(0);
}



// 图像页 - 设置图像路径
void Widget::on_Tab2_Path_Btn_clicked()
{
    QString dir = QFileDialog::getExistingDirectory(nullptr, "选择文件夹", ui->Tab2_PathEdit->text());
    if (!dir.isEmpty()) {
        ui->Tab2_PathEdit->setText(dir);
    }
}

// 图像页 - 图像路径变化
void Widget::on_Tab2_PathEdit_textChanged(const QString &arg1)
{
    const QString path = arg1.trimmed();
    if (path.isEmpty()) {
        Server::GetPtr()->SetReadImagePath("");     // 清空输入框 = 清空图片列表
        return;
    }
    QDir dir(path);
    if (!dir.exists()) {
        // 正在逐字输入时路径本来就不完整，这里不弹窗，等输入完整后自然生效
        return;
    }
    Server::GetPtr()->SetReadImagePath(path.toStdString());
}

// 图像页 - 设置读取图像
void Widget::ApplyLocalImageMode(bool on)
{
    Server::GetPtr()->SetReadImage(on);
    ui->Tab2_UpImg_Btn->setEnabled(on);
    ui->Tab2_DownImg_Btn->setEnabled(on);

    // 勾选了本地模式却一张图都没扫到，直接提示，否则点按钮毫无反应会很迷惑
    if (on && Server::GetPtr()->GetLocalImageCount() == 0) {
        QMessageBox::warning(this, "没有可用图片",
                             "当前文件夹里没有找到图片。\n\n"
                             "支持格式：jpg / jpeg / png / bmp / tif / tiff\n"
                             "请点击「选择文件夹」重新指定图片目录。");
    }
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
void Widget::on_Tab2_UseImg_CheckBox_checkStateChanged(Qt::CheckState state)
{
    ApplyLocalImageMode(state != Qt::CheckState::Unchecked);
}
#else
void Widget::on_Tab2_UseImg_CheckBox_stateChanged(int state)
{
    ApplyLocalImageMode(state != 0);
}
#endif


void Widget::on_Tab2_UpImg_Btn_clicked()
{
    Server::GetPtr()->UpImage();
}


void Widget::on_Tab2_DownImg_Btn_clicked()
{
    Server::GetPtr()->DownImage();
}


void Widget::on_Setting_Btn_clicked()
{
    ui->HomeTabWidget->setCurrentIndex(2);
}

