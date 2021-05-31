#include "HighSpeedProjector.h"


HighSpeedProjector::HighSpeedProjector()
    :width(1024),
    height(768),
    pDynaFlash(nullptr),
    nFrameCnt(0),
    pbuffer(nullptr),
    nProjectionMode(0),
    frame_rate(1000),
    valid_bits(8),
    pattern_offset(0),
    projection_buffer_max_frame(0)
{
    /* DynaFlashクラスのインスタンス生成とそれぞれの変数の初期化 */
    for (int i = 0; i < 20; i++) projector_led_adjust[i] = 0;
    for (int i = 0; i < 8; i++) bit_sequence[i] = i;
    pDynaFlash = CreateDynaFlash();
    if (pDynaFlash == nullptr) throw (std::runtime_error("Error: DynaFlashクラス生成失敗"));
}

HighSpeedProjector::~HighSpeedProjector()
{
    destruct_process();
}

void HighSpeedProjector::destruct_process()
{
	if (pDynaFlash) {
		stop();
		disconnect();
		/* DynaFlashクラスのインスタンス破棄 */
		ReleaseDynaFlash(&pDynaFlash);
		pDynaFlash = nullptr;
	}
}

bool HighSpeedProjector::connect_projector(unsigned int device_index)
{
	std::cout << "bool HighSpeedProjector::connect_projector(unsigned int device_index) " << std::endl;
	std::cout << "This method is deprecated. Please use connect(unsigned int device_index) and start()" << std::endl;
	if (connect(device_index)) {
		return start();
	}
	return false;
}


void HighSpeedProjector::send_image_24bit(const void *data)
{
    try
    {
        for (;;)
        {
            /* 投影データ更新可能な投影用フレームバッファ取得 */
           
            if (pDynaFlash->GetFrameBuffer(&pbuffer, &nFrameCnt) != STATUS_SUCCESSFUL)
            {
                throw (static_cast< int >(__LINE__));
            }
            if ((pbuffer != nullptr) && (nFrameCnt != 0))
            {
                /* 必要であればここにフレームバッファの更新処理を記載する */
                ULONG input_frame_tmp = 0;
                ULONG output_frame_tmp = 0;
                get_status(input_frame_tmp, output_frame_tmp);
                auto diff = static_cast< unsigned int >(input_frame_tmp - output_frame_tmp);
                if (diff <= projection_buffer_max_frame)
                {
                    std::memcpy(pbuffer, data, FRAME_BUF_SIZE_24BIT);
                    /* DynaFlashへ転送する */
                    if (pDynaFlash->PostFrameBuffer(1) != STATUS_SUCCESSFUL) throw (static_cast< int >(__LINE__));
                    break;
                }
            }
        }
    }
    catch (int line)
    {
        std::cout << "Error: line_number = " << line << std::endl;
        destruct_process();
    }
}

void HighSpeedProjector::send_image_8bit(const void *data)
{
    try
    {
        for (;;)
        {
            /* 投影データ更新可能な投影用フレームバッファ取得 */
            if (pDynaFlash->GetFrameBuffer(&pbuffer, &nFrameCnt) != STATUS_SUCCESSFUL)
            {
                throw (static_cast< int >(__LINE__));
            }
            if ((pbuffer != nullptr) && (nFrameCnt != 0))
            {
                /* 必要であればここにフレームバッファの更新処理を記載する */
                ULONG input_frame_tmp = 0;
                ULONG output_frame_tmp = 0;
                get_status(input_frame_tmp, output_frame_tmp);
                auto diff = static_cast< unsigned int >(input_frame_tmp - output_frame_tmp);
                if (diff <= projection_buffer_max_frame)
                {
                    std::memcpy(pbuffer, data, FRAME_BUF_SIZE_8BIT);
                    /* DynaFlashへ転送する */
                    if (pDynaFlash->PostFrameBuffer(1) != STATUS_SUCCESSFUL) throw (static_cast< int >(__LINE__));
                    break;
                }
            }
        }
    }
    catch (int line)
    {
        std::cout << "Error: line_number = " << line << std::endl;
        destruct_process();
    }
}

