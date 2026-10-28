#include "Yolov8ClsOnnx.h"

#include <regex>
#include <chrono>
#include <iostream>

#define benchmark
#define min(a,b)            (((a) < (b)) ? (a) : (b))

void GetRunTime(std::string name, double starttime_1, double starttime_2, bool cudaEnable)
{
    double all_time = (double)(starttime_2 - starttime_1) / CLOCKS_PER_SEC * 1000;
    name = " ms " + name;
    if (cudaEnable)
    {
        std::cout << "[YOLO_V8(CUDA)]: " << all_time << name << std::endl;
    }
    else
    {
        std::cout << "[YOLO_V8(CPU)]: " << all_time << name << std::endl;
    }
}

void GetRunTime(std::string name, std::chrono::steady_clock::time_point starttime_1, std::chrono::steady_clock::time_point starttime_2, bool cudaEnable)
{
    auto all_time = std::chrono::duration_cast<std::chrono::microseconds>(starttime_2 - starttime_1);
    name = " ms " + name;
    if (cudaEnable)
    {
        std::cout << "[YOLO_V8(CUDA)]: " << (double)all_time.count() / 1000.f << name << std::endl;
    }
    else
    {
        std::cout << "[YOLO_V8(CPU)]: " << (double)all_time.count() / 1000.f << name << std::endl;
    }
}


Yolov8ClsOnnx::Yolov8ClsOnnx()
    :_OrtMemoryInfo(Ort::MemoryInfo::CreateCpu(OrtAllocatorType::OrtDeviceAllocator, OrtMemType::OrtMemTypeCPUOutput))
{

}


Yolov8ClsOnnx::~Yolov8ClsOnnx() {
    delete session;

    for (auto& name : inputNodeNames) delete[] name;
    for (auto& name : outputNodeNames) delete[] name;
    inputNodeNames.clear();
    outputNodeNames.clear();
}

#ifdef USE_CUDA
namespace Ort
{
template<>
struct TypeToTensorType<half> { static constexpr ONNXTensorElementDataType type = ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16; };
}
#endif



void Yolov8ClsOnnx::BlobFromImage(cv::Mat& iImg)
{
    int Rows = iImg.rows;
    int Cols = iImg.cols;
    int imgHW = Rows * Cols;

    for (int r = 0; r < Rows; r++)
    {
        int index = r * Rows;
        int index1 = imgHW + r * Rows;
        int index2 = 2 * imgHW + r * Rows;
        cv::Vec3b* rptr = iImg.ptr<cv::Vec3b>(r);
        for (int c = 0; c < Cols; c++)
        {
            _blob[index + c] = (rptr[c][0] == NULL ? 0 : rptr[c][0]) / 255.0f;
            _blob[index1 + c] = (rptr[c][1] == NULL ? 0 : rptr[c][1]) / 255.0f;
            _blob[index2 + c] = (rptr[c][2] == NULL ? 0 : rptr[c][2]) / 255.0f;

        }
    }

}



void Yolov8ClsOnnx::PreProcess(cv::Mat& iImg, std::vector<int> iImgSize, cv::Mat& oImg)
{
    if (iImg.channels() == 3)
    {
        oImg = iImg.clone();
        cv::cvtColor(oImg, oImg, cv::COLOR_BGR2RGB);
    }
    else
    {
        cv::cvtColor(iImg, oImg, cv::COLOR_GRAY2RGB);
    }

    if (oImg.size[0] != iImgSize[0] || oImg.size[1] != iImgSize[1])
    {
        int h = iImg.rows;
        int w = iImg.cols;
        int m = min(h, w);
        int top = (h - m) / 2;
        int left = (w - m) / 2;
        cv::resize(oImg(cv::Rect(left, top, m, m)), oImg, cv::Size(iImgSize.at(0), iImgSize.at(1)));
    }

}


