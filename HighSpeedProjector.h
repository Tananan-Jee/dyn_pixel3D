#pragma once
#include "DynaFlash.h"
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdexcept>
#include <iostream>
#include <vector>
#include <string>
#include <assert.h>

#define EXTENSION_MODE                  /* 開発バージョンの拡張機能を使うためのdefine */
#define BOARD_INDEX			(0)			/* DynaFlash接続ボードのインデックス (0固定) */
#define ALLOC_FRAME_BUFFER	(16)		/* 確保するフレームバッファ数 */

//#define TED_MODE

#ifdef _DEBUG
#pragma comment( lib, "DynaFlash100d")
#else
#pragma comment( lib, "DynaFlash100")
#endif

//static char ini_buf[1024 * 768];			/* バイナリデータ読み込み先 */
//static char ini_buf_bin[1024 * 768 / 8];
//static char ini_buf_color[1024 * 768 * 3];			/* カラーデータ読み込み先 */

typedef enum class PROJECTION_PARAMETER {
	FRAME_RATE = 0,          // フレームレート[fps]
	BUFFER_MAX_FRAME = 1,          // バッファの最大フレーム数
								   /* ここから下は、PM::PATTERN_EMBEDをONにしないと設定不可能 */
								   PATTERN_OFFSET = 2,          // パターンの輝度[μs]
								   PROJECTOR_LED_ADJUST = 3,          // 各ビットごとにLEDの点灯時間を制御[10ns]
								   BIT_SEQUENCE = 4,          // ビットシーケンスの設定
								   SELF_MADE_SEQUENCE = 5,          // 本来ライブラリ内で決定する配列 SWCount, u32GRSTOffset の値
}PP;


class HighSpeedProjector
{
    /* public変数定義 */
public:
    const ULONG width;
    const ULONG height;

	 //char ini_buf[1024 * 768];			/* バイナリデータ読み込み先 */
	 //char ini_buf_bin[1024 * 768 / 8];
	 //char ini_buf_color[1024 * 768 * 3];			/* カラーデータ読み込み先 */

    /* private変数定義 */
private:

    /* プロジェクタ操作に必要なクラス */
    CDynaFlash *pDynaFlash;
    ULONG nFrameCnt;
    char *pbuffer;
    ULONG nProjectionMode;

    /* 各種パラメタ */
    ULONG frame_rate; /* 投影フレームレート[fps]*/
    ULONG valid_bits; /* 有効bitの数.基本は8bit */
    unsigned int projection_buffer_max_frame; /* DynaFalsh上のメモリを画像何枚分に制御するか */

                                              /* これより下のパラメタはパターン埋め込みモードをONにした時のみ使用可能 */
    ULONG pattern_offset; /* パターン埋め込みをする場合のオフセット */
    INT projector_led_adjust[20]; /* 各bitのそれぞれのLED点灯時間の制御[10ns]*/
    ULONG bit_sequence[8]; /* ビットシーケンス */

                           /* 自分でシーケンス制御したい場合に使う変数 */
    ULONG SWCount[8];
    ULONG u32GRSTOffset[20];

    /* public関数定義 */
public:

    /**************************************************************************************************************************
    * 関数名　　：HighSpeedProjector
    * 機能　　　：コンストラクタ。変数の初期化処理を行う
    * 引数　　　：なし
    * 返り値　　：なし
    ***************************************************************************************************************************/
    HighSpeedProjector(
    );

    /**************************************************************************************************************************
    * 関数名　　：~HighSpeedProjector
    * 機能　　　：デストラクタ。終了処理をする
    * 引数　　　：なし
    * 返り値　　：なし
    ***************************************************************************************************************************/
    ~HighSpeedProjector(
    );

    /**************************************************************************************************************************
    * 関数名　　：set_projection_mode
    * 機能　　　：投影モードの選択( 'PM::'から選択する )
    * 引数　　　：1つ
    * 返り値　　：なし
    ***************************************************************************************************************************/
    void set_projection_mode(
        // inputs
        const PROJECTION_MODE           in_projection_mode          // デフォルトから追加したい投影モードを'PM::'から選択
    );

	/**************************************************************************************************************************
	* 関数名　　：unset_projection_mode
	* 機能　　　：投影モードの選択( 'PM::'から選択する )
	* 引数　　　：1つ
	* 返り値　　：なし
	* 備考      ：2019/06/24 追加 by 田畑
	***************************************************************************************************************************/
	void unset_projection_mode(
		// inputs
		const PROJECTION_MODE           in_projection_mode          // 削除したい投影モードを'PM::'から選択
	);