int HighSpeedProjector::send_image_binary(const void *data)
{
    try
    {
            /* 投影データ更新可能な投影用フレームバッファ取得 */
            if (pDynaFlash->GetFrameBuffer(&pbuffer, &nFrameCnt) != STATUS_SUCCESSFUL) {
                throw std::runtime_error("Failed to GetFrameBuffer");
            }
            /* 必要であればここにフレームバッファの更新処理を記載する */
            if ((pbuffer != nullptr) && (nFrameCnt != 0))
            {
                memcpy(pbuffer, data, FRAME_BUF_SIZE_BINARY);
                /* DynaFlashへ転送する */
                if (pDynaFlash->PostFrameBuffer(1) != STATUS_SUCCESSFUL) { 
                    throw std::runtime_error("Failed to PostFrameBuffer");
                }
                return 1;
            }
            return 0;  
    }
    catch (int line)
    {
        std::cout << "Error: line_number = " << line << std::endl;
        destruct_process();
    }
}

void HighSpeedProjector::print_version(CDynaFlash *pDynaFlash)
{
    char DriverVersion[40];
    unsigned long nVersion;

    /* ドライババージョン取得 */
    pDynaFlash->GetDriverVersion(DriverVersion);
    printf("DRIVER Ver : %s\r\n", DriverVersion);

    /* HWバージョン取得 */
    pDynaFlash->GetHWVersion(&nVersion);
    printf("HW Ver     : %08x\r\n", nVersion);

    /* DLLバージョン取得 */
    pDynaFlash->GetDLLVersion(&nVersion);
    printf("DLL Ver    : %08x\r\n", nVersion);
}

void HighSpeedProjector::get_status(ULONG &out_input_frame_count, ULONG &out_output_frame_count)
{
    DYNAFLASH_STATUS pDynaFlashStatus;
    pDynaFlash->GetStatus(&pDynaFlashStatus);
    out_input_frame_count = pDynaFlashStatus.InputFrames;
    out_output_frame_count = pDynaFlashStatus.OutputFrames;
}

void HighSpeedProjector::set_projection_mode(PROJECTION_MODE projection_mode)
{
    nProjectionMode |= static_cast< ULONG >(projection_mode);
}

void HighSpeedProjector::unset_projection_mode(PROJECTION_MODE projection_mode)
{
	nProjectionMode &= ~(static_cast< ULONG >(projection_mode));
}


void HighSpeedProjector::set_parameter_value(const PROJECTION_PARAMETER in_set_parameter_index, const ULONG in_set_parameter)
{
    switch (in_set_parameter_index)
    {
    case PP::FRAME_RATE:
    {
        frame_rate = in_set_parameter;
        break;
    }
    case PP::BUFFER_MAX_FRAME:
    {
        projection_buffer_max_frame = in_set_parameter;
        break;
    }
    case PP::PATTERN_OFFSET:
    {
        assert((nProjectionMode & static_cast< ULONG >(PM::PATTERN_EMBED)) != 0);
        pattern_offset = in_set_parameter;
        break;
    }
    }
}

bool HighSpeedProjector::destruct_process_light()
{
	std::cout << "bool HighSpeedProjector::destruct_process_light() " << std::endl;
	std::cout << "This method is deprecated. Please use parameter_reset() and stop()" << std::endl;
    /* セットしていたパラメタのリセット */
    parameter_reset();
    return stop();
}

bool HighSpeedProjector::connnect_projector_light()
{
	std::cout << "bool HighSpeedProjector::connnect_projector_light() " << std::endl;
	std::cout << "This method is deprecated. Please use start()" << std::endl;
	return start();
}

void HighSpeedProjector::set_parameter_value(const PROJECTION_PARAMETER in_set_parameter_index, const std::vector< ULONG > in_set_parameter)
{
    assert((nProjectionMode & static_cast< ULONG >(PM::PATTERN_EMBED)) != 0);
    assert(in_set_parameter.size() == 8u);
    for (int i = 0; i < in_set_parameter.size(); i++) bit_sequence[i] = in_set_parameter[i];
}

