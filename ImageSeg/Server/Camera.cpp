#include "Camera.h"
//#include "MvCameraControl.h"

//#include "MvGigEDevice.h"
//using namespace MvCamCtrl;
#include <qmessagebox.h>
#include <QDebug>
#include <opencv2/opencv.hpp>

#include "State.h"

Camera::Camera()
{


}


Camera::~Camera()
{
}
//相机
void* handle = NULL;
//MVCC_INTVALUE_EX struIntValue = { 0 };
/*unsigned int    m_nBufSizeForSaveImage*/;

//int nRet = MV_OK;

void vMirrorTrans(const cv::Mat &src, cv::Mat &dst)
{
	CV_Assert(src.depth() == CV_8U);
	dst.create(src.rows, src.cols, src.type());

	int rows = src.rows;

	for (int i = 0; i < rows; i++)
		src.row(rows - i - 1).copyTo(dst.row(i));
}


//int callbackcount = 0;
//uint64 countFrame = 0;
void __stdcall ImageCallBackEx(unsigned char * pData, MV_FRAME_OUT_INFO_EX* pFrameInfo, void* pUser)
{

    unsigned char*  m_pBufForSaveImage = NULL;
    unsigned int    m_nBufSizeForSaveImage;

    if (pFrameInfo)
    {
        printf("Get One Frame: Width[%d], Height[%d], nFrameNum[%d]\n",
               pFrameInfo->nWidth, pFrameInfo->nHeight, pFrameInfo->nFrameNum);
    }

    // 设置对应的相机参数
    if (NULL == m_pBufForSaveImage)
    {
        m_nBufSizeForSaveImage = pFrameInfo->nWidth * pFrameInfo->nHeight * 3 + 2048;
        m_pBufForSaveImage = (unsigned char*)malloc(m_nBufSizeForSaveImage);
    }

    // 数据转换
    MV_SAVE_IMAGE_PARAM_EX stParam = { 0 };
    stParam.enImageType = MV_Image_Bmp;
    stParam.enPixelType = PixelType_Gvsp_RGB8_Packed;
    stParam.nWidth = pFrameInfo->nWidth;
    stParam.nHeight = pFrameInfo->nHeight;
    stParam.nDataLen = 3145728;
    stParam.pData = pData;
    stParam.pImageBuffer = m_pBufForSaveImage;
    stParam.nBufferSize = m_nBufSizeForSaveImage;
    stParam.nJpgQuality = 90;

    MV_CC_SaveImageEx2(handle, &stParam);

    char *Image = new char[512 * 2048 * 3];
    char * pImage = (char *)m_pBufForSaveImage;
    memcpy(Image, pImage, 512 * 2048 * 3);

    cv::Mat ImageMat(512, 2048, CV_8UC3, Image);


    cv::Mat ImageDst; // 垂直镜像
    vMirrorTrans(ImageMat, ImageDst);

    // 关键修改：直接将每一帧存储到 State 的全局 vector 中
    State::frameQueue.push(ImageDst);
    State::frameCounter++;


    //callbackcount++;


    if (Image != NULL)
    {
        delete[] Image;
        Image = NULL;
    }
    if (m_pBufForSaveImage != NULL)
    {
        free(m_pBufForSaveImage);  // 注意：这里应该用 free，因为是用 malloc 分配的
        m_pBufForSaveImage = NULL;
    }
}