    /**************************************************************************************************************************
    * 関数名　　：get_parameter_value
    * 機能　　　：プロジェクタに与えるべきパラメタの設定( フレームレート、ビットシーケンス、LEDの応答時間の調節、etc... )
    * 引数　　　：2つ
    * 返り値　　：なし
    ***************************************************************************************************************************/
    void set_parameter_value(
        // inputs
        const PROJECTION_PARAMETER      in_set_parameter_index,     // セットしたいパラメタを'PP::'から選択
        const ULONG                     in_set_parameter            // セットしたいパラメタの値
    );
    void set_parameter_value(
        // inputs
        const PROJECTION_PARAMETER      in_set_parameter_index,     // セットしたいパラメタを'PP::'から選択
        const std::vector< INT >      in_set_parameter            // セットしたいパラメタの値
    );
    void set_parameter_value(
        // inputs
        const PROJECTION_PARAMETER      in_set_parameter_index,     // セットしたいパラメタを'PP::'から選択
        const std::vector< ULONG >      in_set_parameter            // セットしたいパラメタの値
    );
    void set_parameter_value(
        // inputs
        const PROJECTION_PARAMETER		in_set_parameter_index,		// セットしたいパラメタを'PP::'から選択
        const ULONG*					in_SWCount,					// SWCount
        const ULONG*					in_u32GRSTOffset,			// u32GRSTOffset
        const int						in_SWCount_size,			// SWCount のサイズ
        const int						in_u32GRSTOffset_size		// u32GRSTOffset のサイズ
    );

	/**************************************************************************************************************************
	* 関数名　　：connect
	* 機能　　　：プロジェクタの接続
	* 引数　　　：device_index: 接続するプロジェクタのインデックス
	* 返り値　　：成功 true, 失敗 false
	* 備考      ：2019/06/21 追加 by 田畑
	* 　　      ：connect_projector/connect_projector_lightの共通部分統一, また, start処理と分離.
	***************************************************************************************************************************/
	bool connect(unsigned int device_index = 0);

	/**************************************************************************************************************************
	* 関数名　　：start
	* 機能　　　：プロジェクタの投影開始
	* 引数　　　：なし
	* 返り値　　：成功 true, 失敗 false
	* 備考      ：2019/06/21 追加 by 田畑
	* 　　      ：connect_projector/connect_projector_lightから, 投影開始指示分離. 参考: connect
	***************************************************************************************************************************/
	bool start();

	/**************************************************************************************************************************
	* 関数名　　：stop
	* 機能　　　：プロジェクタの投影終了
	* 引数　　　：なし
	* 返り値　　：成功 true, 失敗 false
	* 備考      ：2019/06/21 追加 by 田畑
	* 　　      ：startの対となる処理. 投影のみをとめる. パラメタの変更を可能にする.
	***************************************************************************************************************************/
	bool stop();

	/**************************************************************************************************************************
	* 関数名　　：disconnect
	* 機能　　　：プロジェクタの切断
	* 引数　　　：なし
	* 返り値　　：成功 true, 失敗 false
	* 備考      ：2019/06/21 追加 by 田畑
	* 　　      ：connectの対となる処理. destruct_projector_lightとの分離.
	***************************************************************************************************************************/
	bool disconnect();

    /**************************************************************************************************************************
    * 関数名　　：connect_projector
    * 機能　　　：プロジェクタの接続
    * 引数　　　：なし
    * 返り値　　：成功 true, 失敗 false
    ***************************************************************************************************************************/
    bool connect_projector(unsigned int device_index = 0
    );

    /**************************************************************************************************************************
    * 関数名　　：send_image
    * 機能　　　：投影映像の転送
    * 引数　　　：1つ
    * 返り値　　：なし
    ***************************************************************************************************************************/
    void send_image_24bit(
        // inputs
        const void                      *in_data                    // 転送したいデータのポインタ
    );
    void send_image_8bit(
        // inputs
        const void                      *in_data                    // 転送したいデータのポインタ
    );
    int send_image_binary(
        // inputs
        const void                      *in_data                       // 転送したいデータのポインタ
    );

    /**************************************************************************************************************************
    * 関数名　　：destruct_process_light
    * 機能　　　：投影を終了し、パラメタを変更できるようにする
    * 引数　　　：なし
    * 返り値　　：なし
    ***************************************************************************************************************************/
    bool destruct_process_light(
    );

