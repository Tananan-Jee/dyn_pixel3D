#include "../util/pch.h"
#include "../util/IGRTframe/Camera.h"
#include "../util/IGRTframe/Scene.h"
#include "DXRPathTracer.h"

namespace DescriptorID {
	enum {
		// First RootParameter
		outUAV = 0,
		patternUAV = 1,
		patternUAV_ = 2,

		// Third RootParameter
		sceneObjectBuff = 7,
		vertexBuff = 8,
		tridexBuff = 9,
		materialBuff = 10,
		cdfBuff = 11,
		transformBuff = 12,

		inTex = 13,

		// Not used since we use RootPointer instead of RootTable
		accelerationStructure = 14,

		maxDesciptors = 32
	};
}

namespace RootParamID {
	enum {
		tableForOutBuffer = 0,
		tableForPatternBuffer,
		tableForPatternBuffer_,
		pointerForAccelerationStructure,
		tableForGeometryInputs,
		pointerForGlobalConstants,
		inTex,
		numParams
	};
}

namespace HitGroupParamID {
	enum {
		constantsForObject = 0,
		numParams = 1
	};
}

DXRPathTracer::~DXRPathTracer()
{
	SAFE_RELEASE(mCmdQueue);
	SAFE_RELEASE(mCmdList);
	SAFE_RELEASE(mCmdAllocator);
	SAFE_RELEASE(mDevice);
}

DXRPathTracer::DXRPathTracer(uint width, uint height, DXGI_FORMAT format, int adapterId)
	: IGRTTracer(width, height), tracerOutFormat(format)
{
	initD3D12(adapterId);

	mSrvUavHeap.create(DescriptorID::maxDesciptors);

	declareRootSignatures();

	buildRaytracingPipeline();

	initializeApplication();

	//mFence.waitCommandQueue(mCmdQueue);
}

void DXRPathTracer::initD3D12(int adapterId)
{
	mDevice = (ID3D12Device*)createDX12Device(getRTXAdapter());


	// graphic commandLine
	D3D12_COMMAND_QUEUE_DESC cqDesc = {};
	cqDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	ThrowFailedHR(mDevice->CreateCommandQueue(&cqDesc, IID_PPV_ARGS(&mCmdQueue)));

	ThrowFailedHR(mDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&mCmdAllocator)));

	ThrowFailedHR(mDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mCmdAllocator, nullptr, IID_PPV_ARGS(&mCmdList)));

	ThrowFailedHR(mCmdList->Close());
	ThrowFailedHR(mCmdAllocator->Reset());
	ThrowFailedHR(mCmdList->Reset(mCmdAllocator, nullptr));

	// compute commandLine
	D3D12_COMMAND_QUEUE_DESC cqCoDesc = {};
	cqCoDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;

	ThrowFailedHR(mDevice->CreateCommandQueue(&cqCoDesc, IID_PPV_ARGS(&mComputeCmdQueue)));

	ThrowFailedHR(mDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&mComputeCmdAllocator)));

	ThrowFailedHR(mDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, mComputeCmdAllocator, nullptr, IID_PPV_ARGS(&mComputeCmdList)));

	ThrowFailedHR(mComputeCmdList->Close());
	ThrowFailedHR(mComputeCmdAllocator->Reset());
	ThrowFailedHR(mComputeCmdList->Reset(mComputeCmdAllocator, nullptr));

	// copy commandLine
	D3D12_COMMAND_QUEUE_DESC cqCpDesc = {};
	cqCpDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;

	ThrowFailedHR(mDevice->CreateCommandQueue(&cqCpDesc, IID_PPV_ARGS(&mCopyCmdQueue)));

	ThrowFailedHR(mDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(&mCopyCmdAllocator)));

	ThrowFailedHR(mDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, mCopyCmdAllocator, nullptr, IID_PPV_ARGS(&mCopyCmdList)));

	ThrowFailedHR(mCopyCmdList->Close());
	ThrowFailedHR(mCopyCmdAllocator->Reset());
	ThrowFailedHR(mCopyCmdList->Reset(mCopyCmdAllocator, nullptr));

	mFence[0].create(mDevice);
	mFence[1].create(mDevice);
	mFence[2].create(mDevice);
}