void HighSpeedProjector::set_parameter_value(const PROJECTION_PARAMETER in_set_parameter_index, const std::vector< INT > in_set_parameter)
{
    assert((nProjectionMode & static_cast< ULONG >(PM::PATTERN_EMBED)) != 0);
    for (int i = 0; i < in_set_parameter.size(); i++) projector_led_adjust[i] = in_set_parameter[i];
}

void HighSpeedProjector::set_parameter_value(const PROJECTION_PARAMETER in_set_parameter_index, const ULONG* in_SWCount, const ULONG* in_u32GRSTOffset, const int in_SWCount_size, const int in_u32GRSTOffset_size)
{
    assert(in_set_parameter_index == PP::SELF_MADE_SEQUENCE);
    ULONG sum_SWCount = 0, sum_u32GRSTOffset = 0;
    for (int i = 0; i < in_SWCount_size; ++i) {
        sum_SWCount += in_SWCount[i];
        SWCount[i] = in_SWCount[i];
    }
    for (int i = 0; i < in_u32GRSTOffset_size; ++i) {
        sum_u32GRSTOffset += in_u32GRSTOffset[i];
        u32GRSTOffset[i] = in_u32GRSTOffset[i];
    }
    assert(sum_SWCount == sum_u32GRSTOffset);
}

void HighSpeedProjector::print_all_mode()
{
    if ((nProjectionMode & static_cast< ULONG >(PM::MIRROR)) != 0) std::cout << "MIRROR MODE : ON" << std::endl;
    else std::cout << "MIRROR MODE : OFF" << std::endl;
    if ((nProjectionMode & static_cast< ULONG >(PM::FLIP)) != 0) std::cout << "FLIP MODE : ON" << std::endl;
    else std::cout << "FLIP MODE : OFF" << std::endl;
    if ((nProjectionMode & static_cast< ULONG >(PM::COMP)) != 0) std::cout << "COMP MODE : ON" << std::endl;
    else std::cout << "COMP MODE : OFF" << std::endl;
    if ((nProjectionMode & static_cast< ULONG >(PM::ONESHOT)) != 0) std::cout << "ONESHOT MODE : ON" << std::endl;
    else std::cout << "ONESHOT MODE : OFF" << std::endl;
    if ((nProjectionMode & static_cast< ULONG >(PM::BINARY)) != 0) std::cout << "BINARY MODE : ON" << std::endl;
    else std::cout << "BINARY MODE : OFF" << std::endl;
	if ((nProjectionMode & static_cast< ULONG >(PM::COLOR)) != 0) std::cout << "COLOR MODE : ON" << std::endl;
	else std::cout << "COLOR MODE : OFF" << std::endl;
	if ((nProjectionMode & static_cast< ULONG >(PM::EXT_TRIGGER)) != 0) std::cout << "EXT_TRIGGER MODE : ON" << std::endl;
    else std::cout << "EXT_TRIGGER MODE : OFF" << std::endl;
    if ((nProjectionMode & static_cast< ULONG >(PM::TRIGGER_SKIP)) != 0) std::cout << "TRIGGER_SKIP : ON" << std::endl;
    else std::cout << "TRIGGER_SKIP : OFF" << std::endl;
    if ((nProjectionMode & static_cast< ULONG >(PM::PATTERN_EMBED)) != 0) std::cout << "PATTERN_EMBED MODE : ON" << std::endl;
    else std::cout << "PATTERN_EMBED MODE: OFF" << std::endl;
    if ((nProjectionMode & static_cast< ULONG >(PM::ILLUMINANCE_HIGH)) != 0) std::cout << "ILLUMINANCE_HIGH MODE : ON" << std::endl;
    else std::cout << "ILLUMINANCE_HIGH MODE : OFF" << std::endl;
    std::cout << "FRAME_RATE = " << frame_rate << " [fps] " << std::endl;
    if (projection_buffer_max_frame == 0) std::cout << "BUFFER_MAX_FRAME : No set" << std::endl;
    else std::cout << "BUFFER_MAX_FRAME = " << projection_buffer_max_frame << " [frame] " << std::endl;
    if (pattern_offset == 0) std::cout << "PATTERN_OFFSET : No set" << std::endl;
    else std::cout << "PATTERN_OFFSET = " << pattern_offset << " [us] " << std::endl;
    std::cout << "BIT_SEQUENCE = [ ";
    for (auto tmp : bit_sequence) std::cout << tmp << ", ";
    std::cout << " ] " << std::endl;
    std::cout << "PROJECTOR_LED_ADJUST = [ ";
    for (int i = 0; i < 8; i++) std::cout << projector_led_adjust[i] << ", ";
    std::cout << " ] " << std::endl;
}


