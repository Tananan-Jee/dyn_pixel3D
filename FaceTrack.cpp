#include "FaceTrack.h"
#include <dlib/gui_widgets.h>
#include <dlib/image_processing.h>
#include <dlib/image_processing/frontal_face_detector.h>
#include <dlib/image_processing/render_face_detections.h>
#include <dlib/opencv.h>
#define PI 3.1415

namespace {
	dlib::frontal_face_detector mFaceDetector = dlib::get_frontal_face_detector();
	std::vector<dlib::rectangle> faceResults;
	dlib::shape_predictor predictor;
	dlib::full_object_detection land;
}

cv::Mat1f camera_Matrix = (cv::Mat_<float>(3, 3) <<
	720.5f, 0.0f, 364.2f,
	0.0f, 723.8f, 263.0f,
	0.0f, 0.0f, 1.0f);

cv::Mat dist_coeffs = cv::Mat::zeros(4, 1, cv::DataType<double>::type); // Assuming no lens distortion

cv::Mat1f RotationMat = (cv::Mat_<float>(3, 3) <<
	1.0f, 0.0f, 0.0f,
	0.0f, 1.0f, 0.0f,
	0.0f, 0.0f, 1.0f);
cv::Mat1f TranslationMat = (cv::Mat_<float>(3, 1) <<
	0.0f,
	100.0f,
	80.0f);

FaceTrack::FaceTrack(int width, int height) {

	dlib::deserialize("shape_predictor_68_face_landmarks.dat") >> predictor;

	mWidth = width;
	mHeight = height;

	int centerX = mWidth / 2;
	int centerY = mHeight / 2;

	one_face.push_back(dlib::rectangle(dlib::point(100, 100), dlib::point(300, 300)));

	face = {
		double(centerX),
		double(centerY),
		centerY - minH,
		centerY + minH,
		centerX - minW,
		centerX + minW
	};

	eye = cv::Point2f(0.0, 0.0);
	translation_vector = (cv::Mat_<float>(3, 1) <<
		0.0f,
		0.0f,
		0.0f
		);
	ViewPoint = (cv::Mat_<float>(3, 1) <<
		0.0f,
		0.0f,
		1200.0f
		);

	input = cv::Mat(mHeight, mWidth, CV_8UC1, cv::Scalar::all(255));
	/*
	model_points.push_back(cv::Point3d(0.0f, -170.0f, 135.0f));               // Nose tip
	model_points.push_back(cv::Point3d(0.0f, -500.0f, 70.0f));          // Chin
	model_points.push_back(cv::Point3d(-225.0f, 0.0f, 0.0f));       // Left eye left corner
	model_points.push_back(cv::Point3d(225.0f, 0.0f, 0.0f));        // Right eye right corner
	model_points.push_back(cv::Point3d(-150.0f, -320.0f, 10.0f));      // Left Mouth corner
	model_points.push_back(cv::Point3d(150.0f, -320.0f, 10.0f));       // Right mouth corner
	*/
	
	model_points.push_back(cv::Point3d(0.0f, -21.33f, 16.94f));               // Nose tip
	model_points.push_back(cv::Point3d(0.0f, -62.75f, 8.78f));          // Chin
	model_points.push_back(cv::Point3d(-32.0f, 0.0f, 0.0f));       // Left eye left corner
	model_points.push_back(cv::Point3d(32.0f, 0.0f, 0.0f));        // Right eye right corner
	model_points.push_back(cv::Point3d(-18.8f, -40.16f, 1.25f));      // Left Mouth corner
	model_points.push_back(cv::Point3d(18.8f, -40.16f, 1.25f));       // Right mouth corner
	
	nose_end_point3D.push_back(cv::Point3d(0, 0, 0));
	nose_end_point3D.push_back(cv::Point3d(0, 0, 500.0));
	rotX = 0.0; rotY = 0.0; rotZ = 0.0;
}


FaceTrack::~FaceTrack() {
}

int FaceTrack::setMinSize(int minw, int minh) {
	minH = minh;
	minW = minw;
	return 0;
}

