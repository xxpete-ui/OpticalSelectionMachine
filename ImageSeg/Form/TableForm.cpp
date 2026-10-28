#include "TableForm.h"
#include "ui_TableForm.h"

#include <QButtonGroup>
#include <string>

#include "Server/State.h"
#include "Server/Server.h"

TableForm::TableForm(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::TableForm)
    , ShowDate(true)
{
    ui->setupUi(this);


    ui->pushButton->setCheckable(true);
    ui->pushButton_2->setCheckable(true);
    QButtonGroup *group = new QButtonGroup(this);
    group->addButton(ui->pushButton, 0);
    group->addButton(ui->pushButton_2, 1);
    group->setExclusive(true);  // 确保互斥

    ui->pushButton_2->setChecked(true); // 默认选中当天
    // 信号槽
    connect(group, &QButtonGroup::buttonClicked, this, [this](QAbstractButton *button){
            if(button->text() == "本日")
            {
                ShowDate = true;
            }
            else
            {
                ShowDate = false;
            }

            qDeleteAll(m_typeLabel);
            m_typeLabel.clear();
            qDeleteAll(m_countLabel);
            m_countLabel.clear();
            qDeleteAll(m_ritoLabel);
            m_ritoLabel.clear();
            qDeleteAll(m_progress);
            m_progress.clear();

            qDebug() << button->text();
        });

    ui ->scrollArea->setWidgetResizable(true);

    //InitDataTable();
    SetDataStart();
}

TableForm::~TableForm()
{
    delete ui;
}

void TableForm::SetDataStart()
{
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &TableForm::UpdateTable);
    // 启动定时器
    timer->start(1000);
}

void TableForm::CreatDataRow(QString typeName)
{
    QString labelStyle = "QLabel{\
            background-color: rgba(255, 255, 255, 0);\
            color: rgb(255, 255, 255);\
            font: 12pt \"Microsoft YaHei UI\";\
        }";
    QString progressStyle = "#QProgressBar{\
            border: none;\
            background-color: #e0e0e0;\
            height: 8px;\
            border-radius: 0px;\
        }\
        #QProgressBar::chunk {\
            background-color: #4caf50;\
            border-top-right-radius: 4px;\
            border-bottom-right-radius: 4px;\
        }";

    QLabel* type = new QLabel(this);
    type->setText(typeName);
    type->setAlignment(Qt::AlignCenter); // 文字居中
    type->setStyleSheet(labelStyle);
    type->setVisible(true);
    type->setFixedHeight(30);
    type->setMinimumWidth(110);
    type->setMaximumWidth(110);
    m_typeLabel.insert(typeName, type);
    ui->TabelDataLayout->addWidget(type, m_typeLabel.count() - 1, 0);
    QLabel* rito = new QLabel(this);
    rito->setText("0");
    rito->setAlignment(Qt::AlignCenter); // 文字居中
    rito->setStyleSheet(labelStyle);
    rito->setVisible(true);
    rito->setFixedHeight(30);
    rito->setMinimumWidth(110);
    rito->setMaximumWidth(110);
    m_ritoLabel.insert(typeName, rito);
    ui->TabelDataLayout->addWidget(rito, m_ritoLabel.count() - 1, 2);
    QLabel* count = new QLabel(this);
    count->setText("0");
    count->setAlignment(Qt::AlignCenter); // 文字居中
    count->setStyleSheet(labelStyle);
    count->setVisible(true);
    count->setFixedHeight(30);
    count->setMinimumWidth(110);
    count->setMaximumWidth(110);
    m_countLabel.insert(typeName, count);
    ui->TabelDataLayout->addWidget(count, m_countLabel.count() - 1, 3);

    QProgressBar* progress = new QProgressBar(this);
    progress->setStyleSheet(progressStyle);
    progress->setTextVisible(false);
    progress->setVisible(true);
    //progress->setFixedHeight(30);
    progress->setValue(0);
    m_progress.insert(typeName, progress);
    ui->TabelDataLayout->addWidget(progress, m_progress.count() - 1, 1);

}

void TableForm::InitDataTable()
{

    for(auto item = State::typeCounts.begin(); item != State::typeCounts.end(); ++item)
    {
        CreatDataRow(item->first.c_str());
    }
}

void TableForm::UpdateTable()
{
    QMap<QString, int> typeData;
    if(ShowDate)
    {
        typeData = Server::GetPtr()->getCollectorPtr()->getDateTypeCount();
    }
    else
    {
        typeData = Server::GetPtr()->getCollectorPtr()->getRunTypeCount();
    }

    double totalCount = 0;
    for(auto it = typeData.begin(); it != typeData.end(); it++)
    {
        totalCount += it.value();
    }
    for(auto it = typeData.begin(); it != typeData.end(); it++)
    {
        if(it.value() > 0)
        {
            if(!m_typeLabel.contains(it.key()))
            {
                CreatDataRow(it.key());
            }
            m_countLabel[it.key()]->setText(QString::number(it.value()));
            double rito = it.value() / totalCount;
            m_ritoLabel[it.key()]->setText(QString::number(rito * 100) + "%");
            m_progress[it.key()]->setValue(rito * 100);
        }
    }
    ui->gridLayout->update();
}




