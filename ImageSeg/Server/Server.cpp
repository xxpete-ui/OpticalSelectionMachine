// 引用winsock2.h解决错误：In included file: redefinition of 'sockaddr'
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>

#include "Server.h"
#include <QSqlQuery>
#include <QVariantMap>
#include <QVariant>
#include <QString>
#include <QCoreApplication>
#include <QSqlError>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDebug>

#include <opencv2/opencv.hpp>
#include "State.h"
#include "Collector/PlcClient.h"
#include "NetWork/ModbusTcpSlave.h"

Server* Server::m_server = nullptr;
std::mutex Server::mtx;


Server::Server()
    : m_databasePool("QSQLITE", (QCoreApplication::applicationDirPath() + "/database/TaiChuGX.db"))
{
    collector = std::make_shared<CountCollector>(m_databasePool.getConnectionAutoPtr());
}

void Server::Init()
{
    // 模型目录跟随 exe 所在目录（必须在这里赋值：静态初始化阶段 QApplication 尚未创建）
    State::ModelInfo::SegModelPath = (QCoreApplication::applicationDirPath() + "/mode/seg").toStdString();
    State::ModelInfo::ClsModelPath = (QCoreApplication::applicationDirPath() + "/mode/cls").toStdString();

    auto sql = m_databasePool.getConnectionAutoPtr();
    // 更新配置信息
    QSqlQuery query(*sql);
    if(query.exec("select * from ConfigInfo;"))
    {
        QVariantMap configInfo;
        while(query.next())
        {
            QString key = query.value("Key").toString();
            configInfo[key] = query.value("Value");
        }

        State::ModelInfo::Mode = configInfo["Mode"].toInt();
        // 注意: Value 列是文本 "0"/"1", 用 toBool() 会把 "0" 也判成 true, 必须转 int 判断
        State::ModelInfo::UseGPU = (configInfo["UseGPU"].toInt() != 0);
        //State::ModelInfo::SegModelPath = configInfo["SegModelPath"].toString().toStdString();
        //State::ModelInfo::ClsModelPath = configInfo["ClsModelPath"].toString().toStdString();

        State::CameraInfo::HightProportion = configInfo["HightProportion"].toFloat();
        State::CameraInfo::IpAddress = configInfo["CameraIP"].toString().toStdString();
        State::CameraInfo::LineRateRatio = configInfo["LineRateRatio"].toFloat();
        State::CameraInfo::SetImgBGR = configInfo["SetImgBGR"].toBool();
        State::CameraInfo::TriggerSource = configInfo["TriggerSource"].toBool();
        State::CameraInfo::TriggerTimer = configInfo["TriggerTimer"].toInt();

        State::BottleControl::BlowAreaStart = configInfo["BlowAreaStart"].toInt();
        State::BottleControl::BlowAreaEnd = configInfo["BlowAreaEnd"].toInt();
        State::BottleControl::BlowAreaHeight = State::BottleControl::BlowAreaEnd - State::BottleControl::BlowAreaStart;
        State::BottleControl::ClsThreshold = configInfo["ClsThreshold"].toFloat();
        State::BottleControl::SegThreshold = configInfo["SegThreshold"].toFloat();
        State::BottleControl::MaxHeight = configInfo["MaxHeight"].toInt();
        State::BottleControl::MinHeight = configInfo["MinHeight"].toInt();
        State::BottleControl::MaxWidth = configInfo["MaxWidth"].toInt();
        State::BottleControl::MinWidth = configInfo["MinWidth"].toInt();
        State::BottleControl::RepeatJudgment = configInfo["RepeatJudgment"].toBool();
        State::BottleControl::SizeLimit = configInfo["SizeLimit"].toBool();
        State::BottleControl::UseGaussianBlur = configInfo["UseGaussianBlur"].toBool();

        State::BlowsSet::MotionCarCount = configInfo["MotionCarCount"].toInt();
        State::BlowsSet::PoreCount = configInfo["PoreCount"].toInt();
        State::BlowsSet::PoreInterval = configInfo["PoreInterval"].toFloat();
        State::BlowsSet::BlowStartX = configInfo["BlowStartX"].toFloat();
        State::BlowsSet::BlowOffset = configInfo["BlowOffset"].toInt();
        State::BlowsSet::BlowTime = configInfo["BlowTime"].toInt();
        State::BlowsSet::ZoomRatio = configInfo["ZoomRatio"].toFloat();


        State::PLCInfo::IPAddress = configInfo["PLC_IP"].toString().toStdString();
        State::PLCInfo::Port = configInfo["PLC_Port"].toInt();
        State::ModbusInfo::IPAddress = configInfo["Modbus_IP"].toString().toStdString();
        State::ModbusInfo::Port = configInfo["Modbus_Port"].toInt();

    }
    else
    {
        qDebug() << "SQL error:" << query.lastError().text();
    }
}

Server::~Server()
{
    State::ThreadRun = false;
    m_databasePool.closeAll();
}

