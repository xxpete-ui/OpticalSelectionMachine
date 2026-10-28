#pragma once
#include "MvCameraControl.h"
//#include "MvGigEDevice.h"
//using namespace MvCamCtrl;
class Camera
{
public:
    bool bCamereConnectedOk = false;
public:
	Camera();
	~Camera();
public:
	void CloseRGBCamera();
	void OpenRGBCamera();
	unsigned int GetEncoderCount();
	void ResetEncoderCount();
	float GetFrameRate();
	bool SetIntValue(char* key, unsigned int nValue);
	bool SetCommandValue(const char* key);

	//void SetCommandValue_software();
private:
	int nRet = MV_OK;
};

extern int callbackcount;
extern void* handle;
//