void DXRPathTracer::declareRootSignatures()
{
	assert(mSrvUavHeap.get() != nullptr);

	// Global(usual) Root Signature
	mGlobalRS.resize(RootParamID::numParams);
	mGlobalRS[RootParamID::tableForOutBuffer]
		= new RootTable("u0", mSrvUavHeap[DescriptorID::outUAV].getGpuHandle());
	mGlobalRS[RootParamID::tableForPatternBuffer]
		= new RootTable("u1", mSrvUavHeap[DescriptorID::patternUAV].getGpuHandle());
	mGlobalRS[RootParamID::tableForPatternBuffer_]
		= new RootTable("u2", mSrvUavHeap[DescriptorID::patternUAV_].getGpuHandle());

	mGlobalRS[RootParamID::pointerForAccelerationStructure]
		= new RootPointer("(100) t0");					// It will be bound to mAccelerationStructure that is not initialized yet.
	mGlobalRS[RootParamID::tableForGeometryInputs]
		= new RootTable("(0) t0-t5", mSrvUavHeap[DescriptorID::sceneObjectBuff].getGpuHandle());
	mGlobalRS[RootParamID::pointerForGlobalConstants]
		= new RootPointer("b0");						// It will be bound to mGlobalConstantsBuffer that is not initialized yet.
	mGlobalRS[RootParamID::inTex]
		= new RootTable("(1) t0", mSrvUavHeap[DescriptorID::inTex].getGpuHandle());
	mGlobalRS.build();

	// Local Root Sinatures
	mHitGroupRS.resize(HitGroupParamID::numParams);
	mHitGroupRS[HitGroupParamID::constantsForObject]
		= new RootConstants<ObjectContants>("b1");		// Every local root signature's parameter is bound to its values via shader table.
	mHitGroupRS.build(D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE);
}

void DXRPathTracer::buildRaytracingPipeline()
{
	dxrLib.load(L"DXRShader.cso");

	mRtPipeline.setDXRLib(&dxrLib);
	mRtPipeline.setGlobalRootSignature(&mGlobalRS);
	mRtPipeline.addHitGroup(HitGroup(L"hitGp", L"closestHit", nullptr));
	mRtPipeline.addLocalRootSignature(LocalRootSignature(&mHitGroupRS, { L"hitGp"}));
	mRtPipeline.setMaxPayloadSize(sizeof(float) * 16);
	mRtPipeline.setMaxRayDepth(6);
	mRtPipeline.build();
}

