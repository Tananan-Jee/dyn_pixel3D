//#pragma pack_matrix( row_major )    // It does not work!


static const float PI = 3.14159265f;

RaytracingAccelerationStructure scene : register(t0, space100);
RWBuffer<float4> tracerOutBuffer : register(u0);
RWByteAddressBuffer patternBuffer : register(u1);
RWByteAddressBuffer patternBuffer_ : register(u2);
Texture2D<float4> tex : register(t0, space1);

uint bit_rev8(unsigned int v) {
	uint temp = 0x0;
	for (int i = 0; i < 8; i++) {
		temp = temp * 2 + (v % 2);
		v /= 2;
	}
	return temp;
}

uint bit_change(unsigned int v) {
	uint tempindex[4];
	uint temp = 0x0;
	for (int i = 0; i < 4; i++) {
		tempindex[i] = (v >> (24 - i * 8)) % 256;
		tempindex[i] = bit_rev8(tempindex[i]);
		temp |= tempindex[i] << (24 - i * 8);
	}
	return temp;
}

struct Vertex
{
	float3 position;
	float3 normal;
	float2 texcoord;
};

static const int Lambertian = 0;
static const int Metal = 1;
static const int Plastic = 2;
static const int Glass = 3;
static const int Tex = 4;
struct Material
{
	float3 emittance;
	uint type;
	uint visible;
	float3 albedo;
	float roughness;
	float reflectivity;
	float transmittivity;
};

struct GPUSceneObject
{
	uint vertexOffset;
	uint tridexOffset;
	uint numTridices;
	float objectArea;
	uint twoSided;
	uint materialIdx;
	uint backMaterialIdx;
	int textureIdx;
	row_major float4x4 modelMatrix;
};
StructuredBuffer<GPUSceneObject> objectBuffer	: register(t0);
StructuredBuffer<Vertex> vertexBuffer			: register(t1);
Buffer<uint3> tridexBuffer						: register(t2);				//ByteAddressBuffer IndexBuffer : register(t2);
StructuredBuffer<Material> materialBuffer		: register(t3);


cbuffer GLOBAL_CONSTANTS : register(b0)
{
	float3 backgroundLight;
	float3 cameraPos;
	float3 cameraX;
	float3 cameraY;
	float3 cameraZ;
	float3 eyePos;
	float2 cameraAspect;
	float rayTmin;
	float rayTmax;
	uint type;
	uint accumulatedFrames;
	uint resDepth;
	uint4 mtlResDepth;
	uint maxPathLength;
	uint time;
}

cbuffer OBJECT_CONSTANTS : register(b1)
{
	uint objIdx;
};

struct RayPayload
{
	float3 radiance;
	float3 attenuation;
	float3 hitPos;
	float3 bounceDir;
	float depth;
	uint hitMaterial;
	uint rayDepth;
	uint seed;
};

struct ShadowPayload
{
	uint occluded;
};

RayDesc Ray(in float3 origin, in float3 direction, in float tMin, in float tMax)
{
	RayDesc ray;
	ray.Origin = origin;
	ray.Direction = direction;
	ray.TMin = tMin;
	ray.TMax = tMax;
	return ray;
}

