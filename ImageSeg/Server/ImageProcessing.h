#ifndef IMAGEPROCESSING_H
#define IMAGEPROCESSING_H

//#include <QThread>
#include <atomic>
#include <vector>
#include <string>
#include <QSqlDatabase>
#include "Yolo/Yolov8SegOnnx.h"
#include "Yolo/Yolov8ClsOnnx.h"
#include "Tools/CircularQueue.h"


class ImageProcessing
{
public:
    ImageProcessing();
    ~ImageProcessing();

    /// 加载模型
    void LoadModel(std::shared_ptr<QSqlDatabase> db);

    /// 开始处理任务
    void Start();

    /// 设置本地读取图像
    void SetReadImage(bool isRead);

    ///
    /// \brief 获取控制卡编码器值
    /// \param 控制卡编号
    /// \return 编码器值
    ///
    long GetEncoder(int car);

    ///
    /// \brief 设置单气阀吹气（触发单点运动控制）
    /// \param port 气阀编号（通道编号）
    /// \param point    触发位置（触发编码器值）
    /// \param blowTime     触发时长（吹气时长）
    /// \return     是否设置成功
    ///
    bool SetPortBlow(int port, int point, int blowTime);
private:

    /// 大图分割
    void ImageSegWorker();

    /// 小图分类
    void ImageClsWorker();

    /// 定位吹气
    void ImageRangeWorker();

    /// 图像保存
    void SaveImage();

    /// 物体重复判断
    bool Estimate(std::vector<OutputParams>& vecInput, OutputParams& input);



private:
    // 图像处理线程
    std::thread imageSegThread;
    std::thread imageClsThread;
    std::thread setRangeThread;
    std::thread::native_handle_type imageSegThreadId;
    std::thread::native_handle_type imageClsThreadId;
    std::thread::native_handle_type setRangeThreadId;

    //std::atomic<bool> readImg = true;

    // 帧计数器
    //std::atomic<unsigned long long> frameCounter;

private:
    // 分割模型
    std::unique_ptr<Yolov8SegOnnx> m_seg;

    // 分类模型
    std::vector<std::unique_ptr<Yolov8ClsOnnx>> m_clsList;

    // 分类选中品类
    std::vector<std::map<std::string, int>> m_typeBlows;
    //std::atomic<bool> m_threadRun = true;

    // 处理帧计数器
    std::atomic<unsigned long long> m_frameCounter;

    // 上一帧大图
    cv::Mat oldImg;

    // 大图分割结果队列
    CircularQueue<YoloSegResult> m_resultYolo;

    // 小图分类结果队列
    CircularQueue<std::pair<unsigned long long, OutputParams>> m_clsResult;

    // 双通道分界线
    int m_channelBoundary = 2048;

    // 是否本地读取图像
    bool IsReadImg = false;
};

#endif // IMAGEPROCESSING_H
