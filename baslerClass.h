#pragma once
#include <pylon/PylonIncludes.h>
#include <pylon/usb/BaslerUsbInstantCamera.h>

using namespace Basler_UsbCameraParams;

/* Pixel Format
Basler_UsbCameraParams::PixelFormat_Mono8
Basler_UsbCameraParams::PixelFormat_RGB8
Basler_UsbCameraParams::PixelFormat_BayerRG8
*/
struct BaslerParam {
	int width = 720;
	int height = 540;
	int offsetX = NULL;
	int offsetY = NULL;
	double exposureTime = NULL; // us
	double frameRate = NULL; // fps
	double gain = 0.0;
	bool reverseX = false;
	bool reverseY = false;

	PixelFormatEnums pixelFormat = PixelFormat_BGR8;
	TriggerModeEnums triggerMode = TriggerMode_Off;
	TriggerSourceEnums triggerSource = TriggerSource_Software;
	double triggerDelay = 0.0;

};

class BaslerClass {
   public:
    int width = 720;
    int height = 540;
	int offsetX = 0;
	int offsetY = 0;

   private:
	   Pylon::CBaslerUsbInstantCamera *pCamera;
	   Pylon::CGrabResultPtr ptrGrabResult;
   public:
	   BaslerClass();
    ~BaslerClass();

    void start(int deviceId = 0);
    int setParam(BaslerParam param);
	void printStatus();
	int getData(void *data);
	int executeSoftwareTrigger();
};