float3 hsv2rgb(float3 c) {
	float4 K = float4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
	float3 p = abs(frac(c.xxx + K.xyz) * 6.0 - K.www);
	return c.z * lerp(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

void computeNormal(out float3 normal, out float3 faceNormal, in BuiltInTriangleIntersectionAttributes attr)
{
	GPUSceneObject obj = objectBuffer[objIdx];

	uint3 tridex = tridexBuffer[obj.tridexOffset + PrimitiveIndex()];
	Vertex vtx0 = vertexBuffer[obj.vertexOffset + tridex.x];
	Vertex vtx1 = vertexBuffer[obj.vertexOffset + tridex.y];
	Vertex vtx2 = vertexBuffer[obj.vertexOffset + tridex.z];

	float t0 = 1.0f - attr.barycentrics.x - attr.barycentrics.y;
	float t1 = attr.barycentrics.x;
	float t2 = attr.barycentrics.y;

	float3x3 transform = (float3x3) obj.modelMatrix;

	faceNormal = normalize(mul(transform,
		cross(vtx1.position - vtx0.position, vtx2.position - vtx0.position)
	));
	normal = normalize(mul(transform,
		t0 * vtx0.normal + t1 * vtx1.normal + t2 * vtx2.normal
	));
}

void computeNormal(out float3 normal, in BuiltInTriangleIntersectionAttributes attr)
{
	GPUSceneObject obj = objectBuffer[objIdx];

	uint3 tridex = tridexBuffer[obj.tridexOffset + PrimitiveIndex()];
	Vertex vtx0 = vertexBuffer[obj.vertexOffset + tridex.x];
	Vertex vtx1 = vertexBuffer[obj.vertexOffset + tridex.y];
	Vertex vtx2 = vertexBuffer[obj.vertexOffset + tridex.z];

	float t0 = 1.0f - attr.barycentrics.x - attr.barycentrics.y;
	float t1 = attr.barycentrics.x;
	float t2 = attr.barycentrics.y;

	float3x3 transform = (float3x3) obj.modelMatrix;

	normal = normalize(mul(transform,
		t0 * vtx0.normal + t1 * vtx1.normal + t2 * vtx2.normal
	));
}

RayPayload tracePath(in float3 startPos, in float3 startDir) {
	float3 radiance = 0.0f;
	float3 attenuation = 1.0f;

	RayDesc ray = Ray(startPos, startDir, rayTmin, rayTmax);
	RayPayload prd;
	prd.rayDepth = 0;
	prd.depth = 0.0;
	//prd.terminateRay = false;

	TraceRay(scene,
		0,
		~0, 0, 1, 0, ray, prd);

	radiance += attenuation * prd.radiance;
	attenuation *= prd.attenuation;

	ray.Origin = prd.hitPos;
	ray.Direction = prd.bounceDir;
	++prd.rayDepth;


	return prd;
}

[shader("raygeneration")]
void rayGen()
{
	uint2 launchIdx = DispatchRaysIndex().xy;
	uint2 launchDim = DispatchRaysDimensions().xy;
	uint bufferOffset = launchDim.x * launchIdx.y + launchIdx.x;

	float2 screenCoord = float2(launchIdx);
	float2 ndc = screenCoord / float2(launchDim) * 2.f - 1.f;
	float3 rayDir = normalize(ndc.x * cameraAspect.x * cameraX + ndc.y * cameraAspect.y * cameraY + cameraZ);

	float4 radiance = float4(0.0, 0.0, 0.0, 1.0);
	int patternNum = resDepth * 4;

	uint4 pattern[3];
	pattern[0] = 0;
	pattern[1] = 0;
	pattern[2] = 0;

	if (type == 0) {
		RayPayload ray = tracePath(cameraPos, rayDir);
		if (ray.hitPos.z > -0.0001 && ray.hitPos.z < 50.0) {
			radiance = float4(hsv2rgb(float3(ray.hitPos.z / 50.0, 1.0, 1.0)), 1.0);
		}
		else {
			radiance = float4(0.0, 0.0, 0.0, 1.0);
		}
		if (ray.depth < 0.0) {
			radiance = float4(backgroundLight, 1.0);
		}
	}
	else if (type == 1) {
		RayPayload ray = tracePath(cameraPos, rayDir);
		if (ray.hitPos.z > -0.0001 && ray.hitPos.z < 50.0) {
			uint mtlResDep_ = mtlResDepth.x;
			if (ray.hitMaterial & 0b0001)		mtlResDep_ = mtlResDepth.x;
			else if (ray.hitMaterial & 0b0010)	mtlResDep_ = mtlResDepth.y;
			else if (ray.hitMaterial & 0b0100)	mtlResDep_ = mtlResDepth.z;
			else if (ray.hitMaterial & 0b1000)	mtlResDep_ = mtlResDepth.w;

			uint d = uint(floor(ray.hitPos.z / 50.0 * mtlResDep_) * resDepth / mtlResDep_ * 1.5);
			radiance = float4(hsv2rgb(float3(d / 10.0, 1.0, 1.0)).bgr, 1.0);
		}
		else {
			radiance = float4(0.0, 0.0, 0.0, 1.0);
		}
		if (ray.depth < 0.0) {
			radiance = float4(backgroundLight, 1.0);
		}
	}
	else if (type == 2) {
		RayPayload ray;
		float3 startPos = cameraPos;
		radiance = float4(0.0, 0.0, 0.0, 1.0);
		uint hitDepth = 0;
		float alpha = 1.0;
		for (int i = 0; i < 3; i++) {
			ray = tracePath(startPos, rayDir);
			startPos = ray.hitPos;
			if (ray.depth < 0.0) break;
			if (ray.hitPos.z < -0.0001 || ray.hitPos.z >= 50.0) break;
			uint d = floor(ray.hitPos.z / 50.0 * resDepth);

			RayPayload gaze = tracePath(ray.hitPos, normalize(eyePos - ray.hitPos));
			if (gaze.depth > length(eyePos - ray.hitPos) || gaze.depth < 0) {
				if (hitDepth == 0) {
					radiance = float4(hsv2rgb(float3(ray.hitPos.z / 50.0, 1.0, 1.0)).bgr, alpha);
					hitDepth++;
				}
			}
		}
	}
	else if (type == 3) {
		uint a = launchDim.x * launchDim.y;
		for (int i = bufferOffset; i < a * resDepth * 4 / 8; i += a) {
			uint zero = 0;
			patternBuffer_.Store(i * 4, zero);
		}

		RayPayload ray = tracePath(cameraPos, rayDir);
		if (ray.hitPos.z > -0.0001 && ray.hitPos.z < 50.0 && ray.depth > 0.0) {
			if (ray.hitPos.x * ray.hitPos.x + ray.hitPos.y * ray.hitPos.y < 100.0 * 100.0) {
				float r = atan2(ray.hitPos.x, ray.hitPos.y);
				if (r < 0) r += 2.0 * PI;
				uint r_ = floor(r / (PI * 2.0) * 360.0);
				radiance = float4(hsv2rgb(float3(r_ / 36.0, 0.5 + 0.5 * (ray.hitPos.z / 50.0), 1.0)).bgr, 1.0);

				for (int i = 0; i < patternNum; i++) {
					uint bitIndex = launchDim.x * launchDim.y * i + bufferOffset;
					uint pixelOffset = (bitIndex / 32) * 4;
					uint tmp = (0x80 << ((int)ceil((bitIndex % 32) / 8)) * 8) >> bitIndex % 8;
					patternBuffer.InterlockedOr(pixelOffset, tmp);
				}
			}
		}
		else {
			radiance = float4(0.0, 0.0, 0.0, 1.0);
		}
		if (ray.depth < 0.0) {
			radiance = float4(backgroundLight, 1.0);
		}


	}
	else if (type == 4) {
		radiance = float4(0.0, 0.0, 0.0, 1.0);
		RayPayload ray = tracePath(cameraPos, rayDir);
		if (ray.hitPos.z > -0.0001 && ray.hitPos.z < 50.0) {
			if (ray.hitMaterial & 0b0001)		radiance += float4(0.5, 0.0, 0.0, 0.0);
			if (ray.hitMaterial & 0b0010)	radiance += float4(0.0, 0.5, 0.0, 0.0);
			if (ray.hitMaterial & 0b0100)	radiance += float4(0.0, 0.0, 0.5, 0.0);
			if (ray.hitMaterial & 0b1000)	radiance += float4(0.5, 0.5, 0.0, 0.0);
		}
		if (ray.depth < 0.0) {
			radiance = float4(0.0, 0.0, 0.0, 1.0);
		}
		//	radiance = float4(ray.radiance, 1.0);
	}
	else if (type == 5) {
		RayPayload ray;
		uint4 voxel = 0;
		float3 startPos = cameraPos;
		for (int i = 0; i < 3; i++) {
			ray = tracePath(startPos, rayDir);
			startPos = ray.hitPos;
			if (ray.hitPos.z > 0.001 && ray.hitPos.z < 50.0) {

				uint d = 0;
				if (ray.hitMaterial & 0b0001)		d = uint(floor(ray.hitPos.z / 50.0 * mtlResDepth.x) * resDepth / mtlResDepth.x * 1.5);
				else if (ray.hitMaterial & 0b0010)	d = uint(floor(ray.hitPos.z / 50.0 * mtlResDepth.y) * resDepth / mtlResDepth.y * 1.5);
				else if (ray.hitMaterial & 0b0100)	d = uint(floor(ray.hitPos.z / 50.0 * mtlResDepth.z) * resDepth / mtlResDepth.z * 1.5);
				else if (ray.hitMaterial & 0b1000)	d = uint(floor(ray.hitPos.z / 50.0 * mtlResDepth.w) * resDepth / mtlResDepth.w * 1.5);


				uint depthBit = 1 << (d % 32);
				uint uintOffset = floor((d % 128) / 32.0);
				if (uintOffset & 0b0001)	voxel.x |= depthBit;
				if (uintOffset & 0b0010)	voxel.y |= depthBit;
				if (uintOffset & 0b0100)	voxel.z |= depthBit;
				if (uintOffset & 0b1000)	voxel.w |= depthBit;
			}
		}


		uint time_ = uint(time % 128);
		uint off = (time_ % 128) % 32;
		uint uintOff = floor((time_ % 128) / 32.0);
		if (uintOff == 0) 		radiance = (voxel.x >> off & 1) ? float4(1.0, 0.0, 0.0, 1.0) : 0.0;
		else if (uintOff == 1) 	radiance = (voxel.y >> off & 1) ? float4(0.0, 1.0, 0.0, 1.0) : 0.0;
		else if (uintOff == 2) 	radiance = (voxel.z >> off & 1) ? float4(0.0, 0.0, 1.0, 1.0) : 0.0;
		else if (uintOff == 3) 	radiance = (voxel.w >> off & 1) ? float4(1.0, 1.0, 0.0, 1.0) : 0.0;
		if (ray.depth < 0.0) {
			radiance = float4(backgroundLight, 1.0);
		}
	}
	else if (type == 6) {
		uint a = launchDim.x * launchDim.y;
		for (int i = bufferOffset; i < a * resDepth * 4 / 8; i += a) {
			uint zero = 0;
			patternBuffer_.Store(i * 4, zero);
		}

		RayPayload ray;
		uint material = 2;
		pattern[0] = 0;
		float3 startPos = cameraPos;
		for (int i = 0; i < 3; i++) {
			ray = tracePath(startPos, rayDir);
			startPos = ray.hitPos;
			if (ray.depth < 0.0) break;
			if (ray.hitPos.z < 0.0001 || ray.hitPos.z >= 50.0) break;

			float r = atan2(ray.hitPos.x, ray.hitPos.y);
			if (r < 0) r += 2.0 * PI;
			uint r_ = floor(r / (PI * 2.0) * 360.0);

			uint bitDepth = 0;
			if (ray.hitMaterial & 0b0001)		bitDepth = uint(floor(ray.hitPos.z / 50.0 * mtlResDepth.x) * resDepth / mtlResDepth.x * 1.5);
			else if (ray.hitMaterial & 0b0010)	bitDepth = resDepth * 2 - 1 - uint(floor(ray.hitPos.z / 50.0 * mtlResDepth.y) * resDepth / mtlResDepth.y * 1.5);
			else if (ray.hitMaterial & 0b0100)	bitDepth = resDepth * 2 + uint(floor(ray.hitPos.z / 50.0 * mtlResDepth.z) * resDepth / mtlResDepth.z * 1.5);
			else if (ray.hitMaterial & 0b1000)	bitDepth = resDepth * 4 - 1 - uint(floor(ray.hitPos.z / 50.0 * mtlResDepth.w) * resDepth / mtlResDepth.w * 1.5);

			uint bitIndex = launchDim.x * launchDim.y * bitDepth + bufferOffset;
			uint pixelOffset = (bitIndex / 32) * 4;
			uint tmp = 1 << (bitIndex % 32);
			patternBuffer.InterlockedOr(pixelOffset, tmp);
		}

		uint time_ = (time % patternNum);
		uint bitIndex_ = launchDim.x * launchDim.y * time_ + bufferOffset;
		uint bitOffset_ = bitIndex_ % 32;
		uint pixelOffset_ = bitIndex_ / 32 * 4;
		float4 color = 0.0;

		if (time_ < resDepth) 			color = float4(1.0, 0.0, 0.0, 1.0);
		else if (time_ < resDepth * 2)	color = float4(0.0, 1.0, 0.0, 1.0);
		else if (time_ < resDepth * 3)	color = float4(0.0, 0.0, 1.0, 1.0);
		else if (time_ < resDepth * 4)	color = float4(1.0, 0.7, 0.0, 1.0);
		radiance = (patternBuffer.Load(pixelOffset_) >> bitOffset_ & 1) ? color : 0.0;


	}
	else if (type == 7) {
		uint a = launchDim.x * launchDim.y;
		for (int i = bufferOffset; i < a * resDepth * 4 / 8; i += a) {
			uint zero = 0;
			patternBuffer_.Store(i * 4, zero);
		}

		RayPayload ray;
		uint material = 2;
		pattern[0] = 0;
		float3 startPos = cameraPos;
		for (int i = 0; i < 6; i++) {
			ray = tracePath(startPos, rayDir);
			startPos = ray.hitPos;
			if (ray.depth < 0.0) break;
			if (ray.hitPos.z < 0.0001 || ray.hitPos.z >= 50.0) break;

			float r = atan2(ray.hitPos.x, ray.hitPos.y);
			if (r < 0) r += 2.0 * PI;
			uint off = 0;
			if (r < PI / 2.0) {
				off = 0;
			}
			else if (r < PI) {
				off = resDepth;
				r -= PI / 2.0;
			}
			else if (r < PI * 3 / 2) {
				off = resDepth * 2;
				r -= PI;
			}
			else {
				off = resDepth * 3;
				r -= PI * 3.0 / 2.0;
			}
			uint r_ = 0;

			uint bitDepth = 0;
			if (ray.hitMaterial & 0b0001) {
				bitDepth = uint(floor(ray.hitPos.z / 50.0 * mtlResDepth.x) * resDepth / mtlResDepth.x);
				r_ = uint(floor(r * 2 / PI * mtlResDepth.x) * resDepth / mtlResDepth.x) + off;
				bitDepth = (bitDepth + r_) % patternNum;
				uint bitIndex = launchDim.x * launchDim.y * bitDepth + bufferOffset;
				uint pixelOffset = (bitIndex / 32) * 4;
				uint tmp = (0x80 << ((int)ceil((bitIndex % 32) / 8)) * 8) >> bitIndex % 8;
				patternBuffer.InterlockedOr(pixelOffset, tmp);
			}
			if (ray.hitMaterial & 0b0010) {
				bitDepth = resDepth * 2 - 1 - uint(floor(ray.hitPos.z / 50.0 * mtlResDepth.y) * resDepth / mtlResDepth.y);
				r_ = uint(floor(r * 2 / PI * mtlResDepth.y) * resDepth / mtlResDepth.y) + off;
				bitDepth = (bitDepth + r_) % patternNum;
				uint bitIndex = launchDim.x * launchDim.y * bitDepth + bufferOffset;
				uint pixelOffset = (bitIndex / 32) * 4;
				uint tmp = (0x80 << ((int)ceil((bitIndex % 32) / 8)) * 8) >> bitIndex % 8;
				patternBuffer.InterlockedOr(pixelOffset, tmp);
			}
			if (ray.hitMaterial & 0b0100) {
				bitDepth = resDepth * 2 + uint(floor(ray.hitPos.z / 50.0 * mtlResDepth.z) * resDepth / mtlResDepth.z);
				r_ = uint(floor(r * 2 / PI * mtlResDepth.z) * resDepth / mtlResDepth.z) + off;
				bitDepth = (bitDepth + r_) % patternNum;
				uint bitIndex = launchDim.x * launchDim.y * bitDepth + bufferOffset;
				uint pixelOffset = (bitIndex / 32) * 4;
				uint tmp = (0x80 << ((int)ceil((bitIndex % 32) / 8)) * 8) >> bitIndex % 8;
				patternBuffer.InterlockedOr(pixelOffset, tmp);
			}
			/*if (ray.hitMaterial & 0b1000) {
				bitDepth = resDepth * 4 - 1 - uint(floor(ray.hitPos.z / 50.0 * mtlResDepth.w) * resDepth / mtlResDepth.w);
				r_ = uint(floor(r * 2 / PI * mtlResDepth.w) * resDepth / mtlResDepth.w) + off;
				bitDepth = (bitDepth + r_) % patternNum;
				uint bitIndex = launchDim.x * launchDim.y * bitDepth + bufferOffset;
				uint pixelOffset = (bitIndex / 32) * 4;
				uint tmp = (0x80 << ((int)ceil((bitIndex % 32) / 8)) * 8) >> bitIndex % 8;
				patternBuffer.InterlockedOr(pixelOffset, tmp);
			}*/
		}

		uint time_ = (time % patternNum);
		uint bitIndex_ = launchDim.x * launchDim.y * time_ + bufferOffset;
		uint bitOffset_ = bitIndex_ % 32;
		uint pixelOffset_ = bitIndex_ / 32 * 4;
		float4 color = 0.0;

		float r__ = atan2(ray.hitPos.x, ray.hitPos.y);
		if (r__ < 0) r__ += 2.0 * PI;
		uint mat = (time_ - r__ / (PI * 2.0) * 360.0 + patternNum) % patternNum;

		if (mat < resDepth) color = float4(1.0, 0.0, 0.0, 1.0);
		else if (mat < resDepth * 2) color = float4(0.0, 1.0, 0.0, 1.0);
		else if (mat < resDepth * 3) color = float4(0.0, 0.0, 1.0, 1.0);
		else if (mat < resDepth * 4) color = float4(1.0, 0.7, 0.0, 1.0);

		radiance = (patternBuffer.Load(pixelOffset_) << bitOffset_ & 0b10000000) ? color : 0.0;
	}
	else if (type == 8) {
		uint a = launchDim.x * launchDim.y;
		for (int i = bufferOffset; i < a * resDepth * 4 / 8; i += a) {
			uint zero = 0;
			patternBuffer_.Store(i * 4, zero);
		}

		RayPayload ray;
		uint material = 2;
		pattern[0] = 0;
		float3 startPos = cameraPos;
		int hitDepth = 0;
		for (int i = 0; i < 6; i++) {
			ray = tracePath(startPos, rayDir);
			startPos = ray.hitPos;
			if (ray.depth < 0.0) break;
			if (ray.hitPos.z < 0.0001 || ray.hitPos.z >= 50.0) break;
			uint d = floor(ray.hitPos.z / 50.0 * resDepth);

			RayPayload gaze = tracePath(ray.hitPos, normalize(eyePos - ray.hitPos));
			if (gaze.depth > length(eyePos - ray.hitPos) || gaze.depth < 0) {
				if (true) {
					float r = atan2(ray.hitPos.x, ray.hitPos.y);
					if (r < 0) r += 2.0 * PI;
					uint off = 0;
					if (r < PI / 2.0) {
						off = 0;
					}
					else if (r < PI) {
						off = resDepth;
						r -= PI / 2.0;
					}
					else if (r < PI * 3 / 2) {
						off = resDepth * 2;
						r -= PI;
					}
					else {
						off = resDepth * 3;
						r -= PI * 3.0 / 2.0;
					}
					
					//original
					/*uint r_ = 0;

					uint bitDepth = 0;
					if (ray.hitMaterial & 0b0001) {
						bitDepth = uint(floor(ray.hitPos.z / 50.0 * mtlResDepth.x) * resDepth / mtlResDepth.x);
						r_ = uint(floor(r * 2 / PI * mtlResDepth.x) * resDepth / mtlResDepth.x) + off;
					}
					else if (ray.hitMaterial & 0b0010) {
						bitDepth = resDepth * 2 - 1 - uint(floor(ray.hitPos.z / 50.0 * mtlResDepth.y) * resDepth / mtlResDepth.y);
						r_ = uint(floor(r * 2 / PI * mtlResDepth.y) * resDepth / mtlResDepth.y) + off;
					}
					else if (ray.hitMaterial & 0b0100) {
						bitDepth = resDepth * 2 + uint(floor(ray.hitPos.z / 50.0 * mtlResDepth.z) * resDepth / mtlResDepth.z);
						r_ = uint(floor(r * 2 / PI * mtlResDepth.z) * resDepth / mtlResDepth.z) + off;
					}
					else if (ray.hitMaterial & 0b1000) {
						bitDepth = resDepth * 4 - 1 - uint(floor(ray.hitPos.z / 50.0 * mtlResDepth.w) * resDepth / mtlResDepth.w);
						r_ = uint(floor(r * 2 / PI * mtlResDepth.w) * resDepth / mtlResDepth.w) + off;
					}

					bitDepth = (bitDepth + r_) % patternNum;
					uint bitIndex = launchDim.x * launchDim.y * bitDepth + bufferOffset;
					uint pixelOffset = (bitIndex / 32) * 4;
					uint tmp = (0x80 << ((int)ceil((bitIndex % 32) / 8)) * 8) >> bitIndex % 8;
					patternBuffer.InterlockedOr(pixelOffset, tmp);
					hitDepth++;*/

					//Tananan edited
					uint r_ = 0;
					uint bitDepth = 0;
					if (ray.hitMaterial == 0b0001) {
						bitDepth = uint(floor(ray.hitPos.z / 50.0 * mtlResDepth.x) * resDepth / mtlResDepth.x);
						r_ = uint(floor(r * 2 / PI * mtlResDepth.x) * resDepth / mtlResDepth.x) + off;
						bitDepth = (bitDepth + r_) % patternNum;
						uint bitIndex = launchDim.x * launchDim.y * bitDepth + bufferOffset;
						uint pixelOffset = (bitIndex / 32) * 4;
						uint tmp = (0x80 << ((int)ceil((bitIndex % 32) / 8)) * 8) >> bitIndex % 8;
						patternBuffer.InterlockedOr(pixelOffset, tmp);
						hitDepth++;
					}
					else if (ray.hitMaterial == 0b0010) {
						bitDepth = resDepth * 2 - 1 - uint(floor(ray.hitPos.z / 50.0 * mtlResDepth.y) * resDepth / mtlResDepth.y);
						r_ = uint(floor(r * 2 / PI * mtlResDepth.y) * resDepth / mtlResDepth.y) + off;
						bitDepth = (bitDepth + r_) % patternNum;
						uint bitIndex = launchDim.x * launchDim.y * bitDepth + bufferOffset;
						uint pixelOffset = (bitIndex / 32) * 4;
						uint tmp = (0x80 << ((int)ceil((bitIndex % 32) / 8)) * 8) >> bitIndex % 8;
						patternBuffer.InterlockedOr(pixelOffset, tmp);
						hitDepth++;
					}
					else if (ray.hitMaterial == 0b0100) {
						bitDepth = resDepth * 2 + uint(floor(ray.hitPos.z / 50.0 * mtlResDepth.z) * resDepth / mtlResDepth.z);
						r_ = uint(floor(r * 2 / PI * mtlResDepth.z) * resDepth / mtlResDepth.z) + off;
						bitDepth = (bitDepth + r_) % patternNum;
						uint bitIndex = launchDim.x * launchDim.y * bitDepth + bufferOffset;
						uint pixelOffset = (bitIndex / 32) * 4;
						uint tmp = (0x80 << ((int)ceil((bitIndex % 32) / 8)) * 8) >> bitIndex % 8;
						patternBuffer.InterlockedOr(pixelOffset, tmp);
						hitDepth++;
					}
					/*else if (ray.hitMaterial & 0b1000) {
						bitDepth = resDepth * 4 - 1 - uint(floor(ray.hitPos.z / 50.0 * mtlResDepth.w) * resDepth / mtlResDepth.w);
						r_ = uint(floor(r * 2 / PI * mtlResDepth.w) * resDepth / mtlResDepth.w) + off;
						bitDepth = (bitDepth + r_) % patternNum;
						uint bitIndex = launchDim.x * launchDim.y * bitDepth + bufferOffset;
						uint pixelOffset = (bitIndex / 32) * 4;
						uint tmp = (0x80 << ((int)ceil((bitIndex % 32) / 8)) * 8) >> bitIndex % 8;
						patternBuffer.InterlockedOr(pixelOffset, tmp);
						hitDepth++;
					}*/
					else if (ray.hitMaterial == 0b0101) {
						bitDepth = uint(floor(ray.hitPos.z / 50.0 * mtlResDepth.x) * resDepth / mtlResDepth.x);
						r_ = uint(floor(r * 2 / PI * mtlResDepth.x) * resDepth / mtlResDepth.x) + off;
						bitDepth = (bitDepth + r_) % patternNum;
						uint bitIndex = launchDim.x * launchDim.y * bitDepth + bufferOffset;
						uint pixelOffset = (bitIndex / 32) * 4;
						uint tmp = (0x80 << ((int)ceil((bitIndex % 32) / 8)) * 8) >> bitIndex % 8;
						patternBuffer.InterlockedOr(pixelOffset, tmp);
						hitDepth++;
						bitDepth = resDepth * 2 + uint(floor(ray.hitPos.z / 50.0 * mtlResDepth.z) * resDepth / mtlResDepth.z);
						r_ = uint(floor(r * 2 / PI * mtlResDepth.z) * resDepth / mtlResDepth.z) + off;
						bitDepth = (bitDepth + r_) % patternNum;
						bitIndex = launchDim.x * launchDim.y * bitDepth + bufferOffset;
						pixelOffset = (bitIndex / 32) * 4;
						tmp = (0x80 << ((int)ceil((bitIndex % 32) / 8)) * 8) >> bitIndex % 8;
						patternBuffer.InterlockedOr(pixelOffset, tmp);
						hitDepth++;
					}
								

				}
			}
		}

		uint time_ = (time % patternNum);
		uint bitIndex_ = launchDim.x * launchDim.y * time_ + bufferOffset;
		uint bitOffset_ = bitIndex_ % 32;
		uint pixelOffset_ = bitIndex_ / 32 * 4;
		float4 color = 0.0;

		float r__ = atan2(ray.hitPos.x, ray.hitPos.y);
		if (r__ < 0) r__ += 2.0 * PI;
		uint mat = (time_ - r__ / (PI * 2.0) * 360.0 + patternNum) % patternNum;

		if (mat < resDepth) color = float4(1.0, 0.0, 0.0, 1.0);
		else if (mat < resDepth * 2) color = float4(0.0, 1.0, 0.0, 1.0);
		else if (mat < resDepth * 3) color = float4(0.0, 0.0, 1.0, 1.0);
		else if (mat < resDepth * 4) color = float4(1.0, 0.7, 0.0, 1.0);

		radiance = (patternBuffer.Load(pixelOffset_) << bitOffset_ & 0b10000000) ? color : 0.0;
	}

	tracerOutBuffer[bufferOffset] = radiance;
}

[shader("closesthit")]
void closestHit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
	GPUSceneObject obj = objectBuffer[objIdx];
	uint mtlIdx = obj.materialIdx;
	int textureIdx = obj.textureIdx;
	uint material = 0;
	float3 radiance = 0.0;

	float3 E = -WorldRayDirection();
	float3 N;
	float3 fN;
	computeNormal(N, fN, attr);
	float EN = dot(E, N), EfN = dot(E, fN);

	if (textureIdx >= 0) {
		uint3 tridex = tridexBuffer[obj.tridexOffset + PrimitiveIndex()];
		Vertex vtx0 = vertexBuffer[obj.vertexOffset + tridex.x];
		Vertex vtx1 = vertexBuffer[obj.vertexOffset + tridex.y];
		Vertex vtx2 = vertexBuffer[obj.vertexOffset + tridex.z];

		float3 bary = float3(
			1.0 - attr.barycentrics.x - attr.barycentrics.y,
			attr.barycentrics.x,
			attr.barycentrics.y
			);
		float2 uv = bary.x * vtx0.texcoord + bary.y * vtx1.texcoord + bary.z * vtx2.texcoord;
		uv.y = 1.0 - uv.y;
		float2 cood = uv * float2(1024, 1024);
		float4 texColor = tex.Load(int3(cood, 0)).rgba;

		//original
		/*if (texColor.r > 0)			material |= 0b0001;
		else if (texColor.b > 0)	material |= 0b0010;
		else if (texColor.g > 0)	material |= 0b0100;
		else if (texColor.a > 0)	material |= 0b1000;*/

		//Tananan edited
		if (texColor.r > 0)			material |= 0b0001;
		if (texColor.b > 0)	material |= 0b0010;
		if (texColor.g > 0)	material |= 0b0100;
		//if (texColor.a > 0)	material |= 0b1000;
		
		//テクスチャを重ねる場合、ここの0を1に変更する。
		//実際にスクリーンを配置する際、水毛糸をRとして、反時計回りに、BGAとする
		/*if (texColor.r > 0)			material |= 0b0001;
		else if (texColor.b > 0)	material |= 0b0010;
		else if (texColor.g > 0)	material |= 0b0100;
		else if (texColor.a > 0)	material |= 0b1000;
		*/
		/*
		if (texColor.r > 0 && texColor.g > 0)	material = 3;
		else if (texColor.r > 0)	material = 0;
		else if (texColor.g > 0)	material = 1;
		else if (texColor.b > 0)	material = 2;
		*/
		radiance = tex.Load(int3(cood, 0)).rgb;
	}
	else {
		Material mtl = materialBuffer[mtlIdx];
		material = mtl.type;
	}

	payload.radiance = radiance;
	payload.attenuation = 1.0f;
	payload.depth = RayTCurrent();
	payload.hitPos = WorldRayOrigin() - RayTCurrent() * E;
	payload.bounceDir = N;
	payload.hitMaterial = material;
}


[shader("miss")]
void missRay(inout RayPayload payload)
{
	//payload.radiance = 0.1;
	payload.radiance = backgroundLight;
	payload.rayDepth = maxPathLength;
	payload.depth = -1.0;
}

[shader("miss")]
void missShadow(inout ShadowPayload payload)
{
	payload.occluded = false;
}