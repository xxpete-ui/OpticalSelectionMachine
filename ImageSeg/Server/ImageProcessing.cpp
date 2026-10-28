#include "ImageProcessing.h"

#include <QSqlQuery>
#include <QSqlError>
#include "State.h"
#include "MCDLL_NET.h"
#include "Database/Table/TableInfo.h"

using namespace std;
using namespace cv;
using namespace DatabaseTable;

ImageProcessing::ImageProcessing()
{
    oldImg = Mat(512, 2048, CV_8UC3, cv::Scalar(0, 0, 0));
    m_channelBoundary = 2048;
}

ImageProcessing::~ImageProcessing()
{
    if(imageSegThread.joinable())   imageSegThread.join();
    if(imageClsThread.joinable())   imageClsThread.join();
    if(setRangeThread.joinable())   setRangeThread.join();
}


/// 加载模型
void ImageProcessing::LoadModel(std::shared_ptr<QSqlDatabase> db)
{
    // 获取配置的分类模型
    std::vector<std::string> clsNames;
    vector<vector<string>> clsTypeVec;
    std::string segName = "test.onnx";
    // 此处根据当前模式获取模型配置信息
    QSqlQuery query(*db);
    ModeInfo info;
    QString modeSql = QString("select * from ModeInfo where MainId = %1;").arg(State::ModelInfo::Mode);
    if (query.exec(modeSql) && query.next()) {

        info.MainId = query.value("MainId").toInt();
        info.Channel = query.value("Channel").toInt();
        info.ChannelBoundary = query.value("ChannelBoundary").toInt();
        info.ModeText = query.value("ModeText").toString();
        info.SegModel = query.value("SegModel").toString();
        // 假设 ClsModels 存储为逗号分隔的字符串，需要解析
        QString clsModelsStr = query.value("ClsModels").toString();
        if (!clsModelsStr.isEmpty()) {
            info.ClsModels = clsModelsStr.split(",", Qt::SkipEmptyParts);
        }

        // 查询子表
        QSqlQuery subQuery(*db);
        QString subSql = QString("SELECT * FROM ModeDetail WHERE MainId = %1;").arg(info.MainId);
        if (subQuery.exec(subSql)) {
            while (subQuery.next()) {
                ModeDetail detail;
                detail.SubId = subQuery.value("SubId").toInt();
                detail.MainId = subQuery.value("MainId").toInt();
                detail.ClsModel = subQuery.value("ClsModel").toString();
                detail.TypeName = subQuery.value("TypeName").toString();
                detail.TypeBlow = subQuery.value("TypeBlow").toInt();
                detail.SortId = subQuery.value("SortId").toInt();
                info.ClsTypes[detail.ClsModel].append(detail);
            }
        }

        State::ModelInfo::Channel = info.Channel;
        State::ModelInfo::ChannelBoundary = info.ChannelBoundary;
        segName = info.SegModel.toStdString();

        for(int i = 0; i < State::ModelInfo::Channel; i++)
        {
            std::string clsName = info.ClsModels[i].toStdString();   // 获取模型名（数据库）
            clsNames.push_back(clsName);

            if(subQuery.isActive())
            {
                vector<string> clsTypes;    // 通过配置文件路径获取模型品类（数据库）
                map<string, int> typeBlow;  // 获取品类吹气设置（数据库）

                for(auto item : info.ClsTypes[info.ClsModels[i]])
                {
                    clsTypes.push_back(item.TypeName.toStdString());
                    typeBlow[item.TypeName.toStdString()] = item.TypeBlow;
                    State::typePoint[item.TypeName.toStdString()] = item.SortId;
                }
                clsTypeVec.push_back(clsTypes);
                m_typeBlows.push_back(typeBlow);
            }
        }
    }
    else
    {
        qDebug() << "SQL error:" << query.lastError().text();
    }

    // 此处加载分割模型路径
    std::string segPath = State::ModelInfo::SegModelPath + "/" + segName;

    // 加载模型
    if(m_seg != nullptr)
    {
        m_seg.reset();
    }
    m_seg = std::make_unique<Yolov8SegOnnx>();
    m_seg->ReadModel(segPath, State::ModelInfo::UseGPU);
    m_clsList.clear();
    for(int i = 0; i < clsNames.size(); i++)
    {
        //Yolov8ClsOnnx cls;
        auto cls = std::make_unique<Yolov8ClsOnnx>();
        DL_INIT_PARAM param;
        param.modelPath = State::ModelInfo::ClsModelPath + "/" + clsNames[i];
        param.cudaEnable = State::ModelInfo::UseGPU;
        cls->CreateSession(param);
        cls->classes = clsTypeVec[i];
        m_clsList.push_back(std::move(cls));

    }

    // 设置双通道分界线
    if(State::ModelInfo::Channel > 1)
    {
        m_channelBoundary = info.ChannelBoundary;
    }

}


