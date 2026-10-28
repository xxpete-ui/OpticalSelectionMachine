#include "SettingForm.h"
#include "ui_SettingForm.h"

#include <QSqlQuery>
#include <QSqlDatabase>
#include <QSqlError>

#include "Server/State.h"
//#include "Database/DatabasePool.h"
#include "Form/Blow/BlowPortForm.h"
#include "NetWork/ModbusTcpSlave.h"
#include "Collector/PlcClient.h"

///
/// \brief updateConfig 更新数据库配置表
/// \param key
/// \param value
/// \param db
/// \return
///
bool updateConfig(const QString &key, const QVariant &value, QSqlDatabase &db) {
    QSqlQuery query(db);
    query.prepare("UPDATE ConfigInfo SET value = :value WHERE key = :key");
    query.bindValue(":value", value);
    query.bindValue(":key", key);
    if (!query.exec()) {
        qWarning() << "Update config failed for key:" << key << ", error:" << query.lastError().text();
        return false;
    }
    return true;
}

SettingForm::SettingForm(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::SettingForm)
{
    ui->setupUi(this);

    InitComboBox();

    if(ui->SetTab->currentIndex() < 0)
    {
        ui->SetTab->setCurrentIndex(0);
    }
    InitValue(ui->SetTab->currentIndex());
}

SettingForm::~SettingForm()
{
    delete ui;
}

void SettingForm::InitComboBox()
{
    QStringList strs = {"否", "是"};
    ui->Tab1_BGR_Combo->addItems(strs);
    ui->Tab1_TriggerSource_Combo->addItems(strs);

    ui->Tab2_GPU_Combo->addItems(strs);
    ui->Tab2_RepeatJudgment_Edit->addItems(strs);
    ui->Tab2_SizeLimit_Combo->addItems(strs);
    ui->Tab2_UseGaussianBlur_Combo->addItems(strs);

    ui->Tab3_IsBlow_Combo->addItems(strs);
}

void SettingForm::InitValue(int index)
{
    if(index == 0)
    {
        ui->Tab1_IP_Edit->setText(QString::fromStdString(State::CameraInfo::IpAddress));
        ui->Tab1_Hight_Edit->setText(QString::number(State::CameraInfo::HightProportion));
        ui->Tab1_LineRate_Edit->setText(QString::number(State::CameraInfo::LineRateRatio));
        ui->Tab1_BGR_Combo->setCurrentIndex(State::CameraInfo::SetImgBGR ? 1 : 0);
        ui->Tab1_TriggerSource_Combo->setCurrentIndex(State::CameraInfo::TriggerSource ? 1 : 0);
        ui->Tab1_TriggerTimer_Edit->setText(QString::number(State::CameraInfo::TriggerTimer));
    }
    else if(index == 1)
    {
        InitTab2Value();
    }
    else if(index == 2)
    {
        InitTab3Value();
    }
}

