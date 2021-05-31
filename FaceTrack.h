#pragma once
#ifndef _FACE_TRACKER_H_
#define _FACE_TRACKER_H_
#include <opencv2/opencv.hpp>
#include <opencv2/highgui.hpp>

#include <dlib/opencv.h>
#include <dlib/image_processing.h>
#include <dlib/image_processing/frontal_face_detector.h>
#include <dlib/image_io.h>
#include <dlib/gui_widgets.h>
#include <windows.h>
#include <iostream>

struct ROI {
	double centerX = 0.0;
	double centerY = 0.0;
	int top = 0;
	int bottom = 0;
	int left = 0;
	int right = 0;
	double vX = 0.0;
	double vY = 0.0;
};


class FaceTrack
{
private:
	int mWidth;
	int mHeight;
	int minW = 70;
	int minH = 70;

	int eye_dist = 64;
	int cam_width = 720;
	int cam_field = 34.5;


	double downRatio = 2.0;
	std::vector<cv::Point> eyes;
	cv::Mat input;

	bool ok = false;
	cv::Mat image_normal;
	cv::Mat image, face_image, rotation_image, detection_image, result;
	float image_degree = 0.0f, degree = 0.0f;
	cv::Point2f nose, roi_origin;
	cv::Size2i velonica, roi_size;
	std::vector<dlib::rectangle> faces, one_face;
	cv::Point2i rect1, rect2, rect3, rect4;
	float down_ratio = 1.0f;
	int face_ok = 2;

public:
	int missCount = 0;
	ROI face;

	cv::Point2f eye;
	cv::Mat1f ViewPoint;
	std::vector<cv::Point> landmarks;
	std::vector<cv::Point3d> model_points;
	std::vector<cv::Point2d> image_points;

	std::vector<cv::Point2d> nose_end_point2D;
	std::vector<cv::Point3d> nose_end_point3D;

	cv::Mat rotation_vector; // Rotation in axis-angle form
	cv::Mat translation_vector;

	cv::Mat rotation_matrix;
	cv::Mat cameraMatrix;
	cv::Mat rotMatrixX;
	cv::Mat	rotMatrixY;
	cv::Mat rotMatrixZ;
	cv::Vec3d eulerAngles;

	double rotX, rotY, rotZ;
	int eyeDet;
	float dist;

public:
	FaceTrack(int width, int height);
	~FaceTrack();
	int setMinSize(int minw, int minh);
	int faceDetect(cv::Mat img);
	int landMarkDetect(cv::Mat img);
	int track(ROI result);
	int spread();
	void face_tracking(cv::Mat img, dlib::shape_predictor sp, dlib::frontal_face_detector detector, float p);
};

#endif