/// 开始处理任务
void ImageProcessing::Start()
{

    imageSegThread = std::thread(&ImageProcessing::ImageSegWorker, this);
    imageClsThread = std::thread(&ImageProcessing::ImageClsWorker, this);
    setRangeThread = std::thread(&ImageProcessing::ImageRangeWorker, this);

    imageSegThreadId = imageSegThread.native_handle();
    SetThreadPriority(imageSegThreadId, THREAD_PRIORITY_HIGHEST);	//高于优先级 2
    imageSegThread.detach();
    imageClsThreadId = imageClsThread.native_handle();
    SetThreadPriority(imageClsThreadId, THREAD_PRIORITY_HIGHEST);
    imageClsThread.detach();
    setRangeThreadId = setRangeThread.native_handle();
    SetThreadPriority(setRangeThreadId, THREAD_PRIORITY_ABOVE_NORMAL);
    setRangeThread.detach();
}

/// 设置本地读取图像
void ImageProcessing::SetReadImage(bool isRead)
{
    IsReadImg = isRead;
}

long ImageProcessing::GetEncoder(int car)
{
    long code = 0;
    short ret = MCF_Get_Encoder_Net(0, &code, car);
    return code;
}

bool ImageProcessing::SetPortBlow(int port, int point, int blowTime)
{
    int car = 0;
    if(port >= 48 && State::BlowsSet::MotionCarCount > 1)
    {
        car = port / 48;
        port = port % 48;
    }

    int ret = MCF_Add_Compare_Point_Net(port, point, 1, blowTime, port, car);
    return ret == 0;
}

/// 大图分割
void ImageProcessing::ImageSegWorker()
{
    while(State::ThreadRun)
    {
        optional<Mat> newImg = State::frameQueue.pop();
        if(!newImg.has_value() || newImg->empty())
        {
            continue;
        }

        cv::Mat image;
        if(IsReadImg)
        {
            image = newImg.value().clone();
        }
        else
        {
            cv::vconcat(oldImg, newImg.value(), image);
        }



        vector<OutputParams> resultYolo;
        if(m_seg != nullptr && m_seg->OnnxDetect(image, resultYolo))
        {

            int hasobjectCount = 0;

            for (int i = 0; i < resultYolo.size(); i++)
            {
                if (resultYolo[i].confidence > State::BottleControl::SegThreshold)
                {
                    if (resultYolo[i].box.width < State::BottleControl::MinWidth || resultYolo[i].box.height < State::BottleControl::MinHeight)
                        continue;
                    hasobjectCount++;
                    break;
                }
            }
            std::sort(resultYolo.begin(), resultYolo.end(), [&](OutputParams r1, OutputParams r2) {return r1.box.y < r2.box.y; });

            //std::unique_lock<std::shared_mutex> lock(srd_resultMtx);
            m_resultYolo.push(YoloSegResult(m_frameCounter, hasobjectCount > 0, image, resultYolo));
        }
        oldImg = newImg->clone();
        m_frameCounter++;
    }
}