int FaceTrack::faceDetect(cv::Mat img) {
	LARGE_INTEGER freq;
	LARGE_INTEGER start, end;
	QueryPerformanceFrequency(&freq);

	cv::Mat clipped(img, cv::Rect(face.left, face.top, face.right - face.left, face.bottom - face.top));
	cv::Mat minimize;
	cv::resize(clipped, minimize, cv::Size(), 1.0 / downRatio, 1.0 / downRatio);
	//cv::imshow("minimize",minimize);
	cv::waitKey(1);
	dlib::cv_image<dlib::bgr_pixel> dlibImg(minimize);
	QueryPerformanceCounter(&start);
	faceResults = mFaceDetector(dlibImg);
	QueryPerformanceCounter(&end);

	double time = static_cast<double>(end.QuadPart - start.QuadPart) / freq.QuadPart;
	//	std::cout << "time = " << time << " s" << std::endl;

	if (faceResults.size() > 0) {
		ROI result = {
			0,
			0,
			faceResults[0].top(),
			faceResults[0].bottom(),
			faceResults[0].left(),
			faceResults[0].right()
		};
		track(result);
		downRatio = std::min(double(faceResults[0].width()) / minW, double(faceResults[0].height()) / minH);
		missCount = 0;
		return 1;
	}
	else {
		spread();
		missCount++;
		return 0;
	}
	return -1;
}

int FaceTrack::landMarkDetect(cv::Mat img) {
	dlib::cv_image<dlib::bgr_pixel> dlibImg(img);
	dlib::rectangle rect(
		face.left,
		face.top,
		face.right,
		face.bottom
	);
	land = predictor(dlibImg, rect);
	landmarks.clear();


	for (int i = 0; i < land.num_parts(); i++) {
		landmarks.push_back(cv::Point(
			land.part(i).x(),
			land.part(i).y()
		));
	}

	eyes.push_back(cv::Point(
		land.part(27).x(),
		land.part(27).y()
	));

	while (eyes.size() > 30) {
		eyes.erase(eyes.begin());
	}
	cv::Point2f tmp(0, 0);
	for (auto e : eyes) {
		tmp.x += e.x;
		tmp.y += e.y;
	}
	eye.x = tmp.x / eyes.size();
	eye.y = tmp.y / eyes.size();

	cv::Mat1f ImagePoint = (cv::Mat_<float>(3, 1) <<
		eye.x,
		eye.y,
		1.0f
		);

	if (missCount == 0) {

		image_points.clear();
		image_points.push_back(cv::Point2d(landmarks[30]));
		image_points.push_back(cv::Point2d(landmarks[8]));
		image_points.push_back(cv::Point2d(landmarks[36]));
		image_points.push_back(cv::Point2d(landmarks[45]));
		image_points.push_back(cv::Point2d(landmarks[48]));
		image_points.push_back(cv::Point2d(landmarks[54]));

		cv::solvePnP(model_points, image_points, camera_Matrix, dist_coeffs, rotation_vector, translation_vector);
		projectPoints(nose_end_point3D, rotation_vector, translation_vector, camera_Matrix, dist_coeffs, nose_end_point2D);

		
		//ViewPoint.at<float>(0, 0) = translation_vector.at<float>(0,0) + TranslationMat.at<float>(0, 0);
		//ViewPoint.at<float>(0, 1) = - translation_vector.at<float>(0, 1) + TranslationMat.at<float>(0, 1);
		//ViewPoint.at<float>(0, 2) = translation_vector.at<float>(0, 2) + TranslationMat.at<float>(0, 2);
		
		cv::Rodrigues(rotation_vector, rotation_matrix);
		double* _r = rotation_matrix.ptr<double>();
		double projMatrix[12] = { _r[0],_r[1],_r[2],0,_r[3],_r[4],_r[5],0,_r[6],_r[7],_r[8],0 };
		cv::Mat projMat = cv::Mat(3, 4, CV_64FC1, projMatrix);
		cv::decomposeProjectionMatrix(projMat, cameraMatrix, rotation_matrix, translation_vector,
			rotMatrixX, rotMatrixY, rotMatrixZ, eulerAngles);
		rotX = eulerAngles[0];
		rotY = eulerAngles[1];
		rotZ = eulerAngles[2];

		eyeDet = (int)(sqrt(pow(image_points[2].x - image_points[3].x, 2) +
			pow(image_points[2].y - image_points[3].y, 2)) / std::cos(rotY * PI / 180));

		dist = (eye_dist * cam_width) / (2 * eyeDet * std::tan(cam_field * PI / (180 * 2)));
		ViewPoint = RotationMat.t() * (dist * camera_Matrix.inv() * ImagePoint - TranslationMat);
	}

	return 0;
}