    /**************************************************************************************************************************
    * 関数名　　：connnect_projector_light
    * 機能　　　：destruct_process_light()した後に、パラメタをセットし直し再び投影可能状態にする
    * 引数　　　：なし
    * 返り値　　：なし
    ***************************************************************************************************************************/
    bool connnect_projector_light(
    );

    /**************************************************************************************************************************
    * 関数名　　：convert_to_binary_row
    * 機能　　　：バイナリー画像を送信用のバイナリー列に変換
    * 引数　　　：1つ
    * 返り値　　：なし
    * 製作者　　：Takuya Kadowaki
    ***************************************************************************************************************************/
    void convert_to_binary_row(
        // inputs
        const void                      *in_data,                 // 変換したいデータのポインタ,binary画像
                                                                  // outputs
        unsigned char                   *out_data                 // 出力したデータのポインタ,unsigned char型1024*768/8
    );


	/**************************************************************************************************************************
	* 関数名　　：get_led_color_balance
	* 機能　　　：DynaFlash v2(カラー版)におけるカラーバランスの確認
	* 引数　　　：なし
	* 返り値　　：なし（結果はcoutで表示される）
	* 備考      ：2019/06/25 追加 by 田畑
	* 　　      ：遠藤プロジェクト(DDCM_MultiView)を参考に導入.
	***************************************************************************************************************************/
	void get_led_color_balance(
	);

	/**************************************************************************************************************************
	* 関数名　　：set_led_color_balance
	* 機能　　　：DynaFlash v2(カラー版)におけるカラーバランスの調整
	* 引数　　　：6つ
	* 返り値　　：なし
	* 備考      ：2019/06/25 追加 by 田畑
	* 　　      ：遠藤プロジェクト(DDCM_MultiView)を参考に導入.
	***************************************************************************************************************************/
	void set_led_color_balance(
		ULONG ConvR1,
		ULONG ConvR2,
		ULONG ConvG1,
		ULONG ConvG2,
		ULONG ConvB1,
		ULONG ConvB2
	);

	/**************************************************************************************************************************
	* 関数名　　：set_led_color_balance_TEDsample
	* 機能　　　：DynaFlash v2(カラー版)におけるカラーバランスの調整. TED提供のサンプル数値
	* 引数　　　：1つ
	* 返り値　　：なし
	* 備考      ：2019/06/25 追加 by 田畑
	* 　　      ：電流値-framerate.txtの数値を流用
	* 　　      ：modeは24bit用の 1:1000fps, 2:2000fps, 3:2840fpsと書かれていたデータ. 21bit 3200fpsは保留.
	***************************************************************************************************************************/
	void set_led_color_balance_TEDsample(
		int mode
	);

    /**************************************************************************************************************************
* 関数名　　：get_status
* 機能　　　：何枚画像を転送したか、何枚画像が投影されたかを取得
* 引数　　　：1つ
* 返り値　　：なし
***************************************************************************************************************************/
    void get_status(
        // outputs
        ULONG& out_input_frame_count,                                    // DynaFlashへ転送完了したデータ数
        ULONG& out_output_frame_count                                    // DynaFlashが投影完了したデータ数
    );

    /* private関数定義 */
private:
    /**************************************************************************************************************************
    * 関数名　　：print_all_mode
    * 機能　　　：すべての設定されたモードの表示
    * 引数　　　：なし
    * 返り値　　：なし
    ***************************************************************************************************************************/
    void print_all_mode(
    );



    /**************************************************************************************************************************
    * 関数名　　：print_version
    * 機能　　　：各種バージョンの表示
    * 引数　　　：1つ
    * 返り値　　：なし
    ***************************************************************************************************************************/
    void print_version(
        // inputs
        CDynaFlash                       *pDynaFlash                // 現在作成しているDynaFlashクラス                         
    );

    /**************************************************************************************************************************
    * 関数名　　：destruct_process
    * 機能　　　：投影終了時のデストラクト処理
    * 引数　　　：なし
    * 返り値　　：なし
    ***************************************************************************************************************************/
    void destruct_process(
    );

    /**************************************************************************************************************************
    * 関数名　　：parameter_reset
    * 機能　　　：セットしていたパラメタのリセット
    * 引数　　　：なし
    * 返り値　　：なし
    ***************************************************************************************************************************/
    void parameter_reset(
    );

};