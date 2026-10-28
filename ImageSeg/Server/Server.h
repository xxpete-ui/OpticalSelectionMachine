#pragma once
#ifndef SERVER_H
#define SERVER_H

#include <QTimer>
#include <vector>
#include <string>
#include <memory>
#include <mutex>
#include "Camera.h"
#include "ImageProcessing.h"
#include "Database/DatabasePool.h"
#include "Collector/CountCollector.h"

class Server
{
private:
    Server();
    ~Server();

public:
    static Server* GetPtr();	//获取单例
    void DeletePtr();			//释放单例指针

    void Init();

    void Start();

    /// 设置图像读取路径
    void SetReadImagePath(std::string path);

    /// 当前图片列表数量
    int GetLocalImageCount() const { return static_cast<int>(m_imgList.size()); }

    /// 设置读取图像
    void SetReadImage(bool read);

    /// 上一张
    void UpImage();

    /// 下一张
    void DownImage();

    // 获取数量采集数据对象指针
    std::shared_ptr<CountCollector> getCollectorPtr();

    ///
    /// \brief 获取数据库连接
    /// \return
    ///
    std::shared_ptr<QSqlDatabase> getDatabasePtr();

    ///
    /// \brief 获取运动控制卡编码器值
    /// \param car  卡号
    /// \return
    ///
    long getCarEncoder(int car);

    ///
    /// \brief 设置单气阀吹气（触发单点运动控制）
    /// \param port 气阀编号（通道编号）
    /// \param point    触发位置（触发编码器值）
    /// \param blowTime     触发时长（吹气时长）
    /// \return     是否设置成功
    ///
    bool setPortBlow(int port, int point, int blowTime);

private:
    /// 后台线程运行，间隔30s
    void Timer_1s();

    /// 读取一张本地图片（走 QFile，避免 cv::imread 在中文路径下失败）
    static cv::Mat ReadLocalImage(const std::string& path);

private:
    ImageProcessing m_imgProcess;
    Camera m_camera;

    //QTimer readTimer;
    std::string readImgPath;
    std::vector<std::string> m_imgList;
    int readIndex = 0;
    bool readImg = false;

    DatabasePool m_databasePool;    // 数据库连接池

    std::shared_ptr<CountCollector> collector;   // 数量采集

    static Server* m_server;		//单例
    static std::mutex mtx;				//锁
};

#endif // SERVER_H