void HighSpeedProjector::parameter_reset()
{
    pbuffer = nullptr;
    nProjectionMode = 0;
    frame_rate = 1000;
    valid_bits = 8;
    pattern_offset = 0;
    projection_buffer_max_frame = 0;
}

void HighSpeedProjector::convert_to_binary_row(const void*in_data, unsigned char *out_data)
{
    //void->unsigned charへの返還変換
    auto ptr = ((unsigned char *)in_data);

    //以下でバイナリー列に変換
    unsigned char tmp = (unsigned char)0;//8bit列

    for (int i = 0; i<FRAME_BUF_SIZE_BINARY; i++) {
        for (int j = 0; j<8; j++) {
            tmp = tmp << 1;
            tmp += ((unsigned char *)in_data)[8 * i + j] != 0;
        }
        out_data[i] = tmp;
        tmp = (unsigned char)0;
    }
}


bool HighSpeedProjector::connect(unsigned int device_index) {
	if (pDynaFlash == nullptr)return false;
	try
	{
		/* ワーキングセット設定 */
		if (!SetProcessWorkingSetSize(::GetCurrentProcess(), (1000UL * 1024 * 1024), (2000UL * 1024 * 1024))) throw (static_cast<int>(__LINE__));

		/* DynaFlash接続 */
		if (pDynaFlash->Connect(device_index) != STATUS_SUCCESSFUL) throw (static_cast<int>(__LINE__));
		if (pDynaFlash->Reset() != STATUS_SUCCESSFUL) throw (static_cast<int>(__LINE__));

		/* 各種バージョン表示 */
		print_version(pDynaFlash);

		return true;
	}
	catch (int line)
	{
		std::cout << "Error: line_number = " << line << std::endl;
		destruct_process();
		return false;
	}
}