void SettingForm::InitTab2Value()
{
    ui->Tab2_GPU_Combo->setCurrentIndex(State::ModelInfo::UseGPU ? 1 : 0);
    ui->Tab2_Channel_Edit->setText(QString::number(State::ModelInfo::Channel));
    ui->Tab2_ChannelInterval_Edit->setText(QString::number(State::ModelInfo::ChannelBoundary));
    ui->Tab2_CarCount_Edit->setText(QString::number(State::BlowsSet::MotionCarCount));
    ui->Tab2_PoreCount_Edit->setText(QString::number(State::BlowsSet::PoreCount));
    ui->Tab2_PoreInterval_Edit->setText(QString::number(State::BlowsSet::PoreInterval));
    ui->Tab2_BlowStartX_Edit->setText(QString::number(State::BlowsSet::BlowStartX));

    ui->Tab2_BlowOffset_Edit->setText(QString::number(State::BlowsSet::BlowOffset));
    ui->Tab2_BlowTime_Edit->setText(QString::number(State::BlowsSet::BlowTime));
    ui->Tab2_ZoomRatio_Edit->setText(QString::number(State::BlowsSet::ZoomRatio));
    ui->Tab2_SegThreshold_Edit->setText(QString::number(State::BottleControl::SegThreshold));
    ui->Tab2_ClsThreshold_Edit->setText(QString::number(State::BottleControl::ClsThreshold));
    ui->Tab2_BlowAreaStart_Edit->setText(QString::number(State::BottleControl::BlowAreaStart));
    ui->Tab2_BlowAreaEnd_Edit->setText(QString::number(State::BottleControl::BlowAreaEnd));
    ui->Tab2_RepeatJudgment_Edit->setCurrentIndex(State::BottleControl::RepeatJudgment ? 1 : 0);

    ui->Tab2_SizeLimit_Combo->setCurrentIndex(State::BottleControl::SizeLimit ? 1 : 0);
    ui->Tab2_MinWidth_Edit->setText(QString::number(State::BottleControl::MinWidth));
    ui->Tab2_MinHeight_Edit->setText(QString::number(State::BottleControl::MinHeight));
    ui->Tab2_MaxWidth_Edit->setText(QString::number(State::BottleControl::MaxWidth));
    ui->Tab2_MaxHeight_Edit->setText(QString::number(State::BottleControl::MaxHeight));
    ui->Tab2_UseGaussianBlur_Combo->setCurrentIndex(State::BottleControl::UseGaussianBlur ? 1 : 0);

}

void SettingForm::InitTab3Value()
{
    ui->Tab3_PLC_IP_Edit->setText(State::PLCInfo::IPAddress.c_str());
    ui->Tab3_PLC_Port_Edit->setText(QString::number(State::PLCInfo::Port));
    ui->Tab3_IsBlow_Combo->setCurrentIndex(State::BlowsSet::IsBlow ? 1 : 0);
    ui->Tab3_PLC_Speed_Edit->setText(QString::number(State::PLCInfo::Speed));

    ui->Tab3_Mb_IP_Edit->setText(State::ModbusInfo::IPAddress.c_str());
    ui->Tab3_Mb_Port_Edit->setText(QString::number(State::ModbusInfo::Port));
}

void SettingForm::on_SetTab_currentChanged(int index)
{
    InitValue(index);
}

void SettingForm::on_CameraSetBtn_clicked()
{
    try
    {
        State::CameraInfo::IpAddress = ui->Tab1_IP_Edit->text().toStdString();
        State::CameraInfo::HightProportion = ui->Tab1_Hight_Edit->text().toFloat();
        State::CameraInfo::LineRateRatio = ui->Tab1_LineRate_Edit->text().toFloat();
        State::CameraInfo::SetImgBGR = ui->Tab1_BGR_Combo->currentIndex();
        State::CameraInfo::TriggerSource = ui->Tab1_TriggerSource_Combo->currentIndex();
        State::CameraInfo::TriggerTimer = ui->Tab1_TriggerTimer_Edit->text().toInt();

        auto db = Server::GetPtr()->getDatabasePtr();
        if(db->transaction())
        {
            bool ok = true;
            ok &= updateConfig("CameraIP", ui->Tab1_IP_Edit->text(), *db);
            ok &= updateConfig("HightProportion", ui->Tab1_Hight_Edit->text(), *db);
            ok &= updateConfig("LineRateRatio", ui->Tab1_LineRate_Edit->text(), *db);
            ok &= updateConfig("SetImgBGR", ui->Tab1_BGR_Combo->currentIndex(), *db);
            ok &= updateConfig("TriggerSource", ui->Tab1_TriggerSource_Combo->currentIndex(), *db);
            ok &= updateConfig("TriggerTimer", ui->Tab1_TriggerTimer_Edit->text(), *db);

            if (ok && db->commit()) {
                qDebug() << "All configs updated successfully";
            } else {
                db->rollback();
                qWarning() << "Transaction rolled back due to error";
            }

        }

    }
    catch(std::runtime_error excepion)
    {

    }
}