void DXRPathTracer::initializeApplication()
{
	float fov = 15.0;
	camera.setFovY(fov);
	camera.setScreenSize((float)tracerOutW, (float)tracerOutH);
	camera.initOrbit(float3(0.0f, 0.0f, 0.0f), 1200.0f, 0.0f, 0.0f);

	for (int i = 0; i < 3; i++) {
		eye[i].setFovY(fov);
		eye[i].setScreenSize((float)tracerOutW, (float)tracerOutH);
		eye[i].initOrbit(float3(0.0f, 0.0f, 0.0f), 800.0f, 0.0f, 0.0f);
	}

	mGlobalConstants.rayTmin = 0.001f;  // 1mm
	mGlobalConstants.accumulatedFrames = 0;
	mGlobalConstants.resDepth = resDepth;
	mGlobalConstants.mtlResDepth.x = resDepth;
	mGlobalConstants.mtlResDepth.y = resDepth;
	mGlobalConstants.mtlResDepth.z = resDepth;
	mGlobalConstants.mtlResDepth.w = resDepth;
	mGlobalConstants.maxPathLength = 4;
	mGlobalConstants.backgroundLight = float3(.0f);
	mGlobalConstants.eyePos = float3(800.0f, 0.0, 0.0);
	mGlobalConstants.time = 0;

	mGlobalConstantsBuffer.create(sizeof(GloabalContants));
	*(RootPointer*)mGlobalRS[RootParamID::pointerForGlobalConstants]
		= mGlobalConstantsBuffer.getGpuAddress();

	uint64 maxBufferSize = _bpp(tracerOutFormat) * tracerOutW * tracerOutH;
	mTracerOutBuffer.create(maxBufferSize);
	
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	{
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		uavDesc.Format = tracerOutFormat;
		uavDesc.Buffer.NumElements = tracerOutW * tracerOutH;
	}


	uint64 patternBufferSize = tracerOutW * tracerOutH * resDepth * 4 / 8;
	for (auto& outBuff : mTracerPatternBuffer) {
		outBuff.create(patternBufferSize);
	}
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc_ = {};
	{
		uavDesc_.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		uavDesc_.Format = DXGI_FORMAT_R32_TYPELESS;
		uavDesc_.Buffer.NumElements = tracerOutW * tracerOutH * resDepth * 4 / 32;
		uavDesc_.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
	}
	mSrvUavHeap[DescriptorID::outUAV].assignUAV(mTracerOutBuffer, &uavDesc);
	mSrvUavHeap[DescriptorID::patternUAV].assignUAV(mTracerPatternBuffer[(patternBuffIdx + 1) % patternBuffNum], &uavDesc_);
	mSrvUavHeap[DescriptorID::patternUAV_].assignUAV(mTracerPatternBuffer[(patternBuffIdx + 2) % patternBuffNum], &uavDesc_);

	mReadBackBuffer.create(patternBufferSize);
}

void DXRPathTracer::onSizeChanged(uint width, uint height)
{
	if (width == tracerOutW && height == tracerOutH)
		return;

	width = width ? width : 1;
	height = height ? height : 1;

	tracerOutW = width;
	tracerOutH = height;

	camera.setScreenSize((float)tracerOutW, (float)tracerOutH);

	UINT64 bufferSize = _bpp(tracerOutFormat) * tracerOutW * tracerOutH;
	mTracerOutBuffer.destroy();
	mTracerOutBuffer.create(bufferSize);
	


	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	{
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		uavDesc.Format = tracerOutFormat;
		uavDesc.Buffer.NumElements = tracerOutW * tracerOutH;
	}
	mSrvUavHeap[DescriptorID::outUAV].assignUAV(mTracerOutBuffer, &uavDesc);
}

void DXRPathTracer::update(const InputEngine & input)
{
	updateCamera(input);
	update();
}


void DXRPathTracer::update()
{
	if (camera.notifyChanged())
	{
		mGlobalConstants.cameraPos = camera.getCameraPos();
		mGlobalConstants.cameraX = camera.getCameraX();
		mGlobalConstants.cameraY = camera.getCameraY();
		mGlobalConstants.cameraZ = camera.getCameraZ();
		mGlobalConstants.cameraAspect = camera.getCameraAspect();
		mGlobalConstants.accumulatedFrames = -1;
		// std::cout << camera.getCameraPos().x << ", " << camera.getCameraPos().y << ", " << camera.getCameraPos().z << std::endl;
	}
	if (eye[0].notifyChanged()) {
		mGlobalConstants.eyePos = eye[0].getCameraPos();
		mGlobalConstants.accumulatedFrames = -1;
	}

	mGlobalConstants.accumulatedFrames++;

	mGlobalConstants.time++;

	*(GloabalContants*)mGlobalConstantsBuffer.map() = mGlobalConstants;
}