bool HighSpeedProjector::start() {
	if (pDynaFlash == nullptr)return false;
#ifndef EXTENSION_MODE
	try
	{
		if (!(nProjectionMode & static_cast< ULONG >(PM::BINARY)) == 0 && (nProjectionMode & static_cast< ULONG >(PM::EXT_TRIGGER)) == 0 && (nProjectionMode & static_cast< ULONG >(PM::TRIGGER_SKIP)) == 0) throw (static_cast< int >(__LINE__));
	}
	catch (int line)
	{
		std::cout << "Error: line nmber = " << line << std::endl;
		std::cout << "拡張機能を使用したいときは'EXTENSION_MODE'をHighSpeedProjector.hの一番上に#defineしてください" << std::endl;
		std::cout << "※ただし、製品版では使用できません" << std::endl;
		assert(0);
	}
#endif
	assert(((nProjectionMode & static_cast< ULONG >(PM::BINARY)) != 0 && (nProjectionMode & static_cast< ULONG >(PM::PATTERN_EMBED)) != 0) == 0);
	try
	{
		print_all_mode();
		/* DynaFlashリセット */
		if (pDynaFlash->Reset() != STATUS_SUCCESSFUL) throw (static_cast< int >(__LINE__));

		if ((nProjectionMode & static_cast< ULONG >(PM::COLOR)) != 0) // COLORである
		{
			valid_bits = 24u;
		}

		/* 投影パラメーター設定 */
		if ((nProjectionMode & static_cast< ULONG >(PM::PATTERN_EMBED)) == 0) // EMBEDでない
		{
			if ((nProjectionMode & static_cast< ULONG >(PM::SELF_SEQUENCE)) == 0) // SELF_SEQUENCEでない
			{
				if (pDynaFlash->SetParam(frame_rate, valid_bits, nProjectionMode) != STATUS_SUCCESSFUL) throw (static_cast< int >(__LINE__));
			}
			else // SELF_SEQUENCEである
			{
				// SELF_SEQUENCE の COLOR は有効とのことなので, 有効化.
				//if ((nProjectionMode & static_cast<ULONG>(PM::COLOR)) != 0) { throw (static_cast< int >(__LINE__)); } // SELF_SEQUENCEかつCOLORは未定義とする.
#ifndef TED_MODE
				if (pDynaFlash->SetParam(frame_rate, valid_bits, nProjectionMode, SWCount, u32GRSTOffset) != STATUS_SUCCESSFUL) throw (static_cast< int >(__LINE__));
#else
				throw (static_cast< int >(__LINE__));
#endif
			}
		}
		else // EMBEDである
		{
#ifndef TED_MODE
			if (pDynaFlash->SetParam(frame_rate, valid_bits, bit_sequence, pattern_offset, projector_led_adjust, nProjectionMode) != STATUS_SUCCESSFUL) throw (static_cast< int >(__LINE__));
#else
			throw (static_cast< int >(__LINE__));
#endif
		}

		/* 照度設定 */
		if ((nProjectionMode & static_cast< ULONG >(PM::ILLUMINANCE_HIGH)) == 0) {
			if (pDynaFlash->SetIlluminance(LOW_MODE) != STATUS_SUCCESSFUL) throw (static_cast< int >(__LINE__));
		}
		else {
			if (pDynaFlash->SetIlluminance(HIGH_MODE) != STATUS_SUCCESSFUL) throw(static_cast< int >(__LINE__));
		}

		/* 投影用フレームバッファ取得 */
		if (pDynaFlash->AllocFrameBuffer(ALLOC_FRAME_BUFFER) != STATUS_SUCCESSFUL) throw (static_cast< int >(__LINE__));

		/* 投影データ更新可能な投影用フレームバッファ取得 */
		int bufCheck = pDynaFlash->GetFrameBuffer(&pbuffer, &nFrameCnt);
		if (bufCheck != STATUS_SUCCESSFUL || (pbuffer == nullptr)) throw (static_cast< int >(__LINE__));

		/* 投影データ初期化 */
		if ((nProjectionMode & static_cast< ULONG >(PM::BINARY)) != 0)std::fill(pbuffer, pbuffer + FRAME_BUF_SIZE_BINARY * ALLOC_FRAME_BUFFER, 0);
		else if ((nProjectionMode & static_cast< ULONG >(PM::COLOR)) != 0)std::fill(pbuffer, pbuffer + FRAME_BUF_SIZE_8BIT * ALLOC_FRAME_BUFFER, 0);
		else std::fill(pbuffer, pbuffer + FRAME_BUF_SIZE_8BIT * ALLOC_FRAME_BUFFER, 0);
		//for (int i = 0; i < ALLOC_FRAME_BUFFER; i++)
		//        {
		//            if ((nProjectionMode & static_cast< ULONG >(PM::BINARY)) != 0)
		//            {
		//				//memcpy(pbuffer, ini_buf, FRAME_BUF_SIZE_BINARY);
		//				std::fill(pbuffer, pbuffer + FRAME_BUF_SIZE_BINARY, 0);
		//                pbuffer += FRAME_BUF_SIZE_BINARY;
		//            }
		//            else if ((nProjectionMode & static_cast< ULONG >(PM::COLOR)) != 0)
		//			{
		//				//memcpy(pbuffer, ini_buf_color, FRAME_BUF_SIZE_24BIT);
		//				std::fill(pbuffer, pbuffer + FRAME_BUF_SIZE_24BIT, 0);
		//				pbuffer += FRAME_BUF_SIZE_24BIT;
		//			}
		//			else
		//            {
		//				//memcpy(pbuffer, ini_buf_bin, FRAME_BUF_SIZE_8BIT);
		//				std::fill(pbuffer, pbuffer + FRAME_BUF_SIZE_8BIT, 0);
		//                pbuffer += FRAME_BUF_SIZE_8BIT;
		//            }
		//        }

		/* 投影開始 */
		if (pDynaFlash->Start() != STATUS_SUCCESSFUL) throw (static_cast<int>(__LINE__));
		return true;
	}
	catch (int line)
	{
		std::cout << "Error: line_number = " << line << std::endl;
		destruct_process();
		return false;
	}
}