/// 小图分类
void ImageProcessing::ImageClsWorker()
{
    std::vector<OutputParams> dropRepetition;
    while(State::ThreadRun)
    {
        optional<YoloSegResult> result = m_resultYolo.pop();
        if (!result.has_value())
        {
            continue;
        }
        Mat image = result.value().img;
        Mat mask = image.clone();

        // 记录本轮选中的物体信息
        std::vector<OutputParams> drop;

        ShowImage showImage;
        // 循环处理小图
        for(OutputParams item : result->resultYolo)
        {
            // 设置蒙版图
            if (item.boxMask.rows > 0 && item.boxMask.cols > 0)
            {
                mask(item.box).setTo(cv::Scalar(0, 255, 0), item.boxMask);
            }
            else
            {
                continue;
            }

            // 获取物体高度和宽度
            // int  grabHeight = 0;
            // int  grabWidth = 0;
            // if (item.rotatedBox.size.width <= item.rotatedBox.size.height)
            // {
            //     grabHeight = item.rotatedBox.size.height;
            //     grabWidth = item.rotatedBox.size.width;
            // }
            // else//宽度>高度
            // {
            //     grabHeight = item.rotatedBox.size.width;
            //     grabWidth = item.rotatedBox.size.height;
            // }

            // 获取物体顶点坐标
            cv::Point2f P[4];
            item.rotatedBox.points(P);//得到矩形的四个点坐标
            vector <cv::Point> pts;
            pts.push_back(cv::Point(P[0].x, P[0].y));
            pts.push_back(cv::Point(P[1].x, P[1].y));
            pts.push_back(cv::Point(P[2].x, P[2].y));
            pts.push_back(cv::Point(P[3].x, P[3].y));

            // 获取小图图像
            cv::Mat boudingGrayRect = cv::Mat::zeros(image.size(), CV_8UC3);
            cv::fillConvexPoly(boudingGrayRect, pts, cv::Scalar(255, 255, 255), 8);
            cv::bitwise_and(boudingGrayRect, image, boudingGrayRect);
            Mat roiYolo = boudingGrayRect(item.bigBox).clone();

            // 物体尺寸限制
            if ((roiYolo.cols * roiYolo.rows <= 0)
                || (roiYolo.cols < State::BottleControl::MinWidth && roiYolo.rows < State::BottleControl::MinHeight)
                || (item.confidence < State::BottleControl::ClsThreshold)
                || (item.box.width > State::BottleControl::MaxWidth || item.box.height > State::BottleControl::MaxHeight)
                || (item.box.y + item.box.height < State::BottleControl::BlowAreaStart || item.box.y + item.box.height > State::BottleControl::BlowAreaEnd)
                )
            {
                continue;
            }
            // if (roiYolo.cols * roiYolo.rows <= 0)
            // {
            //     continue;
            // }
            // if (roiYolo.cols < State::BottleControl::MinWidth && roiYolo.rows < State::BottleControl::MinHeight)
            // {
            //     continue;
            // }
            // if (item.confidence < State::BottleControl::SegThreshold)
            // {
            //     continue;
            // }
            // if (item.box.width > State::BottleControl::MaxWidth || item.box.height > State::BottleControl::MaxHeight)
            // {
            //     qDebug() << "宽度：" << item.box.width << "(" << State::BottleControl::MaxWidth << ")"
            //              << "高度：" << item.box.height << "(" << State::BottleControl::MaxHeight << ")";
            //     continue;
            // }
            // if (item.box.y + item.box.height < State::BottleControl::BlowAreaStart || item.box.y + item.box.height > State::BottleControl::BlowAreaEnd)
            // {
            //     continue;
            // }

            // 重复判断
            if(State::BottleControl::RepeatJudgment && Estimate(dropRepetition, item))
            {
                continue;
            }

            // 获取处理前小图
            cv::Mat RotateROI = cv::Mat::zeros(224, 224, CV_8UC3);
            //cv::Mat maskImageResized = cv::Mat::zeros(224, 224, CV_8UC3);
            if (roiYolo.cols > 224 || roiYolo.rows > 224)
            {
                if (roiYolo.cols > roiYolo.rows)
                {
                    float xyScaleFator = 224 * 1.0f / roiYolo.cols;
                    cv::resize(roiYolo, roiYolo, cv::Size(0, 0), xyScaleFator, xyScaleFator, cv::INTER_LINEAR);
                }
                else
                {
                    float xyScaleFator = 224 * 1.0f / roiYolo.rows;
                    cv::resize(roiYolo, roiYolo, cv::Size(0, 0), xyScaleFator, xyScaleFator, cv::INTER_LINEAR);
                }
            }

            cv::Rect roiRect = cv::Rect(112 - roiYolo.cols / 2, 112 - roiYolo.rows / 2, roiYolo.cols, roiYolo.rows);
            // roiYolo.copyTo(maskImageResized(roiRect));
            // RotateROI = maskImageResized.clone();
            roiYolo.copyTo(RotateROI(roiRect));

            // 缩放后的处理前的小图
            cv::Mat ImageDst = RotateROI.clone();
            //cv::resize(RotateROI, ImageDst, cv::Size(224, 224), (0, 0), (0, 0), cv::INTER_LINEAR);

            // 高斯滤波
            if (State::BottleControl::UseGaussianBlur)
            {
                //cv::Mat RoiGauss = RotateROI.clone();
                cv::GaussianBlur(RotateROI, RotateROI, cv::Size(3, 3), 0);
                //RotateROI = RoiGauss.clone();
            }

            //每个瓶子分类识别
            std::vector<DL_RESULT> clsResult;
            map<string, int> typeBlows;
            if(item.box.x + item.box.width <= m_channelBoundary)
            {
                m_clsList[0]->RunSession(RotateROI, clsResult);
                typeBlows = m_typeBlows[0];
            }
            else
            {
                m_clsList[1]->RunSession(RotateROI, clsResult);
                typeBlows = m_typeBlows[1];
            }

            State::typeCounts[clsResult[0].className]++;


            // 判断是否选中和相似度阈值
            if(typeBlows.empty() || typeBlows[clsResult[0].className] != 1 || clsResult[0].confidence < State::BottleControl::ClsThreshold)
            {
                continue;
            }

            string clsStr = clsResult[0].className + "_" + std::to_string(clsResult[0].confidence);
            showImage.SmallImages.push_back(std::make_pair(clsStr, ImageDst));

            drop.push_back(item);

            // 加入待吹队列（本地回放图片时不触发吹气，避免去调用不存在的运动控制卡）
            if(State::BlowsSet::IsBlow && !IsReadImg)
            {
                m_clsResult.push(std::make_pair(result->processCount, item));
            }
            State::blowCounts[clsResult[0].className]++;

        }

        // 更新上轮选中物体信息
        dropRepetition.clear();
        dropRepetition = drop;

        // 大图上绘制选中物体
        showImage.BigImage = image.clone();
        for (int i = 0; i < result->resultYolo.size(); i++)
        {
            rectangle(showImage.BigImage, result->resultYolo[i].box, cv::Scalar(0, 255, 0), 4, 8);

            cv::putText(showImage.BigImage, std::to_string(i + 1),
                        cv::Point(result->resultYolo[i].box.x + result->resultYolo[i].box.width / 2 - 10, result->resultYolo[i].box.y + result->resultYolo[i].box.height / 2 + 10),
                        3, 2, cv::Scalar(0, 0, 255), 3, 8, false);
        }

        cv::line(showImage.BigImage, cv::Point(0, 512), cv::Point(2048, 512), cv::Scalar(0, 0, 255));

        State::showImgQueue.push(showImage);

    }
}


