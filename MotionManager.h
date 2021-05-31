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

//IMPORTANT NOTICE: LINKER�֌W�̃G���[���o����DEBUG���[�h��x64�����ɂȂ��Ă��邩���m�F���邱�ƁI
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
	HighSpeedProjector& projector;//���e�����c���p�̍����v���W�F�N�^
	MotionController mc;//�������[�^�Ƃ��̐���Ɋւ���N���X
	int mode;//�g��
	std::thread mcThread; //---- �X���b�h�I�u�W�F�N�g
	bool flagMotionControl = true;// --�X���b�h���甲���邽�߂�flag
	bool flagSync = false;

	int numProjPerRotation = NUM_PROJ_PER_ROT;
	double interactivePhaseIncrement = 0;

	double defaultSpeed;//11248/
	double phaseProjector;//projector�̐�Έʑ�
	double phaseActuator;//actuator�̐�Έʑ�
	double errorI;
	double error;


	//�R���X�g���N�^

	MotionManager(HighSpeedProjector& hsp)
		: projector(hsp) {};


	//member functions
	void stopToMove();//s-curve stop
	void startToMove(double);//s-curve 
	void changeSpeedTo(double targetSpeed, double timeToChange);//time[ms]��target-speed�֕ω�
	void changeRPSTo(double targetSpeed, double timeToChange);//


	void startSync();
	void stopSync();

	void finishToControl();//flagMotionControl��false�ɂ��܂��B
	void runThread();//�X���b�h���s
	void joinThread();//�X���b�hjoin
	void threadFunc();//�X���b�h�p�֐�

	void setProjPerRot(int);
	void setSpeedInRPS(double);
	double rps2pps(double);
	int motFrame = 0;

};