void SettingForm::on_Tab2_OtherSet_Btn_clicked()
{
    try
    {
        State::ModelInfo::UseGPU = ui->Tab2_GPU_Combo->currentIndex();
        State::ModelInfo::Channel = ui->Tab2_Channel_Edit->text().toInt();
        State::ModelInfo::ChannelBoundary = ui->Tab2_ChannelInterval_Edit->text().toInt();
        State::BlowsSet::MotionCarCount = ui->Tab2_CarCount_Edit->text().toInt();
        State::BlowsSet::PoreCount = ui->Tab2_PoreCount_Edit->text().toInt();
        State::BlowsSet::PoreInterval = ui->Tab2_PoreInterval_Edit->text().toFloat();
        State::BlowsSet::BlowStartX = ui->Tab2_BlowStartX_Edit->text().toFloat();

        State::BlowsSet::BlowOffset = ui->Tab2_BlowOffset_Edit->text().toInt();
        State::BlowsSet::BlowTime = ui->Tab2_BlowTime_Edit->text().toInt();
        State::BlowsSet::ZoomRatio = ui->Tab2_ZoomRatio_Edit->text().toFloat();
        State::BottleControl::SegThreshold = ui->Tab2_SegThreshold_Edit->text().toFloat();
        State::BottleControl::ClsThreshold = ui->Tab2_ClsThreshold_Edit->text().toFloat();
        State::BottleControl::BlowAreaStart = ui->Tab2_BlowAreaStart_Edit->text().toInt();
        State::BottleControl::BlowAreaEnd = ui->Tab2_BlowAreaEnd_Edit->text().toInt();
        State::BottleControl::RepeatJudgment = ui->Tab2_RepeatJudgment_Edit->currentIndex();

        State::BottleControl::SizeLimit = ui->Tab2_SizeLimit_Combo->currentIndex();
        State::BottleControl::MinWidth = ui->Tab2_MinWidth_Edit->text().toInt();
        State::BottleControl::MinHeight = ui->Tab2_MinHeight_Edit->text().toInt();
        State::BottleControl::MaxWidth = ui->Tab2_MaxWidth_Edit->text().toInt();
        State::BottleControl::MaxHeight = ui->Tab2_MaxHeight_Edit->text().toInt();
        State::BottleControl::UseGaussianBlur = ui->Tab2_UseGaussianBlur_Combo->currentIndex();

        auto db = Server::GetPtr()->getDatabasePtr();
        if(db->transaction())
        {
            bool ok = true;
            ok &= updateConfig("UseGPU", ui->Tab2_GPU_Combo->currentIndex(), *db);
            ok &= updateConfig("Channel", ui->Tab2_Channel_Edit->text(), *db);
            ok &= updateConfig("ChannelBoundary", ui->Tab2_ChannelInterval_Edit->text(), *db);
            ok &= updateConfig("MotionCarCount", ui->Tab2_CarCount_Edit->text(), *db);
            ok &= updateConfig("PoreCount", ui->Tab2_PoreCount_Edit->text(), *db);
            ok &= updateConfig("PoreInterval", ui->Tab2_PoreInterval_Edit->text(), *db);
            ok &= updateConfig("BlowStartX", ui->Tab2_BlowStartX_Edit->text(), *db);

            ok &= updateConfig("BlowOffset", ui->Tab2_BlowOffset_Edit->text(), *db);
            ok &= updateConfig("BlowTime", ui->Tab2_BlowTime_Edit->text(), *db);
            ok &= updateConfig("ZoomRatio", ui->Tab2_ZoomRatio_Edit->text(), *db);
            ok &= updateConfig("SegThreshold", ui->Tab2_SegThreshold_Edit->text(), *db);
            ok &= updateConfig("ClsThreshold", ui->Tab2_ClsThreshold_Edit->text(), *db);
            ok &= updateConfig("BlowAreaStart", ui->Tab2_BlowAreaStart_Edit->text(), *db);
            ok &= updateConfig("BlowAreaEnd", ui->Tab2_BlowAreaEnd_Edit->text(), *db);
            ok &= updateConfig("RepeatJudgment", ui->Tab2_RepeatJudgment_Edit->currentIndex(), *db);

            ok &= updateConfig("SizeLimit", ui->Tab2_SizeLimit_Combo->currentIndex(), *db);
            ok &= updateConfig("MinWidth", ui->Tab2_MinWidth_Edit->text(), *db);
            ok &= updateConfig("MinHeight", ui->Tab2_MinHeight_Edit->text(), *db);
            ok &= updateConfig("MaxWidth", ui->Tab2_MaxWidth_Edit->text(), *db);
            ok &= updateConfig("MaxHeight", ui->Tab2_MaxHeight_Edit->text(), *db);
            ok &= updateConfig("UseGaussianBlur", ui->Tab2_UseGaussianBlur_Combo->currentIndex(), *db);

            if (ok && db->commit()) {
                qDebug() << "All configs updated successfully";
            } else {
                db->rollback();
                qWarning() << "Transaction rolled back due to error";
            }

        }

    }
    catch(std::runtime_error excepion)
    {

    }
}