/// 物体吹气触发
void ImageProcessing::ImageRangeWorker()
{
    while(State::ThreadRun)
    {
        auto range = m_clsResult.pop();
        if(!range.has_value())
        {
            continue;
        }
        //物体大图起始位置
        unsigned long long position = ((range->first) * 512) * State::CameraInfo::HightProportion;

        // 物体位置
        position += range->second.box.y * State::CameraInfo::HightProportion;

        ///// 吹气调节 /////
        int blowExtend = 0;
        if (range->second.box.height > 100)
        {
            blowExtend = (range->second.box.height - 100) / 50; //瓶子长度每多 50 , 吹气时长 + 1
            blowExtend = (blowExtend <= 4) ? blowExtend : 6;
        }

        // 吹气开始位置 = 大图实际位置 + （物体图像中心 - 物体图像吹气起始位置） * 相机高度比例
        int centerPost = (range->second.box.y + range->second.box.height) / 2;
        unsigned long long startPost = position + (centerPost - (range->second.box.height * State::BlowsSet::ZoomRatio) / 2) * State::CameraInfo::HightProportion;

        // 气孔（物体范围下最左侧和最右侧气孔）
        int startPort = (range->second.box.x - State::BlowsSet::BlowStartX) / State::BlowsSet::PoreInterval + 1;
        int endPort = (range->second.box.x + range->second.box.width - State::BlowsSet::BlowStartX) / State::BlowsSet::PoreInterval;

        for(int i = startPort; i < endPort; i++)
        {
            int car = 0;
            int port = i;
            if(port >= 48 && State::BlowsSet::MotionCarCount > 1)
            {
                car = port / 48;
                port = port % 48;
            }

            MCF_Add_Compare_Point_Net(port, startPost + State::BlowsSet::BlowOffset, 1, State::BlowsSet::BlowTime + blowExtend, port, car);
        }
    }
}