void DXRPathTracer::update(int seed)
{
	if (camera.notifyChanged())
	{
		mGlobalConstants.cameraPos = camera.getCameraPos();
		mGlobalConstants.cameraX = camera.getCameraX();
		mGlobalConstants.cameraY = camera.getCameraY();
		mGlobalConstants.cameraZ = camera.getCameraZ();
		mGlobalConstants.cameraAspect = camera.getCameraAspect();
		mGlobalConstants.accumulatedFrames = -1;
		// std::cout << camera.getCameraPos().x << ", " << camera.getCameraPos().y << ", " << camera.getCameraPos().z << std::endl;
	}
	if (eye[0].notifyChanged()) {
		mGlobalConstants.eyePos = eye[0].getCameraPos();
		mGlobalConstants.accumulatedFrames = -1;
	}

	mGlobalConstants.accumulatedFrames++;

	mGlobalConstants.time = seed;

	*(GloabalContants*)mGlobalConstantsBuffer.map() = mGlobalConstants;
}

void DXRPathTracer::updateCamera(const InputEngine & input) {
	camera.update(input);
}

void DXRPathTracer::updateCamera(MouseEvent& input) {
	camera.update(input);
}

OrbitCamera DXRPathTracer::getCamera() {
	return camera;
}

void DXRPathTracer::setCamera(OrbitCamera cam) {
	camera = cam;
}

void DXRPathTracer::updateEye(const InputEngine & input, int idx) {
	eye[idx].update(input);
}

void DXRPathTracer::updateEye(MouseEvent& input, int idx) {
	eye[idx].update(input);
}

void DXRPathTracer::updateEye(const float3 input, int idx) {
	eye[idx].setCameraPos(input);
}

void DXRPathTracer::resetEye(int idx) {
	eye[idx].setCameraPos(camera.getCameraPos());
}

void DXRPathTracer::updateImg(cv::Mat input)
{
	// texture
	UINT8* start = (UINT8*)mTextureBuffer.map();
	for (UINT y = 0; y < mTexture.getHeight(); y++) {
		UINT8* pScan = start + y * mTexture.getRowPitch();
		memcpy(pScan, &(input.at<cv::Vec3b>(y, 0)[0]), sizeof(DWORD) * mTexture.getWidth());
	}
	mTextureBuffer.unmap();
}

void DXRPathTracer::setType(int type) {
	mGlobalConstants.type = type;
}

void DXRPathTracer::setMtlResDepth(int mtlIdx, int res) {
	switch (mtlIdx) {
		case 0:	mGlobalConstants.mtlResDepth.x = res; break;
		case 1:	mGlobalConstants.mtlResDepth.y = res; break;
		case 2:	mGlobalConstants.mtlResDepth.z = res; break;
		case 3:	mGlobalConstants.mtlResDepth.w = res; break;
	}
};

int DXRPathTracer::shootRays()
{
	mRtPipeline.bind(mCmdList);
	mSrvUavHeap.bind(mCmdList);
	mGlobalRS.bindCompute(mCmdList);


	*(RootPointer*)mGlobalRS[RootParamID::pointerForAccelerationStructure] = mAccelerationStructure[(nowAS + ASNum - 1) % ASNum].getGpuAddress();

	D3D12_DISPATCH_RAYS_DESC desc = {};
	{
		desc.Width = tracerOutW;
		desc.Height = tracerOutH;
		desc.Depth = 1;
		desc.RayGenerationShaderRecord = mShaderTable.getRecord(0);
		desc.MissShaderTable = mShaderTable.getSubTable(1, 2);
		desc.HitGroupTable = mShaderTable.getSubTable(3, scene->numObjects());
	}
	mCmdList->DispatchRays(&desc);

	ThrowFailedHR(mCmdList->Close());
	ID3D12CommandList* cmdLists[] = { mCmdList };
	mCmdQueue->ExecuteCommandLists(1, cmdLists);
	mFence[0].waitCommandQueue(mCmdQueue);
	ThrowFailedHR(mCmdAllocator->Reset());
	ThrowFailedHR(mCmdList->Reset(mCmdAllocator, nullptr));

	mTracerOutBuffer.status = 1;

	patternBuffIdx = (patternBuffIdx + 1) % patternBuffNum;

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc_ = {};
	{
		uavDesc_.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		uavDesc_.Format = DXGI_FORMAT_R32_TYPELESS;
		uavDesc_.Buffer.NumElements = tracerOutW * tracerOutH * resDepth * 4 / 32;
		uavDesc_.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
	}
	mSrvUavHeap[DescriptorID::patternUAV].assignUAV(mTracerPatternBuffer[(patternBuffIdx + 1) % patternBuffNum], &uavDesc_);
	mSrvUavHeap[DescriptorID::patternUAV_].assignUAV(mTracerPatternBuffer[(patternBuffIdx + 2) % patternBuffNum], &uavDesc_);


	return 1;
}

