#include "BlowPortForm.h"
#include "ui_BlowPortForm.h"



#include "Server/State.h"

BlowPortForm::BlowPortForm(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::BlowPortForm)
{
    ui->setupUi(this);

    InitPotrBtn();

}

BlowPortForm::~BlowPortForm()
{
    delete ui;
}

void BlowPortForm::InitPotrBtn()
{
    int col = 8;
    for(int i = 0; i < State::BlowsSet::PoreCount; i++)
    {
        QToolButton* btn = new QToolButton(this);
        btn->setText(QString::number(i+1));
        btn->setMinimumWidth(60);
        btn->setMinimumHeight(60);
        connect(btn, &QToolButton::clicked, this, [this, i]() {
            int point = Server::GetPtr()->getCarEncoder( i >= 48 ? 1: 0);
            if(State::PLCInfo::SpeedState)
            {
                point += 2000;
            }
            Server::GetPtr()->setPortBlow(i,  point, 27);
        });
        ui->gridLayout->addWidget(btn, i / 8, i % 8);
        m_btns.append(btn);
    }


}


