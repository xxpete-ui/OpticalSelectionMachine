#ifndef STATE_H
#define STATE_H

#include <string>
#include <atomic>
//#include <queue>
#include <vector>
#include <map>
#include <QDateTime>
#include <opencv2/opencv.hpp>
#include "Tools/CircularQueue.h"

class ShowImage
{
public:
    cv::Mat BigImage;
    std::vector<std::pair<std::string, cv::Mat>> SmallImages;
};


class State
{
public:
    // 线程运行
    static std::atomic<bool> ThreadRun;

    // 弹窗消息
    static CircularQueue<std::string> showMessage;

    // 待处理图像循环队列
    static CircularQueue<cv::Mat> frameQueue;

    // 新增：帧计数器
    static std::atomic<unsigned long long> frameCounter;
    //static unsigned long long frameCounter;

    // 待显示图像队列
    static CircularQueue<ShowImage> showImgQueue;

    /// 图像处理模块准备就绪
    static std::atomic<bool> imgProcessInitSuccess;

    /// 当前处理品类数量
    static std::map<std::string, std::atomic<int>> typeCounts;

    /// 吹气数量
    static std::map<std::string, std::atomic<int>> blowCounts;

    /// 品类modbus位置
    static std::map<std::string, int> typePoint;

    ///  当前订单运行
    static std::atomic<bool> OrderRun;

    /// 本次运行启动时间
    static QDateTime RunTime;

    static QString getRunningTime(const QDateTime& startTime);

public:
    // 相机参数
    class CameraInfo
    {
    public:
        // 相机线率比
        static float LineRateRatio;

        // 相机ip
        static std::string IpAddress;

        // 设置图像颜色通道格式
        static std::atomic<bool> SetImgBGR;

        // 设置相机内触发
        static std::atomic<bool> TriggerSource;

        // 内触发取帧间隔
        static std::atomic<int> TriggerTimer;

        // 相机高度比例
        static float HightProportion;


    };

    // 模型参数
    class ModelInfo
    {
    public:
        // 当前分选模式
        static int Mode;

        // 当前通道数
        static int Channel;

        // 多通道分割线
        static int ChannelBoundary;

        // 分割模型文件夹
        static std::string SegModelPath;

        // 分类模型文件夹
        static std::string ClsModelPath;

        // 使用GPU
        static bool UseGPU;
    };

    /// 瓶子控制
    class BottleControl
    {
    public:

        /// 尺寸限制
        static bool SizeLimit;

        /// 最小宽度
        static int MinWidth;
        /// 最小高度
        static int MinHeight;
        /// 最大宽度
        static int MaxWidth;
        /// 最大高度
        static int MaxHeight;
        /// 分割阈值
        static float SegThreshold;
        /// 分类阈值
        static float ClsThreshold;

        /// 吹气区域开始位置
        static int BlowAreaStart;

        /// 吹气区域长度
        static int BlowAreaHeight;

        /// 吹气区域结束位置
        static int BlowAreaEnd;

        /// 重复判断
        static bool RepeatJudgment;

        /// 使用高斯滤波
        static bool UseGaussianBlur;
    };


    class BlowsSet
    {
    public:
        // 是否启用气阀吹气
        static std::atomic<bool> IsBlow;

        // 吹气起始位置-X坐标（图像左侧第一个气孔位置）
        static float BlowStartX;

        // 吹气孔数量
        static int PoreCount;

        // 气孔间隔
        static float PoreInterval;

        // 物体起始吹气位置比例（物体吹气点在物体高度上的比例）
        static float ZoomRatio;

        // 吹气偏移（Y坐标方向）
        static int BlowOffset;

        // 吹气时长
        static int BlowTime;

        // 控制卡数量
        static int MotionCarCount;
    };


    class PLCInfo
    {
    public:
        // PLC IP地址
        static std::string IPAddress;

        // 端口号
        static std::atomic<int> Port;

        // 是否已连接
        static std::atomic<bool> IsOpen;

        // 自动模式
        static std::atomic<bool> AutoRun;

        // 传送带速率
        static std::atomic<int> Speed;

        // 传送带状态
        static std::atomic<bool> SpeedState;

        // 光源状态
        static std::atomic<bool> LightSourceState;

        // 手动操作传送带运行
        static std::atomic<bool> ManualSpeedRun;

        // 手动操作相机光源运行
        static std::atomic<bool> ManualLightSource;

        // 急停
        static std::atomic<bool> EmergencyStop;

        // 空气压力不足
        static std::atomic<bool> LackOfAirPressure;

        // 输送带变频器故障
        static std::atomic<bool> FrequencyConverterFailure;

        // 阀门气缸未就位
        static std::atomic<bool> AirValvePositionError;

        // 后方设备异常
        static std::atomic<bool> RearEquipmentAbnormality;

        // 全设备急停
        static std::atomic<bool> AllEmergencyStops;

        // 控制柜暂停运行
        static std::atomic<bool> ControlCabinetStop;


    };


    class ModbusInfo
    {
    public:
        // PLC IP地址
        static std::string IPAddress;

        // 端口号
        static std::atomic<int> Port;

        // 是否已连接
        static std::atomic<bool> IsOpen;

    };

};




#endif // STATE_H