Server *Server::GetPtr()
{
    if (m_server == nullptr)
    {
        std::unique_lock<std::mutex> lock(mtx);
        if (m_server == nullptr)
            m_server = new Server();
    }
    return m_server;
}

void Server::DeletePtr()
{

    if (m_server != nullptr)
    {
        delete m_server;
        m_server = nullptr;
    }
}

void Server::Start()
{
    m_camera.OpenRGBCamera();   // 打开相机并开始取帧

    m_imgProcess.LoadModel(m_databasePool.getConnectionAutoPtr());  // 优先加载模型，包含品类、模式初始化

    PlcClient::GetPlcPtr()->Open(); // 连接PLC
    ModbusTcpSlave::GetPtr()->Open(State::ModbusInfo::Port);   // 设置Modbus

    State::ThreadRun = true;
    m_imgProcess.Start();

    std::thread timerRun(&Server::Timer_1s, this);
    timerRun.detach();
}


void Server::SetReadImagePath(std::string path)
{
    readImgPath = path;
    m_imgList.clear();
    readIndex = 0;

    // 用 QDir 而不是 cv::glob：
    //   - cv::glob 在 Windows 下走窄字符 API，中文路径会枚举不到
    //   - 原来的 "/*.jpg" 只认 jpg，现场相机存的多半是 bmp/tif
    QDir dir(QString::fromStdString(path));
    QStringList filters;
    filters << "*.jpg" << "*.jpeg" << "*.png" << "*.bmp" << "*.tif" << "*.tiff";
    const QFileInfoList files = dir.entryInfoList(filters, QDir::Files, QDir::Name);
    for (const QFileInfo& fi : files)
    {
        m_imgList.push_back(fi.absoluteFilePath().toStdString());
    }
    qDebug() << "本地图片目录:" << QString::fromStdString(path)
             << "共" << static_cast<int>(m_imgList.size()) << "张";
}

/// 读取一张本地图片（QFile + imdecode，中文路径可用）
cv::Mat Server::ReadLocalImage(const std::string& path)
{
    cv::Mat img;
    QFile file(QString::fromStdString(path));
    if (!file.open(QIODevice::ReadOnly))
    {
        qDebug() << "打开图片失败:" << QString::fromStdString(path);
        return img;
    }
    const QByteArray data = file.readAll();
    file.close();

    std::vector<uchar> buf(data.begin(), data.end());
    if (!buf.empty())
    {
        img = cv::imdecode(buf, cv::IMREAD_COLOR);
    }
    if (img.empty())
    {
        qDebug() << "图片解码失败:" << QString::fromStdString(path);
    }
    return img;
}

/// 设置读取图像
void Server::SetReadImage(bool read)
{
    readImg = read;
    m_imgProcess.SetReadImage(readImg);
    qDebug() << "本地图片模式:" << (readImg ? "开" : "关")
             << "当前共" << static_cast<int>(m_imgList.size()) << "张";
}

/// 上一张
void Server::UpImage()
{
    if (m_imgList.empty())
    {
        qDebug() << "本地图片列表为空，请先设置图片路径";
        return;
    }

    // readIndex 指向「下一张要播的」，当前显示的是 readIndex-1。
    // 原实现直接 at(readIndex-1)，在 readIndex=0 时会越界崩溃。
    const int cur = readIndex - 1;
    const int target = cur > 0 ? cur - 1 : 0;
    cv::Mat img = ReadLocalImage(m_imgList.at(target));
    if (!img.empty())
    {
        State::frameQueue.push(img);
    }
    readIndex = target + 1;
    qDebug() << "上一张:" << QString::fromStdString(m_imgList.at(target));
}

/// 下一张
void Server::DownImage()
{
    if (m_imgList.empty())
    {
        qDebug() << "本地图片列表为空，请先设置图片路径";
        return;
    }
    if (readIndex >= static_cast<int>(m_imgList.size()))
    {
        qDebug() << "已经是最后一张";
        return;
    }

    cv::Mat img = ReadLocalImage(m_imgList.at(readIndex));
    if (!img.empty())
    {
        State::frameQueue.push(img);
    }
    readIndex++;
    qDebug() << "下一张:" << QString::fromStdString(m_imgList.at(readIndex - 1))
             << "(" << readIndex << "/" << static_cast<int>(m_imgList.size()) << ")";
}


std::shared_ptr<CountCollector> Server::getCollectorPtr()
{
    return collector;
}


std::shared_ptr<QSqlDatabase> Server::getDatabasePtr()
{
    return m_databasePool.getConnectionAutoPtr();
}

long Server::getCarEncoder(int car)
{
    return m_imgProcess.GetEncoder(car);
}

bool Server::setPortBlow(int port, int point, int blowTime)
{
    return m_imgProcess.SetPortBlow(port, point, blowTime);
}

void Server::Timer_1s()
{
    while(State::ThreadRun)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        // 缓存数量数据
        collector->catchCount(getDatabasePtr());

    }
}
