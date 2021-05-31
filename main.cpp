#include <time.h>
#include <iostream>
#include <fstream>
#include "DXRPathTracer.h"
#include "Input.h"
#include "debug.h"
#include "pch.h"
#include "sceneLoader.h"
#include "timer.h"
#include "MotionManager.h"

#define USE_WEBCAM 1
#define USE_BASLER_UM 2
#define USE_BASLER_UC 3

#define CAM_MODE 2
#define USE_DYNA_V1 1

#if USE_DYNA_V1
#include "HighSpeedProjector.h"
#endif

#if CAM_MODE == USE_BASLER_UC || CAM_MODE == USE_BASLER_UM
#include "baslerClass.h"
#include "FaceTrack.h"
//#pragma comment(lib, "BaslerLib.lib")
#include <dlib/opencv.h>
#include <dlib/image_processing/frontal_face_detector.h>
#include <dlib/image_processing/render_face_detections.h>
#include <dlib/image_processing.h>
#include <dlib/gui_widgets.h>

#endif

#define PAT_ORIGINAL

namespace {
	constexpr int WINDOW_WIDTH = 1024;
	constexpr int WINDOW_HEIGHT = 768;
	bool minimized = false;

	constexpr int CAM_WIDTH = 720;
	constexpr int CAM_HEIGHT = 540;
	constexpr int CAM_FPS = 500;

	//double pjFPS = 3600.0;
	const int resDepth = 90;
	const int patternNum = resDepth * 4;
	float motor_rps = 25.40;
	//float motor_rps = 0.25;
	const int projectionFPS = patternNum * motor_rps;

	UINT8 result[WINDOW_WIDTH * WINDOW_HEIGHT * patternNum / 8];
	UINT8 pat[WINDOW_WIDTH * WINDOW_HEIGHT / 8];
	UINT8 patterns[patternNum][WINDOW_WIDTH * WINDOW_HEIGHT / 8];
}  // namespace

static int binToRGB(UINT8* dest, UINT8* src, int pixelNum) {
	for (int i = 0; i < pixelNum; i++) {
		int i_ = (int)i / 8.0;
		UINT8 color = (src[i_] << (i % 8) & 0b10000000) ? 0xff : 0;
		*dest++ = color;  // red
		*dest++ = color;  // green
		*dest++ = color;  // blue
	}
	return 0;
}