bool HighSpeedProjector::stop() {
	if (pDynaFlash) {
		/* 投影終了 */
		if (pDynaFlash->Stop() != STATUS_SUCCESSFUL) return false;
		/* フレームバッファの破棄 */
		if (pDynaFlash->ReleaseFrameBuffer() != STATUS_SUCCESSFUL) return false;
		/* ミラーデバイスをフロート状態にする */
		if (pDynaFlash->Float(0) != STATUS_SUCCESSFUL) return false;
	}
	return true;
}

bool HighSpeedProjector::disconnect() {
	if (pDynaFlash) {
		/* DynaFlash切断 */
		if (pDynaFlash->Disconnect() != STATUS_SUCCESSFUL) return false;
	}
	return true;
}

void HighSpeedProjector::get_led_color_balance() {
	std::cout << "void HighSpeedProjector::get_led_color_balance()" << std::endl;
	if ((nProjectionMode & static_cast<ULONG>(PM::COLOR)) != 0) {
		ULONG iDaValue = 0;
		UINT iRet = 0;

		// 以降の数値出力を 16 進数で行います。
		std::cout.setf(std::ios::hex, std::ios::basefield);
		for (int i = 0; i < 6; ++i) {
			iRet = pDynaFlash->ReadDACRegister(i, &iDaValue);
			std::cout << "readDAC: " << i << "th: " << iDaValue << std::endl;
		}

		std::cout.setf(std::ios::dec, std::ios::basefield);
	}
	else {
		std::cout << "This method is available with only Color Projector(DynaFlash v2)." << std::endl;
		std::cout << "Please call set_projection_mode(PM::COLOR) before using this method." << std::endl;
	}
}

void HighSpeedProjector::set_led_color_balance(ULONG ConvR1, ULONG ConvR2, ULONG ConvG1, ULONG ConvG2, ULONG ConvB1, ULONG ConvB2) {
	std::cout << "void HighSpeedProjector::set_led_color_balance" << std::endl;
	if ((nProjectionMode & static_cast<ULONG>(PM::COLOR)) != 0) {
		UINT iRet = 0;
		iRet = pDynaFlash->WriteDACRegister(0, ConvR1);
		iRet = pDynaFlash->WriteDACRegister(1, ConvR2);
		iRet = pDynaFlash->WriteDACRegister(2, ConvG1);
		iRet = pDynaFlash->WriteDACRegister(3, ConvG2);
		iRet = pDynaFlash->WriteDACRegister(4, ConvB1);
		iRet = pDynaFlash->WriteDACRegister(5, ConvB2);
	}
	else {
		std::cout << "This method is available with only Color Projector(DynaFlash v2)." << std::endl;
		std::cout << "Please call set_projection_mode(PM::COLOR) before using this method." << std::endl;
	}
}

void HighSpeedProjector::set_led_color_balance_TEDsample(int mode) {
	std::cout << "void HighSpeedProjector::set_led_color_balance_TEDsample" << std::endl;
	if (mode < 1 || mode > 3) {
		std::cout << "out of range." << std::endl;
	}
	else {
		switch (mode) {
		case 1:
			std::cout << "set for 1000fps" << std::endl;
			set_led_color_balance(0x300, 0x280, 0x3A0, 0x280, 0x3D0, 0x280);
			break;
		case 2:
			std::cout << "set for 2000fps" << std::endl;
			set_led_color_balance(0x3B0, 0x280, 0x400, 0x280, 0x400, 0x240);
			break;
		case 3:
			std::cout << "set for 2840fps" << std::endl;
			set_led_color_balance(0x400, 0x280, 0x400, 0x230, 0x430, 0x220);
			break;
		defalut:
			std::cout << "out of range" << std::endl;
			break;
		}
	}
	get_led_color_balance();
}