void __stdcall ImageCallBackEx_New(unsigned char * pData, MV_FRAME_OUT_INFO_EX* pFrameInfo, void* pUser)
{
    // 参数检查
    if (!pFrameInfo || !pData) {
        qDebug("Invalid frame data received");
        return;
    }

    // 验证图像尺寸
    const int expectedWidth = 2048;
    const int expectedHeight = 512;

    if (pFrameInfo->nWidth != expectedWidth || pFrameInfo->nHeight != expectedHeight) {
        char msg[256];
        snprintf(msg, sizeof(msg),
                 "Unexpected frame size: %dx%d, expected %dx%d",
                 pFrameInfo->nWidth, pFrameInfo->nHeight,
                 expectedWidth, expectedHeight);
        qDebug(msg);
        return;
    }

    try {
        // 直接使用相机数据创建OpenCV Mat（避免内存拷贝）
        // 相机数据是RGB格式，OpenCV默认BGR
        cv::Mat rawImage(expectedHeight, expectedWidth, CV_8UC3, pData);

        if(State::CameraInfo::SetImgBGR)
        {
            cv::cvtColor(rawImage, rawImage, cv::COLOR_RGB2BGR);    // 改为BGR
        }


        // 垂直镜像（创建新的Mat，因为原始数据是只读的）
        cv::Mat mirroredImage;
        cv::flip(rawImage, mirroredImage, 0);

        // 存储到State（State::addFrame内部会clone，确保线程安全）
        State::frameQueue.push(mirroredImage);

        // 更新计数器
        //callbackcount++;
        State::frameCounter++;

        // 定期输出状态信息（避免频繁打印影响性能）
//        if (callbackcount % 100 == 0) {
//            printf("Successfully processed %d frames\n", callbackcount);

//            // 可选：输出缓冲区状态
//            size_t bufferSize = State::getFrameCount();
//            printf("Frame buffer size: %zu\n", bufferSize);
//        }

    } catch (const cv::Exception& e) {
        char msg[256];
        snprintf(msg, sizeof(msg), "OpenCV exception in callback: %s", e.what());
        qDebug(msg);
    } catch (const std::exception& e) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Exception in image callback: %s", e.what());
        qDebug(msg);
    } catch (...) {
        qDebug("Unknown exception in image callback");
    }
}


float Camera::GetFrameRate()
{
	//获取float型参数
	MVCC_FLOATVALUE struFloatValue = { 0 };
	nRet = MV_CC_GetFloatValue(handle, "ResultingFrameRate", &struFloatValue);
	if (MV_OK != nRet)
	{
		return 0.0;
	}
	else
	{
		return struFloatValue.fCurValue;
	}
}
bool Camera::SetIntValue(char* key, unsigned int nValue)
{
	//设置int型参数
	int nRet = MV_CC_SetIntValue(handle, key, nValue);
	if (MV_OK != nRet)
    {
		return false;
	}
	return true;
}
bool Camera::SetCommandValue(const char* key)
{
	//设置Command型节点
	int nRet = MV_CC_SetCommandValue(handle, key);
	if (MV_OK != nRet)
	{
		return false;
	}
	return true;
}


//void Camera::SetCommandValue_software()
//{
//    int nRet = MV_CC_SetCommandValue(handle, "TriggerSoftware", MV_TRIGGER_SOURCE_SOFTWARE);
//
//}


void Camera::ResetEncoderCount()
{
	nRet = MV_CC_SetCommandValue(handle, "EncoderCounterReset");
}

unsigned int Camera::GetEncoderCount()
{
	//获取int型参数
	MVCC_INTVALUE struIntValue = { 0 };
	nRet = MV_CC_GetIntValue(handle, "EncoderCounter", &struIntValue);
	if (MV_OK != nRet)
	{
		return 0;
	}
	else
	{
		return struIntValue.nCurValue;
	}

}

