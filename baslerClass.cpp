#include "baslerClass.h"

BaslerClass::BaslerClass() {
	Pylon::PylonInitialize();
	pCamera = new Pylon::CBaslerUsbInstantCamera(Pylon::CTlFactory::GetInstance().CreateFirstDevice());

	// Print the model name of the camera.
	std::cout << "Using device " << pCamera->GetDeviceInfo().GetModelName() << std::endl;

	pCamera->MaxNumBuffer = 5;

	// Open the camera.
	pCamera->Open();


	offsetX = pCamera->OffsetX.GetMin();
	offsetY = pCamera->OffsetY.GetMin();
	width = pCamera->Width.GetMax();
	height = pCamera->Height.GetMax();
}

BaslerClass::~BaslerClass() {
	pCamera->Close();
    Pylon::PylonTerminate();
}

int BaslerClass::setParam(BaslerParam param) {
	// Trigger
	pCamera->TriggerMode.SetValue(param.triggerMode);
	if (param.triggerMode == TriggerMode_On) {
		pCamera->TriggerSelector.SetValue(TriggerSelector_FrameStart);
		pCamera->TriggerDelay.SetValue(param.triggerDelay);
		pCamera->TriggerSource.SetValue(param.triggerSource);
	}

	// Exposure time
	if (param.exposureTime != NULL) {
		pCamera->ExposureAuto.SetValue(ExposureAuto_Off);
		pCamera->ExposureMode.SetValue(ExposureMode_Timed);
		pCamera->ExposureTime.SetValue(param.exposureTime);
	}
	else {
		pCamera->ExposureAuto.SetValue(ExposureAuto_Once);
	}
	
	// Gain
	pCamera->Gain.SetValue(param.gain);

	// ROI
	width = param.width;
	height = param.height;
	if (param.offsetX == NULL) {
		pCamera->CenterX.SetValue(true);
	}
	else {
		if (IsWritable(pCamera->OffsetX)) {
			pCamera->OffsetX.SetValue(offsetX);
		}
	}
	if (param.offsetX == NULL) {
		pCamera->CenterY.SetValue(true);
	}
	else {
		if (IsWritable(pCamera->OffsetY)) {
			pCamera->OffsetY.SetValue(offsetY);
		}
	}
	pCamera->Width.SetValue(width);
	pCamera->Height.SetValue(height);

	// Reverse
	pCamera->ReverseX.SetValue(param.reverseX);
	pCamera->ReverseY.SetValue(param.reverseY);

	// Frame rate
	if (param.frameRate != NULL) {
		pCamera->AcquisitionFrameRateEnable.SetValue(true);
		pCamera->AcquisitionFrameRate.SetValue(param.frameRate);
	}

	// Pixel format
	pCamera->PixelFormat.SetValue(param.pixelFormat);

	printStatus();
	
	return 0;
}

void BaslerClass::printStatus() {
	std::cout
		<< "\033[36m"
		<< "Size          : " << pCamera->Width.GetValue() << " x " << pCamera->Height.GetValue() << std::endl
		<< "Exposure Time : " << pCamera->ExposureTime.GetValue() << " us" << std::endl
		<< "Trigger       : " << pCamera->TriggerMode.GetValue() << std::endl
		<< "\033[39m" << std::endl;
}

void BaslerClass::start(int deviceId) {
    // Start Capture
	pCamera->StartGrabbing(Pylon::GrabStrategy_OneByOne);

    // This smart pointer will receive the grab result data.
	Pylon::CGrabResultPtr ptrGrabResult;
}

int BaslerClass::getData(void *data) {
    // Wait for an image and then retrieve it. A timeout of 5000 ms is used.
	pCamera->RetrieveResult(5000, ptrGrabResult, Pylon::TimeoutHandling_ThrowException);

    // Image grabbed successfully?
    if (ptrGrabResult->GrabSucceeded()) {
		memcpy(data, (uint8_t*)ptrGrabResult->GetBuffer(), ptrGrabResult->GetBufferSize());
        return 1;
    } else {
        return 0;
    }
}

int BaslerClass::executeSoftwareTrigger() {
	pCamera->TriggerSoftware.Execute();
	return 0;
}