/// 物体重复判断
bool ImageProcessing::Estimate(std::vector<OutputParams>& vecInput, OutputParams& input)
{
    for (int i = 0; i < vecInput.size(); i++)
    {
        if (std::abs(vecInput[i].box.x - input.box.x) <= 10
            //&& std::abs(vecInput[i].box.y - 512 - input.box.y) <= 10 + State::BottleControl::BlowAreaHeight - 512
            && std::abs(vecInput[i].box.y - 512 - input.box.y) <= 10
            && std::abs(vecInput[i].box.width - input.box.width) <= 10)
        {
            return true;

            //// 精准控制
            //if (std::abs(std::abs(vecInput[i].rotatedBox.angle) - std::abs(input.rotatedBox.angle)) <= 3)	//旋转角度相似 <= 3
            //{
            //	return true;
            //}
            //else if (vecInput[i].box.y + vecInput[i].box.height >= 1023 && input.box.y + input.box.height > 522)	//超出半张范围较大
            //{
            //	return true;
            //}
            //else
            //{
            //	/*double area1 = vecInput[i].box.area();
            //	double area2 = input.box.area();
            //	double difference = std::abs(area1 - area2);
            //	double averageArea = (area1 + area2) / 2.0;
            //	double data = (difference / averageArea) * 100.0;*/

            //	double area1 = vecInput[i].rotatedBox.size.area();
            //	double area2 = input.rotatedBox.size.area();
            //	double difference = std::abs(area1 - area2);
            //	double data = difference / (max(area1, area2)) * 100.0;		//计算旋转矩形面积相差百分比

            //	//std::wstring wstr;
            //	//wstr = L"------------> x, y 对比通过，面积对比相差 " + std::to_wstring(data) + L"%";
            //	////strProTime3T00.Format(L"------------> x, y 对比通过，面积对比相差 %s \%", std::to_string(data));
            //	//WriteTrace(wstr.c_str());

            //	if (data < 5)
            //	{
            //		return true;
            //	}
            //}



            /*if ((vecInput[i].box.x - input.box.x <= 3 && vecInput[i].box.x - input.box.x >= -3)
                && (vecInput[i].confidence - input.confidence <= 0.05 && vecInput[i].confidence - input.confidence >= -0.05)
                )
            {

            }*/
        }
    }

    return false;
}