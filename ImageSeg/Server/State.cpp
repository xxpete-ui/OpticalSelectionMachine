#include "State.h"
#include <QCoreApplication>

std::atomic<bool> State::ThreadRun = false;
std::atomic<bool> State::imgProcessInitSuccess = false;

CircularQueue<std::string> State::showMessage(30);


CircularQueue<cv::Mat> State::frameQueue(20);

CircularQueue<ShowImage> State::showImgQueue(20, false);

std::atomic<unsigned long long> State::frameCounter = 0;

std::map<std::string, std::atomic<int>> State::typeCounts;
std::map<std::string, std::atomic<int>> State::blowCounts;
std::map<std::string, int> State::typePoint;

std::atomic<bool> State::OrderRun = false;

QDateTime State::RunTime = QDateTime::currentDateTime();

QString State::getRunningTime(const QDateTime& startTime)
{
    qint64 msecs = startTime.msecsTo(QDateTime::currentDateTime());
    qint64 totalSecs = msecs / 1000;

    qint64 days = totalSecs / 86400;
    qint64 hours = (totalSecs % 86400) / 3600;
    qint64 minutes = (totalSecs % 3600) / 60;
    qint64 seconds = totalSecs % 60;

    if (days > 0)
        return QString("%1天 %2:%3:%4")
            .arg(days)
            .arg(hours, 2, 10, QChar('0'))
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'));
    else
        return QString("%1:%2:%3")
            .arg(hours, 2, 10, QChar('0'))
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'));
}



// 相机参数
float State::CameraInfo::LineRateRatio = 1.3;
std::string State::CameraInfo::IpAddress = "169.254.166.205";
std::atomic<bool> State::CameraInfo::SetImgBGR = false;
std::atomic<bool> State::CameraInfo::TriggerSource = false;
std::atomic<int> State::CameraInfo::TriggerTimer = 100;
float State::CameraInfo::HightProportion = 4;

// 模型参数
int State::ModelInfo::Mode = 1;
int State::ModelInfo::Channel = 1;
int State::ModelInfo::ChannelBoundary = 2048;
// 模型路径在 Server::Init() 里赋值
// 不能在静态初始化阶段调用 applicationDirPath()，那时 QApplication 还没创建，会返回空串
std::string State::ModelInfo::SegModelPath;
std::string State::ModelInfo::ClsModelPath;
bool State::ModelInfo::UseGPU = true;

/// 瓶子控制
bool State::BottleControl::SizeLimit = true;
int State::BottleControl::MinWidth = 20;
int State::BottleControl::MinHeight = 20;
int State::BottleControl::MaxWidth = 400;
int State::BottleControl::MaxHeight = 400;
float State::BottleControl::SegThreshold = 0.5;
float State::BottleControl::ClsThreshold = 0.7;
int State::BottleControl::BlowAreaStart = 462;
int State::BottleControl::BlowAreaHeight = 512;
int State::BottleControl::BlowAreaEnd = 974;
bool State::BottleControl::RepeatJudgment = true;
bool State::BottleControl::UseGaussianBlur = false;

// 吹气参数设置
std::atomic<bool> State::BlowsSet::IsBlow = true;
float State::BlowsSet::BlowStartX = 120;
int State::BlowsSet::PoreCount = 80;
float State::BlowsSet::PoreInterval = 26.5;
float State::BlowsSet::ZoomRatio = 0.7;
int State::BlowsSet::BlowOffset = 9200;
int State::BlowsSet::BlowTime = 20;
int State::BlowsSet::MotionCarCount = 2;


// PLC参数
std::string State::PLCInfo::IPAddress = "192.168.2.17";
std::atomic<int> State::PLCInfo::Port = 502;
std::atomic<bool> State::PLCInfo::IsOpen = false;
std::atomic<bool> State::PLCInfo::AutoRun = false;
std::atomic<int> State::PLCInfo::Speed = 40;
std::atomic<bool> State::PLCInfo::SpeedState = false;
std::atomic<bool> State::PLCInfo::LightSourceState = false;
std::atomic<bool> State::PLCInfo::ManualSpeedRun = false;
std::atomic<bool> State::PLCInfo::ManualLightSource = false;
std::atomic<bool> State::PLCInfo::EmergencyStop = false;
std::atomic<bool> State::PLCInfo::LackOfAirPressure = false;
std::atomic<bool> State::PLCInfo::FrequencyConverterFailure = false;
std::atomic<bool> State::PLCInfo::AirValvePositionError = false;
std::atomic<bool> State::PLCInfo::RearEquipmentAbnormality = false;
std::atomic<bool> State::PLCInfo::AllEmergencyStops = false;
std::atomic<bool> State::PLCInfo::ControlCabinetStop = false;

// modbus 参赛
std::string State::ModbusInfo::IPAddress = "127.0.0.1";
std::atomic<int> State::ModbusInfo::Port = 502;
std::atomic<bool> State::ModbusInfo::IsOpen = false;