int FaceTrack::spread() {
	float r = missCount * missCount / 10.0 / 10.0;
	face.vX = 0.0;
	face.vY = 0.0;
	face.top = std::max(int(face.top - r), 0);
	face.bottom = std::min(int(face.bottom + r), mHeight);
	face.left = std::max(int(face.left - r), 0);
	face.right = std::min(int(face.right + r), mWidth);
	if (face.bottom - face.top >= mHeight) Sleep(100);
	return 0;
}

int FaceTrack::track(ROI result) {

	double x = face.left + downRatio * (result.left + result.right) / 2;
	double y = face.top + downRatio * (result.top + result.bottom) / 2;

	face.vX = x - face.centerX;
	face.vY = y - face.centerY;
	face.centerX = x;
	face.centerY = y;

	// update roi
	int top = face.top;
	int left = face.left;
	int width = face.right - face.left;
	int height = face.bottom - face.top;
	double margin = (result.right - result.left) * 0.2;
	double coef = 2.0;
	face.top = std::max(int(top + downRatio * result.top + std::min(face.vY * coef, -margin)), 0);
	face.bottom = std::min(int(top + downRatio * result.bottom + std::max(face.vY * coef, margin)), mHeight);
	face.left = std::max(int(left + downRatio * result.left + std::min(face.vX * coef, -margin)), 0);
	face.right = std::min(int(left + downRatio * result.right + std::max(face.vX * coef, margin)), mWidth);
	return 0;
}