int DXRPathTracer::getPatternResult(void* result) {
	//if (mTracerPatternBuffer[patternBuffIdx].status == 1) {
		mReadBackBuffer.readback(mCopyCmdList, mTracerPatternBuffer[patternBuffIdx]);

		ThrowFailedHR(mCopyCmdList->Close());
		ID3D12CommandList* cmdCopyLists[] = { mCopyCmdList };
		mCopyCmdQueue->ExecuteCommandLists(1, cmdCopyLists);
		mFence[1].waitCommandQueue(mCopyCmdQueue);
		ThrowFailedHR(mCopyCmdAllocator->Reset());
		ThrowFailedHR(mCopyCmdList->Reset(mCopyCmdAllocator, nullptr));
		uint64 patternBufferSize = tracerOutW * tracerOutH * resDepth * 4 / 8;
		memcpy(result, mReadBackBuffer.map(), patternBufferSize);
		mReadBackBuffer.unmap();
		mTracerPatternBuffer[patternBuffIdx].status == 0;
		return 1;
	//}
	
	//return 0;
}

int DXRPathTracer::getResult(void* result) {
	if (mTracerOutBuffer.status == 1) {
		mReadBackBuffer.readback(mCopyCmdList, mTracerOutBuffer);

		ThrowFailedHR(mCopyCmdList->Close());
		ID3D12CommandList* cmdCopyLists[] = { mCopyCmdList };
		mCopyCmdQueue->ExecuteCommandLists(1, cmdCopyLists);
		mFence[1].waitCommandQueue(mCopyCmdQueue);
		ThrowFailedHR(mCopyCmdAllocator->Reset());
		ThrowFailedHR(mCopyCmdList->Reset(mCopyCmdAllocator, nullptr));
		UINT64 bufferSize = _bpp(tracerOutFormat) * tracerOutW * tracerOutH;
		// sUINT64 bufferSize = 3 * tracerOutW * tracerOutH;

		//	RGBAtoRGB((UINT8*)result, (UINT8*)mReadBackBuffer.map(), tracerOutW * tracerOutH);

		memcpy(result, mReadBackBuffer.map(), bufferSize);
		mReadBackBuffer.unmap();
		mTracerOutBuffer.status = 0;
		return 1;
	}
	
	return 0;
}

