
#ifdef _WIN32
#include <Windows.h>
#include <direct.h>
#include <io.h>
#endif

#include <string>
#include <vector>
#include <cstdio>
#include <opencv2/opencv.hpp>
#include "onnxruntime_cxx_api.h"

//#include <cuda_fp16.h>

#ifndef YOLOV8CLSONNX_H
#define YOLOV8CLSONNX_H


enum MODEL_TYPE
{
    //FLOAT32 MODEL
    YOLO_DETECT_V8 = 1,
    YOLO_POSE = 2,
    YOLO_CLS = 3,

    //FLOAT16 MODEL
    YOLO_DETECT_V8_HALF = 4,
    YOLO_POSE_V8_HALF = 5,
};


typedef struct _DL_INIT_PARAM
{
    std::string modelPath;
    MODEL_TYPE modelType = YOLO_CLS;
    std::vector<int> imgSize = { 224, 224 };
    float rectConfidenceThreshold = 0.3;
    float iouThreshold = 0.5;
    int	keyPointsNum = 2;   //Note:kpt number for pose
    bool cudaEnable = true;
    int logSeverityLevel = 3;
    int intraOpNumThreads = 1;
} DL_INIT_PARAM;


typedef struct _DL_RESULT
{
    int classId;
    float confidence;
    std::string className;
    cv::Rect box;
    std::vector<cv::Point2f> keyPoints;
} DL_RESULT;


class Yolov8ClsOnnx
{
public:
    Yolov8ClsOnnx();

    ~Yolov8ClsOnnx();

public:
    void BlobFromImage(cv::Mat& iImg);

    void CreateSession(DL_INIT_PARAM& iParams);

    void RunSession(cv::Mat& iImg, std::vector<DL_RESULT>& oResult);

    void WarmUpSession();

    //template<typename N>
    /*void TensorProcess(std::chrono::steady_clock::time_point& starttime_1,
        cv::Mat& iImg,
        std::vector<int64_t>& inputNodeDims,
        std::vector<DL_RESULT>& oResult);*/

    void TensorProcess(cv::Mat& iImg,
                       std::vector<int64_t>& inputNodeDims,
                       std::vector<DL_RESULT>& oResult);

    void PreProcess(cv::Mat& iImg, std::vector<int> iImgSize, cv::Mat& oImg);

    std::vector<std::string> classes {};

private:
    Ort::Env env;
    Ort::Session* session;
    bool cudaEnable;
    Ort::RunOptions options;
    Ort::MemoryInfo _OrtMemoryInfo;
    std::vector<const char*> inputNodeNames;
    std::vector<const char*> outputNodeNames;

    MODEL_TYPE modelType;
    std::vector<int> imgSize;
    float rectConfidenceThreshold;
    float iouThreshold;
    float resizeScales;//letterbox scale

    std::vector<float> _blob;
};

#endif // YOLOV8CLSONNX_H
