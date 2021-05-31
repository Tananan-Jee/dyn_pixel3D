#include "MotionManager.h"


//--------------
//implementation for MotionController
//----------------

//constructor
MotionController::MotionController() {

}

//--------------
//from basemove@OnInitDialog
//----------------
bool MotionController::InitMotionController() {

	AxisNo = 1;
	// Initialize
	dblResolveSpeed = 1;
	dblStartSpeed = 100;
	dblTargetSpeed = 3000;
	dblAccelTime = 2000;
	dblDecelTime = 2000;
	dblSSpeed = 400;
	lDistance = 1000;

	bCoordinate = CSMC_INC;
	MotionType = CSMC_JOG;
	StartDir = CSMC_CW;
	Change = 4;

	//wStr = _T("SMC003");
	//m_DeviceName.SetWindowText(wStr);
	SetDeviceName();

	return true;
}


//------
// SetDeviceName
// from void CBaseMoveDlg::OnEnChangeDeviceName()
// call from InitMotionController
//--------
void MotionController::SetDeviceName() {

	char	cDeviceName[256] = "SMC001";
	//	sprintf_s(cDeviceName, "%S", _T("SMC003"));
	dwRet = SmcWInit(cDeviceName, &Id);
	if (dwRet) {
		std::cout << "fail SmcWInit" << std::endl;
		return;
	}

	// ------------------------
	// Set lDistance to 1000
	// ------------------------
	lDistance = 1000;
	dwRet = SmcWSetStopPosition(Id, AxisNo, bCoordinate, lDistance);
	if (dwRet) {
		std::cout << "fail SmcWSetStopPosition" << std::endl;
		return;
	}

	//---------------------------
	// Get parameters of Driver
	//---------------------------
	//bRet = GetMoveParam();
	//if (bRet == FALSE)return;

}


//------------
// SetMoveParam
// call from
//--------------
bool MotionController::SetMoveParam() {
	dwRet = SmcWSetResolveSpeed(Id, AxisNo, dblResolveSpeed);
	dwRet = SmcWSetStartSpeed(Id, AxisNo, dblStartSpeed);
	dwRet = SmcWSetTargetSpeed(Id, AxisNo, dblTargetSpeed);
	dwRet = SmcWSetAccelTime(Id, AxisNo, dblAccelTime);
	dwRet = SmcWSetDecelTime(Id, AxisNo, dblDecelTime);
	dwRet = SmcWSetSSpeed(Id, AxisNo, dblSSpeed);
	dwRet = SmcWSetStopPosition(Id, AxisNo, bCoordinate, lDistance);
	return true;
}


//--------------
//StartToMove
// from void CBaseMoveDlg::OnBnClickedComCw()
//---------------
void MotionController::StartToMove(double targetSpeed = 3000) {
	dblTargetSpeed = targetSpeed;
	bRet = SetMoveParam();
	if (bRet == FALSE)return;
	dwRet = SmcWSetReady(Id, AxisNo, MotionType, StartDir);
	dwRet = SmcWMotionStart(Id, AxisNo);

}


//--------------
//StopToMove
// from void CBaseMoveDlg::OnBnClickedComSdStop()
//-------------
void MotionController::StopToMove() {
	dwRet = SmcWMotionDecStop(Id, AxisNo);
	if (dwRet) {
		exit(1);
	}
	return;
}


//-------------
//Get
// wrapping for Ret = SmcWGetCountPulse( Id , AxisNo , CountPulse )
//-------
long MotionController::GetCountPulse() {
	long CountPulse;
	dwRet = SmcWGetCountPulse(Id, AxisNo, &CountPulse);
	if (dwRet) {
		exit(1);
	}
	return CountPulse;
}


//--------------
//ChangeMotion
//from void CBaseMoveDlg::OnBnClickedComChange()
//---------------

void MotionController::ChangeMotionTo(double targetSpeed, double timeToChange = 500)
{
	dblTargetSpeed = targetSpeed;
	dwRet = SmcWSetTargetSpeed(Id, AxisNo, dblTargetSpeed);
	dwRet = SmcWSetAccelTime(Id, AxisNo, timeToChange);
	dwRet = SmcWSetDecelTime(Id, AxisNo, timeToChange);

	//--------------------
	// Set Change Motion
	//--------------------
	dwRet = SmcWSetMotionChangeReady(Id, AxisNo, Change);

	//----------------
	// Change Motion
	//----------------
	dwRet = SmcWMotionChange(Id, AxisNo);
	if (dwRet) {
		exit(1);
	}
	return;
}