int main() {

	//-------------------------------- Setting Parameter ------------------------
	int cameraMode = 0;  // 0 : Camera 1: eye
	float targetRad = 30.0;
	int type = 0;        // 0 ~ 3
	uint seed = 0;
	boolean loopFlag = true;
	bool flagprjSync = false;

	volatile int sceneFrame = 0;
	volatile int rtFrame = 0;
	volatile int cpFrame = 0;
	volatile int cvtFrame = 0;
	volatile int projFrame = 0;
	volatile int capFrame = 0;
	volatile int faceFrame = 0;
	volatile int landFrame = 0;

	cv::Mat faceResult(cv::Size(CAM_WIDTH, CAM_HEIGHT), CV_8UC3, cv::Scalar(0, 0, 0));
	cv::Mat camImg(cv::Size(CAM_WIDTH, CAM_HEIGHT), CV_8UC1, cv::Scalar(255));
	cv::Mat output(cv::Size(WINDOW_WIDTH, WINDOW_HEIGHT), CV_8UC4, cv::Scalar(0.0));
	cv::Mat binPattern = cv::Mat(cv::Size(WINDOW_WIDTH, WINDOW_HEIGHT), CV_8UC3, cv::Scalar::all(0));

	MouseEvent input;
	SceneManager sceneManager;

	//---------------------- Set Content ----------------------------------
	sceneManager.object_number.push_back(sceneManager.BACK);

	sceneManager.object_number.push_back(sceneManager.M_PANDA);
	//sceneManager.object_number.push_back(sceneManager.M_DEER);
	//sceneManager.object_number.push_back(sceneManager.M_BUNNY);

	
	//-----------------------------------------------------------------------

	sceneManager.initScene();

	HighSpeedProjector hsproj;
	MotionManager mcManager(hsproj);
	int flagOneSec = false;

	std::thread thrProj([&] {
		int buffNum = patternNum;

#if USE_DYNA_V1
		hsproj.set_projection_mode(PM::MIRROR);
		hsproj.set_projection_mode(PM::BINARY);
		hsproj.set_parameter_value(PP::FRAME_RATE, projectionFPS);
		hsproj.set_parameter_value(PP::BUFFER_MAX_FRAME, 50);
		int count = 0;
		//UINT8 pat_[WINDOW_WIDTH * WINDOW_HEIGHT / 8];
		hsproj.connect_projector();

		while (loopFlag) {

			 if (hsproj.send_image_binary(patterns[seed])) {
				projFrame ++;
				//projFrame += hsproj.send_image_binary(binary_row);
				//memcpy(pat_, result + WINDOW_WIDTH * WINDOW_HEIGHT / 8 * count, WINDOW_WIDTH * WINDOW_HEIGHT / 8);
				seed = (seed + 1) % buffNum; 
			}
		}
		hsproj.stop();
#endif
		});

#if CAM_MODE != 0
	std::thread thrCapture([&] {
			BaslerClass camera;
			const Basler_UsbCameraParams::PixelFormatEnums pixelFormat = PixelFormat_Mono8;
			try {
				// camera param
				BaslerParam param;
				param.width = CAM_WIDTH;
				param.height = CAM_HEIGHT;
				param.frameRate = CAM_FPS;
				param.exposureTime = 1000000.0 / param.frameRate - 220.0; // us
				param.pixelFormat = pixelFormat;

				camera.setParam(param);
				camera.start();
				while (loopFlag) {
					capFrame += camera.getData(camImg.data);
					
				}
			}
			catch (const Pylon::GenericException& e) {
				std::cerr << "An exception occurred." << std::endl
					<< e.GetDescription() << std::endl;
			}
			});

		int status = 0;
		FaceTrack tracker(CAM_WIDTH, CAM_HEIGHT);
		
		
		std::thread thrFaceTrack([&] {
			while (loopFlag) {
				status = tracker.faceDetect(faceResult);
				faceFrame++;
			}
			});

		std::thread thrLandTrack([&] {
			dlib:: frontal_face_detector detector = dlib::get_frontal_face_detector();
			dlib::shape_predictor sp;
			float p = 0.05;

			while (loopFlag) {
					tracker.landMarkDetect(faceResult);
				//tracker.face_tracking(faceResult, sp, detector, p);
					landFrame++;
			}
			});
								
#endif

	DXRPathTracer * tracer;
	tracer = new DXRPathTracer(WINDOW_WIDTH, WINDOW_HEIGHT, DXGI_FORMAT_B8G8R8A8_UNORM, 0);
	tracer->setupScene(sceneManager.getScene());

	tracer->setMtlResDepth(0, 15); //15 Blue
	tracer->setMtlResDepth(1, 90); //90 White
	tracer->setMtlResDepth(2, 90); //10 Yellow
	tracer->setMtlResDepth(3, 30); //30 Orange

	std::thread thrScene([&] {
		float3 eye = float3(0.0, 00.0, 20.0);
		float rad = 0;
		while (loopFlag) {
			if (!minimized) {
				switch (cameraMode) {
				case 0:
					tracer->updateCamera(input);
					tracer->resetEye();
					break;
				case 1:
					tracer->updateCamera(input);
					break;
				case 2:
					tracer->updateEye(input);
					break;
				case 3:
#if CAM_MODE
					eye.x = -tracker.ViewPoint.at<float>(0, 0);
					eye.y = -tracker.ViewPoint.at<float>(0, 1);
					eye.z = tracker.ViewPoint.at<float>(0, 2);
#endif
					tracer->updateEye(eye, 0);
					break;
				}

				tracer->update(seed);
				tracer->updateScene(sceneManager.getScene());

				sceneFrame++;
			}
		}
		});

	std::thread thrRT([&] {
		while (loopFlag) {
			tracer->shootRays();
			rtFrame++;
		}
		});

	std::thread thrCopy([&] {
		int status = 0;
		while (loopFlag) {
			status = tracer->getResult(output.data); //この行をコメントアウトしないと低確率で例外スローがおきます
			//撮影時、デモ時はコメントアウト推奨
			status = tracer->getPatternResult(result);
			if (status == 1) {
				cpFrame++;
			}
		}
		});

	
	std::thread thrCvt([&] {
		while (loopFlag) {
			for (int i = 0; i < patternNum; i++) {
				memcpy(patterns[i], result + WINDOW_WIDTH * WINDOW_HEIGHT / 8 * (i % patternNum), WINDOW_WIDTH* WINDOW_HEIGHT / 8);
			}
			memcpy(pat, result + WINDOW_WIDTH * WINDOW_HEIGHT / 8 * (seed % patternNum), WINDOW_WIDTH * WINDOW_HEIGHT / 8);
			//cv::imwrite("./output/1.bmp", result[0]);
			//binToRGB(binPattern.data, pat, WINDOW_WIDTH * WINDOW_HEIGHT);
			cvtFrame++;
		}
		});

	std::thread thrMotor([&] {
			mcManager.setSpeedInRPS(motor_rps);
			mcManager.threadFunc();
			mcManager.stopToMove();
		});

	int key;
	clock_t start = clock();
	cv::imshow("output", output);
	cv::imshow("pattern", output);
	cv::setMouseCallback("output", &MouseEvent::callback, &input);
	cv::setMouseCallback("pattern", &MouseEvent::callback, &input);
	uint seedIncriment = 0;

	int count = 0;
	double diffsp = 0.0f;
	double temp = 0.0;
	int seedCount = 0;
	bool decFlag = true;

	while (loopFlag) {
		clock_t now = clock();
		const double time_ = static_cast<double>(now - start) / CLOCKS_PER_SEC;
		if (mcManager.error < -0.0032) {
			seed = (seed + 1) % patternNum;
			std::cout << "Phase Proj Increased!" << std::endl;
		}
		if (mcManager.error > 0.0027) {
			seed = (seed + 360 - 1) % patternNum;
			std::cout << "Phase Proj decreased!" << std::endl;
		}

		if (time_ > 1.0) {
			
			
			if (mcManager.flagSync) {
				const char *fileName = "TimeResult.txt";

				std::ofstream ofs(fileName, std::ios::app);
				if (ofs && faceFrame / time_ >= 180.0) {
					std::cout
						// << "total : " << (rtFrame[0] + rtFrame[1] + rtFrame[2] + rtFrame[3]) / time << " fps "
						<< "Total			: " << cvtFrame / time_ << " fps \n"
						<< "Scene			: " << sceneFrame / time_ << " fps \n"
						<< "RT			: " << rtFrame / time_ << " fps \n"
						<< "Copy			: " << cpFrame / time_ << " fps \n"
						<< "Projection		: " << projFrame / time_ << " fps \n"
						<< "Capture			: " << capFrame / time_ << " fps \n"
						<< "FaceDet			: " << faceFrame / time_ << " fps \n"
						<< "Land+PnP		: " << landFrame / time_ << " fps \n"
						<< "Motor			: " << mcManager.motFrame / time_ << " fps \n"
						<< std::endl;
					ofs
						<< cvtFrame / time_						<< " 	"
						<< sceneFrame / time_					<< " 	"
						<< rtFrame / time_						<< " 	"
						<< cpFrame / time_						<< " 	"
						<< projFrame / time_					<< " 	"
						<< capFrame / time_						<< " 	"
						<< faceFrame / time_					<< " 	"
						<< landFrame / time_					<< " 	"
						<< mcManager.motFrame / time_	<< " 	"
						<< std::endl;
					ofs.close();
				}

			}

			/*
			std::cout << "X : " << -tracker.ViewPoint.at<float>(0,0) << " \t"
				<< "Y : " << -tracker.ViewPoint.at<float>(0, 1) << " \t"
				<< "Z : " << tracker.ViewPoint.at<float>(0, 2) << std::endl;
				*/

			
			
			if ((seedCount % 3) == 0) 	seed = (seed + 1) % patternNum;
			seedCount = (seedCount + 1) % 20;
			
			rtFrame = 0;
			sceneFrame = 0;
			cpFrame = 0;
			cvtFrame = 0;
			projFrame = 0;
			capFrame = 0;
			faceFrame = 0;
			landFrame = 0;
			mcManager.motFrame = 0;
			start = now;

		}

		cv::cvtColor(camImg, faceResult, cv::COLOR_GRAY2RGB);
		
		if (tracker.missCount == 0) {
			for (int i = 0; i < tracker.landmarks.size(); i++) {
				if (i == 27)
					cv::circle(faceResult, tracker.landmarks[i], 3, cv::Scalar(0, 255, 255), -1, 2);
				else
					cv::circle(faceResult, tracker.landmarks[i], 3, cv::Scalar(0, 255, 0), -1, 2);
			}
		}


		//cv::imshow("pattern", binPattern);
		cv::imshow("output", output);
		cv::imshow("facetrack", faceResult);

		key = cv::waitKey(1);
		switch (key) {
		case 'q':	loopFlag = false;	
			mcManager.flagMotionControl = false;
			break;
		case 'h':	cameraMode = 1;		break; //Mouse_Projection
		case 'j':	cameraMode = 2;		break; //Mouse Eye
		case 'k':	cameraMode = 0;		break; //EyeReset_Projection
		case 'l':	cameraMode = 3;		break; //Camera Mode
		case 'e':
			seedIncriment = (seedIncriment == 0) ? 1 : 0;
			break;
		case 'u':
			cv::imwrite("./output/" + std::to_string(time(NULL)) + ".bmp", output);
			break;
		case '.':
			seed = (seed + patternNum - 1) % patternNum;
			break;
		case ',':
			seed = (seed + 1) % patternNum;
			break;
		case 'n':
			targetRad -= 2.0;
			sceneManager.setRotation(1, getRotationAsQuternion({ 0, 1, 0 }, targetRad));
			break;
		case 'm':
			targetRad += 2.0;
			sceneManager.setRotation(1, getRotationAsQuternion({ 0, 1, 0 }, targetRad));
			break;
		case 't':
			motor_rps = motor_rps - 0.02;
			mcManager.changeRPSTo(motor_rps, 200);
			std::cout << "diffsp" << diffsp << std::endl;
		case 'y':
			motor_rps = motor_rps + 0.01;
			mcManager.changeRPSTo(motor_rps, 200);
			std::cout << "diffsp" << diffsp << std::endl;
		case 'a': 
			mcManager.startToMove(1000); 
			mcManager.changeRPSTo(motor_rps / 2, 2500);
			mcManager.changeRPSTo(motor_rps, 2500);
			break;
		case 'i': flagprjSync = true; break;
		case 'o': flagprjSync = false; break;
		case 's' :  mcManager.startSync(); break;
		case 'w': mcManager.stopSync(); break;
		case 'c' : mcManager.stopToMove(); break;
		case 'd': mcManager.interactivePhaseIncrement = 0.01; break;
		case 'f': mcManager.interactivePhaseIncrement = -0.02; break;
		}
		if (key >= '0' && key < '9') {
			int num = key - 48;
			type = num;
			tracer->setType(type);
		}

#ifdef PAT_ORIGINAL
		
		sceneManager.setTranslation(3, float3(
			-45.0 + 20.0 * sin(count / 150.0),
			-55.0 + 20.0 * cos(count / 150.0),
			25.0 //+ 5 * cos(count / 25.0)
		));
		
		sceneManager.setRotation(2, getRotationAsQuternion({ 0, 1, 0 }, count/2));
		sceneManager.setRotation(3, getRotationAsQuternion({ 0, 1, 0 }, count/5));
		
		//sceneManager.setRotation(1, getRotationAsQuternion({ 0, 0, 1 }, count));
#else 
		sceneManager.setTranslation(0, float3(
			-10.0,
			5.0,
			15.0 + 25 * std::pow(cos(PI / 8.0 * ((-10 * 0 + count) / 24.0)), 1.0/3.0)
		));
		sceneManager.setTranslation(1, float3(
			10.0,
			5.0,
			15.0 + 25 * std::pow(cos(PI / 8.0 * ((-10 * 1 + count) / 24.0)), 1.0 / 3.0)
		));
		sceneManager.setTranslation(2, float3(
			-20.0,
			-20.0,
			15.0 + 25 * std::pow(cos(PI / 8.0 * ((-10 * 2 + count) / 24.0)), 1.0 / 3.0)
		));
		sceneManager.setTranslation(3, float3(
			-12.0,
			-20.0,
			15.0 + 25 * std::pow(cos(PI / 8.0 * ((-10 * 3 + count) / 24.0)), 1.0 / 3.0)
		));
		sceneManager.setTranslation(4, float3(
			-4.0,
			-20.0,
			15.0 + 25 * std::pow(cos(PI / 8.0 * ((-10* 4 + count )/ 24.0)), 1.0 / 3.0)
		));
		sceneManager.setTranslation(5, float3(
			4.0,
			-20.0,
			15.0 + 25 * std::pow(cos(PI / 8.0 * ((-10 * 5 + count )/ 24.0)), 1.0 / 3.0)
		));
		sceneManager.setTranslation(6, float3(
			12.0,
			-20.0,
			15.0 + 25 * std::pow(cos(PI / 8.0 * ((-10 * 6 + count) / 24.0)), 1.0 / 3.0)
		));
		sceneManager.setTranslation(7, float3(
			20.0,
			-20.0,
			15.0 + 25 * std::pow(cos(PI / 8.0 * ((-10 * 7 + count) / 24.0)), 1.0 / 3.0)
		));

#endif

		sceneManager.update();

		seed = (seed + seedIncriment) % patternNum;
		count++;
	}

	thrProj.join();
#if CAM_MODE
	thrCapture.join();
	thrFaceTrack.join();
	thrLandTrack.join();
#endif
	thrMotor.join();
	thrScene.join();
	thrRT.join();
	thrCopy.join();
	thrCvt.join();

	return 0;
}