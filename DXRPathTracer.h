#pragma once
#include "../util/RTUtility/dxHelpers.h"
#include "../util/IGRTframe/IGRTTracer.h"
#include "../util/IGRTframe/Camera.h"
#define NextAlignedLine __declspec(align(16))

// This structure will be bind to dxr shader directly via [global] root signature. 
struct GloabalContants
{
	NextAlignedLine
		float3 backgroundLight;
	NextAlignedLine
		float3 cameraPos;
	NextAlignedLine
		float3 cameraX;
	NextAlignedLine
		float3 cameraY;
	NextAlignedLine
		float3 cameraZ;
	NextAlignedLine
		float3 eyePos;
	NextAlignedLine
		float2 cameraAspect;
	float rayTmin = 1e-4f;
	float rayTmax = 1e27f;
	uint type = 0;
	uint accumulatedFrames;
	uint resDepth;
	NextAlignedLine
		uint4 mtlResDepth;
	uint maxPathLength;
	uint time;
};


// This structure will be bind to dxr shader directly via [local] root signature.
struct ObjectContants
{
	uint objectIdx;
};

static const uint xxx = sizeof(GloabalContants);
static const uint identifierSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
static const uint hitGroupRecordSize = _align(
	identifierSize + (uint) sizeof(ObjectContants), D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);


union HitGroupRecord
{
	struct {
		ShaderIdentifier shaderIdentifier;
		ObjectContants objConsts;
	};
private:
	char pad[hitGroupRecordSize];
};


class Scene;
class InputEngine;
class DXRPathTracer : public IGRTTracer
{
	DXGI_FORMAT			tracerOutFormat = DXGI_FORMAT_R8_UNORM;
	static const uint					recordSize = hitGroupRecordSize;

	ID3D12Device* mDevice;
	ID3D12CommandQueue* mCmdQueue;
	ID3D12GraphicsCommandList4* mCmdList;
	ID3D12CommandAllocator* mCmdAllocator;

	ID3D12CommandQueue* mComputeCmdQueue;
	ID3D12GraphicsCommandList4* mComputeCmdList;
	ID3D12CommandAllocator* mComputeCmdAllocator;

	ID3D12CommandQueue* mCopyCmdQueue;
	ID3D12GraphicsCommandList4* mCopyCmdList;
	ID3D12CommandAllocator* mCopyCmdAllocator;

	BinaryFence							mFence[5];
	void initD3D12(int adapterId);

	DescriptorHeap						mSrvUavHeap;

	RootSignature						mGlobalRS;
	RootSignature						mHitGroupRS;
	void declareRootSignatures();

	dxShader							dxrLib;
	RaytracingPipeline					mRtPipeline;
	void buildRaytracingPipeline();

	GloabalContants						mGlobalConstants;
	UploadBuffer						mGlobalConstantsBuffer;
	UploadBuffer						mSceneUploadBuffer;
	UploadBuffer						mTextureBuffer;
	UnorderAccessBuffer					mTracerOutBuffer;
	const int patternBuffNum = 10;
	int patternBuffIdx = 0;
	UnorderAccessBuffer					mTracerPatternBuffer[10];
	ReadbackBuffer						mReadBackBuffer;
	void initializeApplication();
	//------Until here, scene independent members-------------------------//

	OrbitCamera camera;
	OrbitCamera eye[3];

	//------From now, scene dependent members-----------------------------//
	DefaultBuffer						mSceneObjectBuffer;
	DefaultBuffer						mVertexBuffer;
	DefaultBuffer						mTridexBuffer;
	DefaultBuffer						mCdfBuffer;			// Now not use.
	DefaultBuffer						mTransformBuffer;	// Now not use.
	DefaultBuffer						mMaterialBuffer;	// Now not use.

	DefaultTexture						mTexture;

	ShaderTable							mShaderTable;
	void setupShaderTable();

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags =
		// D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
		// D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE | D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
	AccelerationStructureBuildMode		buildMode =
		//ONLY_ONE_BLAS;
		//BLAS_PER_OBJECT_AND_BOTTOM_LEVEL_TRANSFORM;
		BLAS_PER_OBJECT_AND_TOP_LEVEL_TRANSFORM;
	const int ASNum = 5;
	int nowAS = 0;
	dxAccelerationStructure				mAccelerationStructure[5];
	void initAccelerationStructure();
	void buildAccelerationStructure();
	//void buildAccelerationStructure1();

public:
	int sceneFlag = true;
	const uint resDepth = 90;

	~DXRPathTracer();
	DXRPathTracer(uint width, uint height, DXGI_FORMAT format, int adapterId);
	virtual void onSizeChanged(uint width, uint height);
	virtual void update(const InputEngine& input);
	virtual int shootRays();
	int resetPatternResult();
	virtual int getResult(void* result);
	int getPatternResult(void* result);
	virtual void setupScene(const Scene* scene);
	void update();
	void update(int seed);
	void updateCamera(const InputEngine& input);
	void updateEye(const InputEngine& input, int idx = 0);
	void updateCamera(MouseEvent& input);
	void updateEye(MouseEvent& input, int idx = 0);
	void updateEye(const float3 input, int idx = 0);
	void resetEye(int idx = 0);
	void updateImg(cv::Mat input);
	void updateScene(const Scene* scene);
	void updateMaterial(int mtlId);
	void setType(int type);
	void setMtlResDepth(int type, int res);
	OrbitCamera getCamera();
	void setCamera(OrbitCamera cam);
};