void Camera::OpenRGBCamera()
{
    try {
        // 相机初始化代码

        // ch:枚举设备 | Enum device
        MV_CC_DEVICE_INFO_LIST stDeviceList;
        memset(&stDeviceList, 0, sizeof(MV_CC_DEVICE_INFO_LIST));
        nRet = MV_CC_EnumDevices(MV_GIGE_DEVICE | MV_USB_DEVICE, &stDeviceList);
        if (MV_OK != nRet)
        {
            //printf("Enum Devices fail! nRet [0x%x]\n", nRet);
            //QMessageBox::information(NULL, "错误", "没有找到相机！", QMessageBox::No);
            qCritical() << "Camera:Enum devices fail!";
            //return;

        }

        qInfo() << "Camera:Enum devices success,num = " << stDeviceList.nDeviceNum;

        /*unsigned int nIndex = theApp.m_RGBCameraIndex;*/
        int nIndex = -1;
        if (stDeviceList.nDeviceNum > 0)
        {
            qInfo() << "Camera:Find devices num of " << stDeviceList.nDeviceNum;
            for (unsigned int i = 0; i < stDeviceList.nDeviceNum; i++)
            {
                if (stDeviceList.pDeviceInfo[i] == NULL)
                {
                    continue;
                }

                nIndex = i;

                int nIp1 = ((stDeviceList.pDeviceInfo[i]->SpecialInfo.stGigEInfo.nCurrentIp & 0xff000000) >> 24);
                int nIp2 = ((stDeviceList.pDeviceInfo[i]->SpecialInfo.stGigEInfo.nCurrentIp & 0x00ff0000) >> 16);
                int nIp3 = ((stDeviceList.pDeviceInfo[i]->SpecialInfo.stGigEInfo.nCurrentIp & 0x0000ff00) >> 8);
                int nIp4 = (stDeviceList.pDeviceInfo[i]->SpecialInfo.stGigEInfo.nCurrentIp & 0x000000ff);
                std::stringstream ss;
                ss << nIp1 << "." << nIp2 << "." << nIp3 << "." << nIp4;
                auto ip = ss.str();
                // 找到对应相机后不再对比其它设备ip
                if(ip == State::CameraInfo::IpAddress)
                {
                    qInfo() << "Camera:The target ip device was successfully located.";
                    break;
                }
            }


            if(nIndex < 0)
            {
                qCritical() << "Camera:No find Camera and index less-than 0！";
                return;
            }

            // ch:选择设备并创建句柄 | Select device and create handle
            nRet = MV_CC_CreateHandle(&handle, stDeviceList.pDeviceInfo[nIndex]);
            if (MV_OK != nRet)
            {
                //QMessageBox::information(NULL, "错误", "Create Handle fail!", QMessageBox::No);
                qCritical() << "Camera:Create Handle fail!";
            }
            else
            {
                qInfo() << "Camera:Create handle success.";
            }

            // ch:打开设备 | Open device
            nRet = MV_CC_OpenDevice(handle);
            if (MV_OK != nRet)
            {
                //QMessageBox::information(NULL, "错误", "打开RGB相机失败", QMessageBox::No);
                qCritical() << "Camera:Open RGB camera failed: " << State::CameraInfo::IpAddress;
            }
            else
            {
                bCamereConnectedOk = true;
                qInfo() << "Camera:Open RGB camera success.";
            }

            // ch:探测网络最佳包大小(只对GigE相机有效) | en:Detection network optimal package size(It only works for the GigE camera)
            /*if (stDeviceList.pDeviceInfo[nIndex]->nTLayerType == MV_GIGE_DEVICE)
            {
                int nPacketSize = MV_CC_GetOptimalPacketSize(handle);
                if (nPacketSize > 0)
                {
                    nRet = MV_CC_SetIntValue(handle, "GevSCPSPacketSize", nPacketSize);
                    if (nRet != MV_OK)
                    {
                        qWarning() << "相机：Warning: Set Packet Size fail";
                    }
                }
                else
                {
                    qWarning() << "相机：Warning: Get Packet Size fail";
                }
            }*/

            nRet = MV_CC_SetFloatValue(handle, "LineRateRatio", State::CameraInfo::LineRateRatio);
            if (MV_OK != nRet)
            {
                qWarning()  << "Camera:Set LineRateRatio failed!";
            }
            else
            {
                qInfo() << "Camera:Set LineRateRatio success.";
            }

            ////设置触发模式
            if(State::CameraInfo::TriggerSource)
            {
                nRet = MV_CC_SetEnumValue(handle, "TriggerMode", MV_TRIGGER_MODE_ON);
                if (MV_OK != nRet)
                {
                    //printf("Set Trigger Mode fail! nRet [0x%x]\n", nRet);
                    qWarning() << "Camera:Set Trigger Mode failed!";
                }
                else
                {
                    qInfo() << "Camera:Set Trigger Mode success.";
                }

                // 设置触发源为软触发
                nRet = MV_CC_SetEnumValue(handle, "TriggerSource", MV_TRIGGER_SOURCE_SOFTWARE);
                if (MV_OK != nRet)
                {
                    //printf("Set Trigger Source fail! nRet [0x%x]\n", nRet);
                    qWarning() << "Camera:Set Trigger Source fail!";
                }
                else
                {
                    qInfo() << "Camera:Set Trigger Source Success.";
                }
            }

            // ch:注册抓图回调 | en:Register image callback
            //nRet = MV_CC_RegisterImageCallBackEx(handle, ImageCallBackEx, handle);
            nRet = MV_CC_RegisterImageCallBackEx(handle, ImageCallBackEx_New, handle);
            if (MV_OK != nRet)
            {
                //printf("Register Image CallBack fail! nRet [0x%x]\n", nRet);
                qWarning() << "Camera:Register Image CallBack fail! ";
            }
            else
            {
                qInfo() << "Camera:Register Image CallBack Success.";
            }

            // ch:获取数据包大小 | en:Get payload size
            MVCC_INTVALUE stParama1;
            memset(&stParama1, 0, sizeof(MVCC_INTVALUE));
            nRet = MV_CC_GetIntValue(handle, "PayloadSize", &stParama1);
            if (MV_OK != nRet)
            {
                //printf("Get PayloadSize fail! nRet [0x%x]\n", nRet);
                qWarning() << "Camera:Get PayloadSize fail!";
            }
            else
            {
                qInfo() << "Camera:Get PayloadSize Success.";
            }

            // ch:开始取流 | en:Start grab image
            nRet = MV_CC_StartGrabbing(handle);
            if (MV_OK != nRet)
            {
                //printf("Start Grabbing fail! nRet [0x%x]\n", nRet);
                qWarning() << "Camera:Start Grabbing fail!";
            }
            else
            {
                qInfo() << "Camera:Start Grabbing Success.";
            }

            qInfo() << "Camera:Camera Open And Init Successfully.";
        }
        else
        {
            qWarning() << "Camera:Find No Devices!";
        }


    }
    catch (const std::exception& e) {
        qCritical() << "相机初始化异常:" << e.what();
    }
    catch (...) {
        qCritical() << "相机初始化未知异常";
    }


}

void Camera::CloseRGBCamera()
{
	MV_CC_SetCommandValue(handle, "EncoderCounterReset");
	MV_CC_SetCommandValue(handle, "DeviceReset");
	// ch:停止取流 | en:Stop grab image
	nRet = MV_CC_StopGrabbing(handle);
	if (MV_OK != nRet)
	{
        //printf("Stop Grabbing fail! nRet [0x%x]\n", nRet);
        qCritical() << "Camera:Stop Grabbing fail!";
	}

	nRet = MV_CC_CloseDevice(handle);
	if (MV_OK != nRet)
	{
        //printf("Close Device fail! nRet [0x%x]\n", nRet);
        qCritical() << "Camera:Close Device fail!";
	}

	// ch:销毁句柄 | en:Destroy handle
	nRet = MV_CC_DestroyHandle(handle);
	if (MV_OK != nRet)
	{
        //printf("Destroy Handle fail! nRet [0x%x]\n", nRet);
        qCritical() << "Camera:Destroy Handle fail!";
	}
	bCamereConnectedOk = false;
}