//--------------
//implementation for MCThread
//----------------
//MotionManager::MotionManager(HSP& hsp){
//
//}


void MotionManager::stopToMove() {
	mc.StopToMove();
	Sleep(3000);
	return;
}
void MotionManager::startToMove(double targetSpeed = 3000) {
	mc.StartToMove(targetSpeed);
	Sleep(3000);
	return;
}

void MotionManager::changeSpeedTo(double targetSpeed, double timeToChange = 500) {
	mc.ChangeMotionTo(targetSpeed, timeToChange);
	return;
}

void MotionManager::changeRPSTo(double targetRPS, double timeToChange = 500) {
	changeSpeedTo(rps2pps(targetRPS), timeToChange);
}

void MotionManager::startSync() {
	flagSync = true;
	std::cout << "--START SYNCHRONIZED CONTROL-- " << std::endl;

}
void MotionManager::stopSync() {
	flagSync = false;
	std::cout << "--STOP SYNCHRONIZED CONTROL-- " << std::endl;

}

void MotionManager::finishToControl() {
	flagMotionControl = false;
}

void MotionManager::runThread() {
	mcThread = std::thread(&MotionManager::threadFunc, this);
}
void MotionManager::joinThread() {
	mcThread.join();
}

void MotionManager::threadFunc() {

	mc.InitMotionController();

	//data logger
	int numDataLog = 1000000;
	int cntDataLog = 0;
	LARGE_INTEGER curTime;
	std::vector<double> dataLog;
	dataLog.resize(numDataLog);
	//	std::ostringstream oss;

		//���O��flagSync��ێ�
	bool flagPrevSync = false;//sync�ɓ�����������init�����邽�߂����ɗ��p�����t���O

	//feed back system

	double phaseOffset = 0;//sync�J�n���ł�offset�B�ЂƂ�sync�����ʂ��Ĉ��
	double diffAngle = 0;//proj��act�̍���
	double prevDiffAngle = 0;//�O�񃋁[�v��diffAngle
	error = 0;//����̍���
	errorI = 0;//error�̗ݐϒl
	double targetSpeed = 0;


	//control loop
	while (flagMotionControl) {

		//�ϑ��l�̎擾�ƕϊ�
		ULONG ifc, ofc;
		
		projector.get_status(ifc, ofc);
		//		ULONG curNumProjection = projector.getOutputFrames();//�ڕW�l�ɑΉ�
		ULONG curNumProjection = ofc;//�ڕW�l�ɑΉ�
		ULONG curNumActuation = mc.GetCountPulse();//���䌋�ʂ̎����l�ɑΉ�
		const double pulsePerRotation = 1000.0;//1000*B/A[ppr]
		phaseProjector = 1.0 * (curNumProjection) / numProjPerRotation;//�萔�͂P���ɑΉ����铊�e�������猈��
		phaseActuator = 1.0 * (curNumActuation) / pulsePerRotation;//�萔�͂P�T�ɑΉ�����p���X�����猈�� 

		//�������[�h�̎��s�͂���
		if (flagSync) {
			//flagSync�ɓ��������񃋁[�v�̂�phaseOffset�̍X�V������Binit()�̖����B
			if (flagSync && !flagPrevSync) {
				phaseOffset = phaseProjector - phaseActuator;//todo
				diffAngle = (phaseProjector - phaseActuator) - phaseOffset;//�΍�
				error = diffAngle - prevDiffAngle;
				//debug
				std::cout << "phaseProjector: " << phaseProjector << std::endl;
				std::cout << "phaseActuator: " << phaseActuator << std::endl;
				std::cout << "phaseOffset: " << phaseOffset << std::endl;
				std::cout << "error: " << error << std::endl;
				std::cout << "targetSpeed: " << targetSpeed << std::endl;
				std::cout << std::endl;
			}
			//interactivePhase
			if (abs(interactivePhaseIncrement) > 0) {
				phaseOffset += interactivePhaseIncrement;
				interactivePhaseIncrement = 0;
			}

			//error�̍X�V
			diffAngle = (phaseProjector - phaseActuator) - phaseOffset;//�΍�
			error = diffAngle - prevDiffAngle;
			prevDiffAngle = diffAngle;
			errorI += error;
			//if (abs(error) > 1.0) error -= copysign(1.0, error);//�{������̂����Ă��͈͐���//�p�x����Ȃ��̂ŕs�v

			//PID control
			//double defaultSpeed = 11248;
			double Kp = 820;
			double Ki = 0.1;

			//range constraints
			double upBnd = 1.01;
			double lowBnd = 0.99;
			if (targetSpeed > defaultSpeed * upBnd) targetSpeed = defaultSpeed * upBnd;// range constraint //�����Ƃ��܂�����������Ǝv��
			if (targetSpeed < defaultSpeed * lowBnd) targetSpeed = defaultSpeed * lowBnd;
			targetSpeed = targetSpeed + Kp * error + Ki * errorI;//�������v�H
			//range constraints
			if (targetSpeed > defaultSpeed * upBnd) targetSpeed = defaultSpeed * upBnd;// range constraint //�����Ƃ��܂�����������Ǝv��
			if (targetSpeed < defaultSpeed * lowBnd) targetSpeed = defaultSpeed * lowBnd;
			if(targetSpeed != 10590) changeSpeedTo(targetSpeed, 0);//feedback�̂����������Ȃ��H�{������̂ƃp�������ɂȂ��Ă�H�H
			
		}

		//display

		//if (flagSync){ std::cout << "--SYNCHRONIZED CONTROL-- " << std::endl; }
		//else{ std::cout << "--FIXED CONTROL-- " << std::endl; }
		//std::cout << "phaseProjector: " << phaseProjector << std::endl;
		//std::cout << "phaseActuator: " << phaseActuator << std::endl;
		//std::cout << "phaseOffset: " << phaseOffset << std::endl;
		//std::cout << "error: " << error << std::endl;
		//std::cout << "targetSpeed: " << targetSpeed << std::endl;
		//std::cout << std::endl;

		//data logger
		if (cntDataLog < numDataLog) {
			//dataLog[cntDataLog++] << "#" << "normalizedNumProjection: " << "normalizedNumActuation: " << "absoluteError: " << "phaseProjector: " << "phaseActuator: " << "error: " << "targetSpeed: " << std::endl;
			QueryPerformanceCounter(&curTime);
			//std::cout << "cntDataLog: " << cntDataLog << std::endl;
			if (cntDataLog < numDataLog) dataLog[cntDataLog++] = curTime.QuadPart;//quadpart��long long int
			if (cntDataLog < numDataLog) dataLog[cntDataLog++] = flagSync ? 1 : 0;//synchronization
			if (cntDataLog < numDataLog) dataLog[cntDataLog++] = phaseProjector;
			if (cntDataLog < numDataLog) dataLog[cntDataLog++] = phaseActuator;
			if (cntDataLog < numDataLog) dataLog[cntDataLog++] = error;
			if (cntDataLog < numDataLog) dataLog[cntDataLog++] = targetSpeed;
			if (cntDataLog < numDataLog) dataLog[cntDataLog++] = 0;//sentinel
		}
		
		//while end
		flagPrevSync = flagSync;
		motFrame++;
	}


	//fstream for data logger
	std::cout << "--DATA LOGGING--" << std::endl;
	std::ofstream ofs;
	ofs.open("syncInfo.dat");
	const int numElements = 7;
	for (int idx = 0; idx < cntDataLog; idx++) {
		ofs << dataLog[idx] << ' ';
		if (idx % numElements == numElements - 1) ofs << std::endl;
	}
	ofs.flush();
	ofs.close();

	return;

}


void MotionManager::setProjPerRot(int ppr) {
	numProjPerRotation = ppr;
}

void MotionManager::setSpeedInRPS(double rps) {
	defaultSpeed = rps2pps(rps);
	assert(defaultSpeed > 0 && defaultSpeed < 16000);
	std::cout << "MotionManager: set new defaultSpeed: " << defaultSpeed << std::endl;
}

double MotionManager::rps2pps(double rps) {
	double A = 1000;
	double B = 360;
	return rps * 1000 * B / A;
}