void DXRPathTracer::setupScene(const Scene * scene)
{
	uint numObjs = scene->numObjects();

	const Array<Vertex> vtxArr = scene->getVertexArray();
	const Array<Tridex> tdxArr = scene->getTridexArray();
	const Array<Transform> trmArr = scene->getTransformArray();
	const Array<float> cdfArr = scene->getCdfArray();
	const Array<Material> mtlArr = scene->getMaterialArray();
	const Array<Texture> texArr = scene->getTextureArray();

	assert(cdfArr.size() == 0 || cdfArr.size() == tdxArr.size());

	uint64 vtxBuffSize = vtxArr.size() * sizeof(Vertex);
	uint64 tdxBuffSize = tdxArr.size() * sizeof(Tridex);
	uint64 trmBuffSize = trmArr.size() * sizeof(Transform);
	uint64 cdfBuffSize = cdfArr.size() * sizeof(float);
	uint64 mtlBuffSize = mtlArr.size() * sizeof(Material);
	uint64 objBuffSize = numObjs * sizeof(GPUSceneObject);

	uint64 texBuffSize = 0;
	if (texArr.size() > 0) texBuffSize = _textureDataSize(texArr[0].format, texArr[0].width, texArr[0].height);

	mSceneUploadBuffer.create(vtxBuffSize + tdxBuffSize + trmBuffSize + cdfBuffSize + mtlBuffSize + objBuffSize);
	uint64 uploaderOffset = 0;

	auto initBuffer = [&](DefaultBuffer & buff, uint64 buffSize, void* srcData) {
		if (buffSize == 0)
			return;
		buff.create(buffSize);
		memcpy((uint8*)mSceneUploadBuffer.map() + uploaderOffset, srcData, buffSize);
		buff.uploadData(mCmdList, mSceneUploadBuffer, uploaderOffset);
		uploaderOffset += buffSize;
	};

	initBuffer(mVertexBuffer, vtxBuffSize, (void*)vtxArr.data());
	initBuffer(mTridexBuffer, tdxBuffSize, (void*)tdxArr.data());
	initBuffer(mTransformBuffer, trmBuffSize, (void*)trmArr.data());
	initBuffer(mCdfBuffer, cdfBuffSize, (void*)cdfArr.data());
	initBuffer(mMaterialBuffer, mtlBuffSize, (void*)mtlArr.data());

	mSceneObjectBuffer.create(objBuffSize);
	GPUSceneObject* copyDst = (GPUSceneObject*)((uint8*)mSceneUploadBuffer.map() + uploaderOffset);
	for (uint objIdx = 0; objIdx < numObjs; ++objIdx)
	{
		const SceneObject& obj = scene->getObject(objIdx);

		GPUSceneObject gpuObj = {};
		gpuObj.vertexOffset = obj.vertexOffset;
		gpuObj.tridexOffset = obj.tridexOffset;
		gpuObj.numTridices = obj.numTridices;
		gpuObj.objectArea = obj.meshArea * obj.scale * obj.scale;
		gpuObj.twoSided = obj.twoSided;
		gpuObj.materialIdx = obj.materialIdx;
		gpuObj.backMaterialIdx = obj.backMaterialIdx;
		gpuObj.textureIdx = obj.textureIdx;
		gpuObj.modelMatrix = obj.modelMatrix;

		copyDst[objIdx] = gpuObj;
	}
	mSceneObjectBuffer.uploadData(mCmdList, mSceneUploadBuffer, uploaderOffset);


	// texture
	if (texArr.size() > 0) {
		mTextureBuffer.create(texBuffSize);

		mTexture.create(texArr[0].format, texArr[0].width, texArr[0].height);
		mTexture.uploadData(mCmdList, mTextureBuffer, 0);
		UINT8* start = (UINT8*)mTextureBuffer.map();
		for (UINT y = 0; y < mTexture.getHeight(); y++) {
			UINT8* pScan = start + y * mTexture.getRowPitch();
			memcpy(pScan, &(texArr[0].img.at<cv::Vec3b>(y, 0)[0]), sizeof(DWORD) * mTexture.getWidth());
		}
		mTextureBuffer.unmap();
	}

	ThrowFailedHR(mCmdList->Close());
	ID3D12CommandList* cmdLists[] = { mCmdList };
	mCmdQueue->ExecuteCommandLists(1, cmdLists);
	mFence[0].waitCommandQueue(mCmdQueue);
	ThrowFailedHR(mCmdAllocator->Reset());
	ThrowFailedHR(mCmdList->Reset(mCmdAllocator, nullptr));

	this->scene = const_cast<Scene*>(scene);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	{
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Buffer.StructureByteStride = sizeof(GPUSceneObject);
		srvDesc.Buffer.NumElements = numObjs;
	}
	mSrvUavHeap[DescriptorID::sceneObjectBuff].assignSRV(mSceneObjectBuffer, &srvDesc);

	{
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Buffer.StructureByteStride = sizeof(Vertex);
		srvDesc.Buffer.NumElements = vtxArr.size();
	}
	mSrvUavHeap[DescriptorID::vertexBuff].assignSRV(mVertexBuffer, &srvDesc);

	{
		srvDesc.Format = DXGI_FORMAT_R32G32B32_UINT;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Buffer.StructureByteStride = 0;
		srvDesc.Buffer.NumElements = tdxArr.size();
	}
	mSrvUavHeap[DescriptorID::tridexBuff].assignSRV(mTridexBuffer, &srvDesc);

	{
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Buffer.StructureByteStride = sizeof(Material);
		srvDesc.Buffer.NumElements = mtlArr.size();
	}
	mSrvUavHeap[DescriptorID::materialBuff].assignSRV(mMaterialBuffer, &srvDesc);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc_ = {};
	{
		srvDesc_.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		srvDesc_.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc_.Texture2D.MipLevels = 1;
		srvDesc_.Texture2D.MostDetailedMip = 0;
		srvDesc_.Texture2D.PlaneSlice = 0;
		srvDesc_.Texture2D.ResourceMinLODClamp = 0.0F;
		srvDesc_.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	}
	mSrvUavHeap[DescriptorID::inTex].assignSRV(mTexture, &srvDesc_);

	setupShaderTable();

	initAccelerationStructure();

	*(RootPointer*)mGlobalRS[RootParamID::pointerForAccelerationStructure]
		= mAccelerationStructure[0].getGpuAddress();
}