void Yolov8ClsOnnx::CreateSession(DL_INIT_PARAM& iParams) {
    //char* Ret = RET_OK;
    std::regex pattern("[\u4e00-\u9fa5]");
    bool result = std::regex_search(iParams.modelPath, pattern);
    if (result)
    {
        std::cout << "[YOLO_V8]:Your model path is error.Change your model path without chinese characters." << std::endl;
        //Log::GetPtr()->Error("[YOLO_V8]:Your model path is error.Change your model path without chinese characters.");
        return;
    }
    try
    {
        rectConfidenceThreshold = iParams.rectConfidenceThreshold;
        iouThreshold = iParams.iouThreshold;
        imgSize = iParams.imgSize;
        modelType = iParams.modelType;
        env = Ort::Env(ORT_LOGGING_LEVEL_WARNING, "Yolo");
        Ort::SessionOptions sessionOption;
        if (iParams.cudaEnable)
        {
            cudaEnable = iParams.cudaEnable;
            OrtStatus* status = OrtSessionOptionsAppendExecutionProvider_CUDA(sessionOption, 0);
        }
        sessionOption.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        sessionOption.SetIntraOpNumThreads(iParams.intraOpNumThreads);
        sessionOption.SetLogSeverityLevel(iParams.logSeverityLevel);

#ifdef _WIN32
        int ModelPathSize = MultiByteToWideChar(CP_UTF8, 0, iParams.modelPath.c_str(), static_cast<int>(iParams.modelPath.length()), nullptr, 0);
        wchar_t* wide_cstr = new wchar_t[ModelPathSize + 1];
        MultiByteToWideChar(CP_UTF8, 0, iParams.modelPath.c_str(), static_cast<int>(iParams.modelPath.length()), wide_cstr, ModelPathSize);
        wide_cstr[ModelPathSize] = L'\0';
        const wchar_t* modelPath = wide_cstr;
#else
        const char* modelPath = iParams.modelPath.c_str();
#endif // _WIN32

        session = new Ort::Session(env, modelPath, sessionOption);
        Ort::AllocatorWithDefaultOptions allocator;
        size_t inputNodesNum = session->GetInputCount();
        for (size_t i = 0; i < inputNodesNum; i++)
        {
            Ort::AllocatedStringPtr input_node_name = session->GetInputNameAllocated(i, allocator);
            char* temp_buf = new char[50];
            strcpy(temp_buf, input_node_name.get());
            inputNodeNames.push_back(temp_buf);
        }
        size_t OutputNodesNum = session->GetOutputCount();
        for (size_t i = 0; i < OutputNodesNum; i++)
        {
            Ort::AllocatedStringPtr output_node_name = session->GetOutputNameAllocated(i, allocator);
            char* temp_buf = new char[10];
            strcpy(temp_buf, output_node_name.get());
            outputNodeNames.push_back(temp_buf);
        }
        options = Ort::RunOptions{ nullptr };
        WarmUpSession();
        return;
    }
    catch (const std::exception& e)
    {
        const char* str1 = "[YOLO_V8]:";
        const char* str2 = e.what();
        std::string result = std::string(str1) + std::string(str2);
        char* merged = new char[result.length() + 1];
        std::strcpy(merged, result.c_str());
        std::cout << merged << std::endl;
        delete[] merged;
        std::cout << "[YOLO_V8]:Create session failed." << std::endl;
        return;
    }

}


void Yolov8ClsOnnx::RunSession(cv::Mat& iImg, std::vector<DL_RESULT>& oResult) {
    //#ifdef benchmark
    //auto starttime_1 = std::chrono::high_resolution_clock::now();
    //#endif // benchmark

    cv::Mat processedImg;
    PreProcess(iImg, imgSize, processedImg);

    //auto starttime_2 = std::chrono::high_resolution_clock::now();
    BlobFromImage(processedImg);
    std::vector<int64_t> inputNodeDims = { 1, 3, imgSize.at(0), imgSize.at(1) };

    //auto starttime_3 = std::chrono::high_resolution_clock::now();
    //GetRunTime("Process_1 : PreProcess", starttime_1, starttime_2, cudaEnable);
    //GetRunTime("Process_2 : BlobFromImage", starttime_2, starttime_3, cudaEnable);

    TensorProcess( iImg, inputNodeDims, oResult);

    //_blob.clear();
}