void SettingForm::on_Tab3_Ok_Btn_clicked()
{
    try
    {
        auto plc_ip = ui->Tab3_PLC_IP_Edit->text().toStdString();
        int plc_port = ui->Tab3_PLC_Port_Edit->text().toInt();
        State::BlowsSet::IsBlow = ui->Tab3_IsBlow_Combo->currentIndex();
        if(State::PLCInfo::IPAddress != plc_ip || State::PLCInfo::Port != plc_port)
        {
            State::PLCInfo::IPAddress = plc_ip;
            State::PLCInfo::Port = plc_port;
            PlcClient::GetPlcPtr()->Close();
            PlcClient::GetPlcPtr()->Open();
        }


        int speed = ui->Tab3_PLC_Speed_Edit->text().toInt();
        if(speed != State::PLCInfo::Speed)
        {
            State::PLCInfo::Speed = speed;
            PlcClient::GetPlcPtr()->SetConfig();
        }

        auto modbus_ip = ui->Tab3_Mb_IP_Edit->text().toStdString();
        int mb_port = ui->Tab3_Mb_Port_Edit->text().toInt();
        if(State::ModbusInfo::IPAddress != modbus_ip || State::ModbusInfo::Port != mb_port)
        {
            State::ModbusInfo::IPAddress = modbus_ip;
            State::ModbusInfo::Port = mb_port;

            ModbusTcpSlave::GetPtr()->Close();
            ModbusTcpSlave::GetPtr()->Open(State::ModbusInfo::Port);
        }

        auto db = Server::GetPtr()->getDatabasePtr();
        if(db->transaction())
        {
            bool ok = true;
            ok &= updateConfig("PLC_IP", ui->Tab3_Mb_IP_Edit->text(), *db);
            ok &= updateConfig("PLC_Port", ui->Tab3_PLC_Port_Edit->text(), *db);
            ok &= updateConfig("PLC_Speed", ui->Tab3_PLC_Speed_Edit->text(), *db);

            ok &= updateConfig("Modbus_IP", ui->Tab2_CarCount_Edit->text(), *db);
            ok &= updateConfig("Modbus_Port", ui->Tab3_Mb_Port_Edit->text(), *db);

            if (ok && db->commit()) {
                qDebug() << "All configs updated successfully";
            } else {
                db->rollback();
                qWarning() << "Transaction rolled back due to error";
            }

        }


    }
    catch (...)
    {
    }
}


void SettingForm::on_Tab3_PLC_Clear_Btn_clicked()
{
    PlcClient::GetPlcPtr()->Clean_valve();
}


void SettingForm::on_Tab3_AutoBlow_Btn_clicked()
{
    int car = 0;
    for(int i = 0; i < State::BlowsSet::PoreCount; i++)
    {
        if(i >= 48)
        {
            car = 1;
        }
        Server::GetPtr()->setPortBlow(i, Server::GetPtr()->getCarEncoder(car), 27);
    }
}


void SettingForm::on_Tab3_OnePortBlow_Btn_clicked()
{
    BlowPortForm form(this);
    form.exec();
}


void SettingForm::on_Tab3_PLC_SpeedSave_Btn_clicked()
{
    State::PLCInfo::Speed = ui->Tab3_PLC_Speed_Edit->text().toInt();
    PlcClient::GetPlcPtr()->SetConfig();
}