void FaceTrack::face_tracking(cv::Mat img, dlib::shape_predictor sp, dlib::frontal_face_detector detector, float p) {

	LARGE_INTEGER freq;
	QueryPerformanceFrequency(&freq);
	LARGE_INTEGER start, end;



	image = img.clone();
	face_image = image.clone();

	QueryPerformanceCounter(&start);


	//回転
	if (face_ok > 5)image_degree = 0, degree = 0;
	cv::Mat r = cv::getRotationMatrix2D(nose, image_degree, 1.0);
	cv::warpAffine(face_image, rotation_image, r, img.size(), 0, 1);//what size I should use?
	face_image = rotation_image.clone();
	if (face_ok < 2) {
		//画像クロップ
		if (faces.size() != 0) {
			roi_origin.x = faces[0].left() + velonica.width - faces[0].width() * p;
			roi_origin.y = faces[0].top() + velonica.height - faces[0].height() * p;
			roi_size.width = faces[0].width() + 2 * faces[0].width() * p;
			roi_size.height = faces[0].height() + 2 * faces[0].height() * p;

			if (roi_origin.x < 0) {
				roi_origin.x = 0;
			}
			if (roi_origin.y < 0) {
				roi_origin.y = 0;
			}
			if (roi_origin.x + roi_size.width > image.cols) {
				roi_size.width = image.cols - roi_origin.x;
			}
			if (roi_origin.y + roi_size.height > image.rows) {
				roi_size.height = image.rows - roi_origin.y;
			}


			cv::Rect roi(roi_origin, roi_size);
			face_image = rotation_image(roi);

		}

		//ダウンサンプリング
		if (faces.size() != 0) {
			down_ratio = std::min(double(faces[0].width()) / 80.0, double(faces[0].height()) / 80.0);
			cv::resize(face_image, face_image, cv::Size(), 1.0 / down_ratio, 1.0 / down_ratio);
		}
	}
	//顔検出
	dlib::cv_image<dlib::rgb_pixel>  cimg(face_image);
	faces = detector(cimg);
	QueryPerformanceCounter(&end);
	std::cout << "face detection time=" << static_cast<double>(end.QuadPart - start.QuadPart) * 1000.0 / freq.QuadPart << "ms";

	if (face_ok < 2) {
		//顔矩形領域復元
		for (int i = 0; i < faces.size(); i++) {
			faces[0].set_left(roi_origin.x + faces[0].left() * down_ratio);
			faces[0].set_right(roi_origin.x + faces[0].right() * down_ratio);
			faces[0].set_bottom(roi_origin.y + faces[0].bottom() * down_ratio);
			faces[0].set_top(roi_origin.y + faces[0].top() * down_ratio);
		}
	}
	if (faces.size() != 0) {
		one_face = faces;
		std::cout << faces[0] << std::endl;
		face_ok = 0;
	}
	else {
		int expand = 2;
		one_face[0].set_left(one_face[0].left() - expand);
		one_face[0].set_right(one_face[0].right() + expand);
		one_face[0].set_bottom(one_face[0].bottom() + expand);
		one_face[0].set_top(one_face[0].top() - expand);
		faces = one_face;

		face_ok++;;
	}


	//顔器官検出
	dlib::full_object_detection shape, virtual_shape;
	if (face_ok == 0) {
		for (unsigned long i = 0; i < faces.size(); ++i) {
			dlib::cv_image<dlib::rgb_pixel> c_img(rotation_image);
			QueryPerformanceCounter(&start);
			shape = sp(c_img, faces[i]);
			QueryPerformanceCounter(&end);
			std::cout << " Landmark detection time=" << static_cast<double>(end.QuadPart - start.QuadPart) * 1000.0 / freq.QuadPart << "ms" << std::endl;

			virtual_shape = shape;

			//顔器官復元
			if (image_degree != 0) {
				for (int i = 0; i < shape.num_parts(); i++) {
					shape.part(i).x() = std::cos(-degree) * (virtual_shape.part(i).x() - nose.x) - std::sin(-degree) * (virtual_shape.part(i).y() - nose.y) + nose.x;
					shape.part(i).y() = std::sin(-degree) * (virtual_shape.part(i).x() - nose.x) + std::cos(-degree) * (virtual_shape.part(i).y() - nose.y) + nose.y;

				}
			}
			//顔矩形復元
			if (image_degree != 0) {
				rect1.x = std::cos(-degree) * (faces[0].left() - nose.x) - std::sin(-degree) * (faces[0].top() - nose.y) + nose.x;
				rect1.y = std::sin(-degree) * (faces[0].left() - nose.x) + std::cos(-degree) * (faces[0].top() - nose.y) + nose.y;
				rect2.x = std::cos(-degree) * (faces[0].left() - nose.x) - std::sin(-degree) * (faces[0].bottom() - nose.y) + nose.x;
				rect2.y = std::sin(-degree) * (faces[0].left() - nose.x) + std::cos(-degree) * (faces[0].bottom() - nose.y) + nose.y;
				rect3.x = std::cos(-degree) * (faces[0].right() - nose.x) - std::sin(-degree) * (faces[0].bottom() - nose.y) + nose.x;
				rect3.y = std::sin(-degree) * (faces[0].right() - nose.x) + std::cos(-degree) * (faces[0].bottom() - nose.y) + nose.y;
				rect4.x = std::cos(-degree) * (faces[0].right() - nose.x) - std::sin(-degree) * (faces[0].top() - nose.y) + nose.x;
				rect4.y = std::sin(-degree) * (faces[0].right() - nose.x) + std::cos(-degree) * (faces[0].top() - nose.y) + nose.y;
			}
			else {
				rect1.x = faces[0].left();
				rect1.y = faces[0].top();
				rect2.x = faces[0].left();
				rect2.y = faces[0].bottom();
				rect3.x = faces[0].right();
				rect3.y = faces[0].bottom();
				rect4.x = faces[0].right();
				rect4.y = faces[0].top();
			}

			//角度抽出
			float dx = virtual_shape.part(16).x() - virtual_shape.part(0).x();
			float dy = virtual_shape.part(16).y() - virtual_shape.part(0).y();

			degree -= atan(dy / dx);
			image_degree += atan(dy / dx) * 180.0 / PI;

			//描画

			//draw_mesh(image, shape);

			for (int g = 1; g <= shape.num_parts(); g++) {
				cv::circle(image, cv::Point(shape.part(g - 1).x(), shape.part(g - 1).y()), 3, cv::Scalar(0, 0, 200), -1, 1);
			}

			cv::line(image, rect1, rect2, cv::Scalar(0, 255, 0), 3, 16);
			cv::line(image, rect2, rect3, cv::Scalar(0, 255, 0), 3, 16);
			cv::line(image, rect3, rect4, cv::Scalar(0, 255, 0), 3, 16);
			cv::line(image, rect4, rect1, cv::Scalar(0, 255, 0), 3, 16);

			//速度抽出
			if (norm(nose) != 0) {
				if (abs(shape.part(33).x() - nose.x) < faces[0].width() * 8)velonica.width = shape.part(33).x() - nose.x;
				if (abs(shape.part(33).y() - nose.y) < faces[0].height() * 8)velonica.height = shape.part(33).y() - nose.y;
			}
			nose.x = shape.part(33).x();
			nose.y = shape.part(33).y();

		}
	}




	//double time = static_cast<double>(end.QuadPart - start.QuadPart) * 1000.0 / freq.QuadPart;
	//std::cout << "time=" << time << endl;

	ok = false;

	result = image.clone();

	ok = true;

}