//template<typename N>
//void Yolov8ClsOnnx::TensorProcess(std::chrono::steady_clock::time_point& starttime_1, cv::Mat& iImg, std::vector<int64_t>& inputNodeDims,
//	std::vector<DL_RESULT>& oResult)
//{
//	Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
//		_OrtMemoryInfo, _blob.data(), 3 * imgSize[0] * imgSize[1],
//		inputNodeDims.data(), inputNodeDims.size());
//#ifdef benchmark
//	//auto starttime_2 = std::chrono::high_resolution_clock::now();
//#endif // benchmark
//	auto outputTensor = session->Run(options, inputNodeNames.data(), &inputTensor, 1, outputNodeNames.data(),
//		outputNodeNames.size());
//#ifdef benchmark
//	//auto starttime_3 = std::chrono::high_resolution_clock::now();
//#endif // benchmark
//
//	//Ort::TypeInfo typeInfo = outputTensor.front().GetTypeInfo();
//
//	auto output = outputTensor.front().GetTensorMutableData<float>();
//	//delete[] blob;
//	//int size = sizeof(output) / sizeof(output[0]);
//
//	DL_RESULT result;
//	for (int i = 0; i < this->classes.size(); i++)
//	{
//		result.classId = i;
//		result.confidence = output[i];
//		result.className = classes[i];
//		oResult.push_back(result);
//	}
//	std::sort(oResult.begin(), oResult.end(),
//		[&](DL_RESULT r1, DL_RESULT r2)
//		{return r1.confidence > r2.confidence; });
//	/*auto starttime_4 = std::chrono::high_resolution_clock::now();
//
//	GetRunTime("Process_3 : session->Run", starttime_2, starttime_3, cudaEnable);
//	GetRunTime("Process_4 : select_time", starttime_3, starttime_4, cudaEnable);
//	GetRunTime("Process: all_time", starttime_1, starttime_4, cudaEnable);*/
//
//}

void Yolov8ClsOnnx::TensorProcess(cv::Mat& iImg, std::vector<int64_t>& inputNodeDims, std::vector<DL_RESULT>& oResult)
{
    Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
        _OrtMemoryInfo, _blob.data(), 3 * imgSize[0] * imgSize[1],
        inputNodeDims.data(), inputNodeDims.size());
#ifdef benchmark
    //auto starttime_2 = std::chrono::high_resolution_clock::now();
#endif // benchmark
    auto outputTensor = session->Run(options, inputNodeNames.data(), &inputTensor, 1, outputNodeNames.data(),
                                     outputNodeNames.size());
#ifdef benchmark
    //auto starttime_3 = std::chrono::high_resolution_clock::now();
#endif // benchmark

    //Ort::TypeInfo typeInfo = outputTensor.front().GetTypeInfo();

    auto output = outputTensor.front().GetTensorMutableData<float>();
    //delete[] blob;
    //int size = sizeof(output) / sizeof(output[0]);

    DL_RESULT result;
    for (int i = 0; i < this->classes.size(); i++)
    {
        result.classId = i;
        result.confidence = output[i];
        result.className = classes[i];
        oResult.push_back(result);
    }
    std::sort(oResult.begin(), oResult.end(),
              [&](DL_RESULT r1, DL_RESULT r2)
              {return r1.confidence > r2.confidence; });
    /*auto starttime_4 = std::chrono::high_resolution_clock::now();

    GetRunTime("Process_3 : session->Run", starttime_2, starttime_3, cudaEnable);
    GetRunTime("Process_4 : select_time", starttime_3, starttime_4, cudaEnable);
    GetRunTime("Process: all_time", starttime_1, starttime_4, cudaEnable);*/

}


void Yolov8ClsOnnx::WarmUpSession()
{
    //clock_t starttime_1 = clock();
    cv::Mat iImg = cv::Mat(cv::Size(imgSize.at(0), imgSize.at(1)), CV_8UC3);
    cv::Mat processedImg;
    PreProcess(iImg, imgSize, processedImg);

    _blob.resize(processedImg.total() * 3);
    BlobFromImage(processedImg);
    std::vector<int64_t> YOLO_input_node_dims = { 1, 3, imgSize.at(0), imgSize.at(1) };
    Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
        _OrtMemoryInfo, _blob.data(), 3 * imgSize.at(0) * imgSize.at(1),
        YOLO_input_node_dims.data(), YOLO_input_node_dims.size());
    auto output_tensors = session->Run(options, inputNodeNames.data(), &input_tensor, 1, outputNodeNames.data(),
                                       outputNodeNames.size());

    /*clock_t starttime_4 = clock();
    double post_process_time = (double)(starttime_4 - starttime_1) / CLOCKS_PER_SEC * 1000;
    if (cudaEnable)
    {
        std::cout << "[YOLO_V8(CUDA)]: " << "Cuda warm-up cost " << post_process_time << " ms. " << std::endl;
    }*/

}
