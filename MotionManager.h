#pragma once

#pragma once

#include <iostream>
#include <fstream>
#include <sstream>
#include <Windows.h>
#include "CSmc.h"
#include "CSmcdef.h"
#include <thread>
#include <vector>
#include <stdexcept>
#include <assert.h>
#include <stdio.h>
#include "HighSpeedProjector.h"

#define NUM_PROJ_PER_ROT 360

//IMPORTANT NOTICE: LINKER関係のエラーが出たらDEBUGモードがx64向けになっているかを確認すること！
#pragma comment(lib,"CSMC.lib")

class MotionController {


public:
	//-------------
	//declaration for system parameters
	//-------------
	long dwRet;
	short MotionType;
	double dblResolveSpeed;
	double dblStartSpeed;
	double dblTargetSpeed;
	double dblAccelTime;
	double dblDecelTime;
	double dblSSpeed;
	long lDistance;
	short lZCount;//modify
	char ErrorString[256];
	char szErrorString[256];
	short StartDir;
	bool bRet;
	short Id = -1;
	short AxisNo;
	short bCoordinate;
	short Change;

	//constructor
	MotionController();

	//member function
	bool InitMotionController();
	void SetDeviceName();
	bool SetMoveParam();
	void StartToMove(double);
	void StopToMove();
	long GetCountPulse();
	void ChangeMotionTo(double, double);
};


class MotionManager {

public:
	//fields
	HighSpeedProjector& projector;//投影枚数把握用の高速プロジェクタ
	MotionController mc;//物理モータとその制御に関するクラス
	int mode;//拡張
	std::thread mcThread; //---- スレッドオブジェクト
	bool flagMotionControl = true;// --スレッドから抜けるためのflag
	bool flagSync = false;

	int numProjPerRotation = NUM_PROJ_PER_ROT;
	double interactivePhaseIncrement = 0;

	double defaultSpeed;//11248/
	double phaseProjector;//projectorの絶対位相
	double phaseActuator;//actuatorの絶対位相
	double errorI;
	double error;


	//コンストラクタ

	MotionManager(HighSpeedProjector& hsp)
		: projector(hsp) {};


	//member functions
	void stopToMove();//s-curve stop
	void startToMove(double);//s-curve 
	void changeSpeedTo(double targetSpeed, double timeToChange);//time[ms]でtarget-speedへ変化
	void changeRPSTo(double targetSpeed, double timeToChange);//


	void startSync();
	void stopSync();

	void finishToControl();//flagMotionControlをfalseにします。
	void runThread();//スレッド実行
	void joinThread();//スレッドjoin
	void threadFunc();//スレッド用関数

	void setProjPerRot(int);
	void setSpeedInRPS(double);
	double rps2pps(double);
	int motFrame = 0;

};