void DXRPathTracer::updateScene(const Scene * scene)
{
	this->scene = const_cast<Scene*>(scene);
	
	uint numObjs = scene->numObjects();
	GPUSceneObject* copyDst = (GPUSceneObject*)((uint8*)mSceneUploadBuffer.map() + 0);
	for (uint objIdx = 0; objIdx < numObjs; ++objIdx)
	{
		const SceneObject& obj = scene->getObject(objIdx);

		GPUSceneObject gpuObj = {};
		gpuObj.vertexOffset = obj.vertexOffset;
		gpuObj.tridexOffset = obj.tridexOffset;
		gpuObj.numTridices = obj.numTridices;
		gpuObj.objectArea = obj.meshArea * obj.scale * obj.scale;
		gpuObj.twoSided = obj.twoSided;
		gpuObj.materialIdx = obj.materialIdx;
		gpuObj.backMaterialIdx = obj.backMaterialIdx;
		gpuObj.textureIdx = obj.textureIdx;
		//gpuObj.material = obj.material;
		//gpuObj.emittance = obj.lightColor * obj.lightIntensity;
		gpuObj.modelMatrix = obj.modelMatrix;

		copyDst[objIdx] = gpuObj;
	}
	mSceneObjectBuffer.uploadData(mComputeCmdList, mSceneUploadBuffer, 0);
	
	buildAccelerationStructure();
}

void DXRPathTracer::setupShaderTable()
{
	ShaderIdentifier* rayGenID = mRtPipeline.getIdentifier(L"rayGen");
	ShaderIdentifier* missRayID = mRtPipeline.getIdentifier(L"missRay");
	ShaderIdentifier* missShadowID = mRtPipeline.getIdentifier(L"missShadow");
	ShaderIdentifier* hitGpID = mRtPipeline.getIdentifier(L"hitGp");

	uint numObjs = scene->numObjects();
	mShaderTable.create(recordSize, numObjs + 3);

	HitGroupRecord * table = (HitGroupRecord*)mShaderTable.map();
	table[0].shaderIdentifier = *rayGenID;
	table[1].shaderIdentifier = *missRayID;
	table[2].shaderIdentifier = *missShadowID;

	auto & mtlArr = scene->getMaterialArray();
	for (uint i = 0; i < numObjs; ++i)
	{
		table[3 + i].shaderIdentifier = *hitGpID;
		table[3 + i].objConsts.objectIdx = i;
	}

	mShaderTable.uploadData(mCmdList);
}

void DXRPathTracer::initAccelerationStructure()
{
	uint numObjs = scene->numObjects();
	Array<GPUMesh> gpuMeshArr(numObjs);
	Array<dxTransform> transformArr(numObjs);

	D3D12_GPU_VIRTUAL_ADDRESS vtxAddr = mVertexBuffer.getGpuAddress();
	D3D12_GPU_VIRTUAL_ADDRESS tdxAddr = mTridexBuffer.getGpuAddress();
	for (uint objIdx = 0; objIdx < numObjs; ++objIdx)
	{
		const SceneObject& obj = scene->getObject(objIdx);

		gpuMeshArr[objIdx].numVertices = obj.numVertices;
		gpuMeshArr[objIdx].vertexBufferVA = vtxAddr + obj.vertexOffset * sizeof(Vertex);
		gpuMeshArr[objIdx].numTridices = obj.numTridices;
		gpuMeshArr[objIdx].tridexBufferVA = tdxAddr + obj.tridexOffset * sizeof(Tridex);

		transformArr[objIdx] = obj.modelMatrix;
	}

	for (auto& AS : mAccelerationStructure) {
		AS.init(mCmdList, gpuMeshArr, transformArr, sizeof(Vertex), 1, buildMode, buildFlags);
	}

	ThrowFailedHR(mCmdList->Close());
	ID3D12CommandList* cmdLists[] = { mCmdList };
	mCmdQueue->ExecuteCommandLists(1, cmdLists);
	mFence[0].waitCommandQueue(mCmdQueue);
	ThrowFailedHR(mCmdAllocator->Reset());
	ThrowFailedHR(mCmdList->Reset(mCmdAllocator, nullptr));

	//mAccelerationStructure.flush();
}

void DXRPathTracer::buildAccelerationStructure()
{
	uint numObjs = scene->numObjects();
	Array<GPUMesh> gpuMeshArr(numObjs);
	Array<dxTransform> transformArr(numObjs);

	D3D12_GPU_VIRTUAL_ADDRESS vtxAddr = mVertexBuffer.getGpuAddress();
	D3D12_GPU_VIRTUAL_ADDRESS tdxAddr = mTridexBuffer.getGpuAddress();
	for (uint objIdx = 0; objIdx < numObjs; ++objIdx)
	{
		const SceneObject& obj = scene->getObject(objIdx);

		gpuMeshArr[objIdx].numVertices = obj.numVertices;
		gpuMeshArr[objIdx].vertexBufferVA = vtxAddr + obj.vertexOffset * sizeof(Vertex);
		gpuMeshArr[objIdx].numTridices = obj.numTridices;
		gpuMeshArr[objIdx].tridexBufferVA = tdxAddr + obj.tridexOffset * sizeof(Tridex);

		transformArr[objIdx] = obj.modelMatrix;
	}


	mAccelerationStructure[nowAS % ASNum].build(mComputeCmdList, gpuMeshArr, transformArr,
		sizeof(Vertex), 1, buildMode, buildFlags);

	ThrowFailedHR(mComputeCmdList->Close());
	ID3D12CommandList * cmdLists[] = { mComputeCmdList };
	mComputeCmdQueue->ExecuteCommandLists(1, cmdLists);
	mFence[2].waitCommandQueue(mComputeCmdQueue);

	mAccelerationStructure[nowAS % ASNum].updated = true;
	ThrowFailedHR(mComputeCmdAllocator->Reset());
	ThrowFailedHR(mComputeCmdList->Reset(mComputeCmdAllocator, nullptr));
	nowAS++;
}
