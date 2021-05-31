#include "../util/pch.h"
#include "SceneLoader.h"
#include "../util/Mesh/generateMesh.h"
#include "../util/Mesh/loadMesh.h"

#define PAT_ORIGINAL


void SceneManager::initializeGeometryFromMeshes(Scene* scene, const Array<Mesh*>& meshes)
{
	scene->clear();

	uint numObjs = meshes.size();
	Array<Vertex>& vtxArr = scene->vtxArr;
	Array<Tridex>& tdxArr = scene->tdxArr;
	Array<SceneObject>& objArr = scene->objArr;

	objArr.resize(numObjs);

	uint totVertices = 0;
	uint totTridices = 0;

	for (uint i = 0; i < numObjs; ++i)
	{
		uint nowVertices = meshes[i]->vtxArr.size();
		uint nowTridices = meshes[i]->tdxArr.size();

		objArr[i].vertexOffset = totVertices;
		objArr[i].tridexOffset = totTridices;
		objArr[i].numVertices = nowVertices;
		objArr[i].numTridices = nowTridices;

		totVertices += nowVertices;
		totTridices += nowTridices;
	}

	vtxArr.resize(totVertices);
	tdxArr.resize(totTridices);

	for (uint i = 0; i < numObjs; ++i)
	{
		memcpy(&vtxArr[objArr[i].vertexOffset], &meshes[i]->vtxArr[0], sizeof(Vertex) * objArr[i].numVertices);
		memcpy(&tdxArr[objArr[i].tridexOffset], &meshes[i]->tdxArr[0], sizeof(Tridex) * objArr[i].numTridices);
	}
}

void SceneManager::computeModelMatrices(Scene* scene)
{
	for (auto& obj : scene->objArr)
	{
		obj.modelMatrix = composeMatrix(obj.translation, obj.rotation, obj.scale);
	}
}

int SceneManager::setTranslation(int objectId, float3 trans) {
	if (objectId < scene->objArr.size()) {
		scene->objArr[objectId].translation = trans;
		return 1;
	}
	return 0;
}

int SceneManager::setRotation(int objectId, float4 rot) {
	if (objectId < scene->objArr.size()) {
		scene->objArr[objectId].rotation = rot;
		return 1;
	}
	return 0;
}



Scene* SceneManager::update() {
	computeModelMatrices(scene);
	return scene;
}

Scene* SceneManager::initScene() {

#ifdef PAT_ORIGINAL




	int numObjects = object_number.size();

	std::vector<Mesh> objects;

	for (int i = 0; i < numObjects; i++) {
		switch (object_number[i]) {
		case(BACK):
			objects.push_back(generateRectangleMesh(float3(0.0f), float3(200.0f, 200.0f, 0.0f), FaceDir::front));
			break;
		case(M_PANDA):
			objects.push_back(loadMeshFromOBJFile("./data/mesh/Panda_pressed.obj", true));
			break;
		case(M_BUNNY):
			objects.push_back(loadMeshFromOBJFile("./data/mesh/bunny_tex.obj", true));
			break;
		case(M_HUMAN):
			objects.push_back(loadMeshFromOBJFile("./data/mesh/Human4.obj", true));
			break;
		case(M_MOUNTAIN):
			objects.push_back(loadMeshFromOBJFile("./data/mesh/Mountain4.obj", true));
			break;
		case(M_TREE):
			objects.push_back(loadMeshFromOBJFile("./data/mesh/Tree1.obj", true));
			break;
		case(M_TOYPOODLE):
			objects.push_back(loadMeshFromOBJFile("./data/mesh/Tpoodle3.obj", true));
			break;
		case(CUBE):
			objects.push_back(generateCubeMesh(float3(0.0f, 1.0f, 0.0f), float3(30.0, 30.0, 30.0)));
			break;
		case(SPHERE):
			objects.push_back(generateSphereMesh(float3(0, 1, 0), 10.0f));
			break;
		case(M_DODECAHEDRON):
			objects.push_back(loadMeshFromOBJFile("./data/mesh/12_5.obj", true));
			break;
		case(M_MUSCHROOM):
			objects.push_back(loadMeshFromOBJFile("./data/mesh/muschroom_new.obj", true));
			break;
		case(M_HAND):
			objects.push_back(loadMeshFromOBJFile("./data/mesh/hand.obj", true));
			break;
		case(M_CUP):
			objects.push_back(loadMeshFromOBJFile("./data/mesh/cup.obj", true));
			break;
		case(M_DEER):
			objects.push_back(loadMeshFromOBJFile("./data/mesh/deer.obj", true));
			break;
		default:
			break;
		}
	}

	Array<Mesh*> meshes;
	for (int j = 0; j < numObjects; j++) {
		meshes.push_back(&objects[j]);
	}
	initializeGeometryFromMeshes(scene, meshes);
	cv::Mat TexMat = cv::imread("./data/img/Panda_ColorBR.png", cv::IMREAD_UNCHANGED);

	// ------------------------< geometry & material >------------------------
	Array<SceneObject> objArr(numObjects);
	for (uint i = 0; i < numObjects; ++i) objArr[i] = scene->objArr[i];

	Array<Material>& mtlArr = scene->mtlArr;
	mtlArr.resize(numObjects);
	Array<Texture>& texArr = scene->texArr;

	for (int i = 0; i < numObjects; i++) {
		switch (object_number[i]) {
		case(BACK):
			objArr[i].translation = float3(0.0, 0.0, -10.0);
			mtlArr[i].albedo = float3(1.0f, 1.0f, 1.0f);
			break;
		case(M_PANDA):
			objArr[i].translation = float3(80.0f, 0.0f, 20.0f); // Comparison of Hidden Surface Removal
			objArr[i].scale = 2.8f;
			//objArr[i].translation = float3(60.0f, 0.0f, 5.0f);
			//objArr[i].scale = 4.0f;
			objArr[i].twoSided = true;
			objArr[i].textureIdx = 0;
			mtlArr[i].type = Glass;
			mtlArr[i].albedo = float3(1.0f);
			mtlArr[i].reflectivity = 0.01f;
			mtlArr[i].roughness = 0.02f;
			TexMat = cv::imread("./data/img/Panda_ColorBR.png", cv::IMREAD_UNCHANGED);
			break;

		case(M_BUNNY):
			objArr[i].translation = float3(-30.0f, 45.0f, 20.0f);
			objArr[i].rotation = getRotationAsQuternion({ 0, 1, 0 }, 0.0f);
			objArr[i].scale = 400.0f;
			objArr[i].twoSided = true;
			//objArr[i].textureIdx = 0;
			mtlArr[i].type = 0b0100;
			mtlArr[i].albedo = float3(1.0f);
			mtlArr[i].reflectivity = 0.01f;
			mtlArr[i].roughness = 0.02f;
			TexMat = cv::imread("./data/img/BunnyTex.png", cv::IMREAD_UNCHANGED);
			//TexMat = cv::imread("./data/img/Flat.png", cv::IMREAD_UNCHANGED);
			break;

		case(M_HUMAN):
			objArr[i].translation = float3(50.0f, -15.0f, 5.0f);
			objArr[i].rotation = getRotationAsQuternion({ 1, 0, 0 }, 90.0f);
			objArr[i].scale = 25.0f;
			objArr[i].twoSided = true;
			objArr[i].textureIdx = 0;
			mtlArr[i].type = Glass;
			mtlArr[i].albedo = float3(1.0f);
			mtlArr[i].reflectivity = 0.01f;
			mtlArr[i].roughness = 0.02f;
			TexMat = cv::imread("./data/img/HumanTex.png", cv::IMREAD_UNCHANGED);
			break;

		case(M_MOUNTAIN):
			objArr[i].translation = float3(30.0f, 0.0f, 40.0f);
			objArr[i].rotation = getRotationAsQuternion({ 0, 0, 1 }, -90.0f);
			objArr[i].scale = 20.0f;
			objArr[i].twoSided = true;
			objArr[i].textureIdx = 0;
			mtlArr[i].type = Glass;
			mtlArr[i].albedo = float3(1.0f);
			mtlArr[i].reflectivity = 0.01f;
			mtlArr[i].roughness = 0.02f;
			//TexMat = cv::imread("./data/img/Mountain5_Texture.png", cv::IMREAD_UNCHANGED);
			TexMat = cv::imread("./data/img/Flat.png", cv::IMREAD_UNCHANGED);
			break;

		case(M_TREE):
			objArr[i].translation = float3(40.0f, -50.0f, 0.0f);
			objArr[i].scale = 5.0f;
			objArr[i].twoSided = true;
			objArr[i].textureIdx = 0;
			mtlArr[i].type = Glass;
			mtlArr[i].albedo = float3(1.0f);
			mtlArr[i].reflectivity = 0.01f;
			mtlArr[i].roughness = 0.02f;
			TexMat = cv::imread("./data/img/TreeTex.png", cv::IMREAD_UNCHANGED);
			break;

		case(M_TOYPOODLE):
			objArr[i].translation = float3(50.0f, -50.0f, 45.0f);
			objArr[i].scale = 2.8f;
			objArr[i].twoSided = true;
			objArr[i].textureIdx = 0;
			mtlArr[i].type = Glass;
			mtlArr[i].albedo = float3(1.0f);
			mtlArr[i].reflectivity = 0.01f;
			mtlArr[i].roughness = 0.02f;
			TexMat = cv::imread("./data/img/ToyPoodleColor.png", cv::IMREAD_UNCHANGED);
			break;

		case(CUBE):
			objArr[i].translation = float3(-30.0f, -40.0f, 25.0f);
			objArr[i].scale = 0.7f;
			// objArr[i].rotation = getRotationAsQuternion({ 0.7, 1, 0 }, 30.0f);
			mtlArr[i].type = 0b0001;
			mtlArr[i].albedo = float3(0.1, 0.1, 0.7);
			mtlArr[i].roughness = 0.3f;
			mtlArr[i].reflectivity = 0.1f;
			break;

		case(SPHERE):
			objArr[i].scale = 1.2f;
			objArr[i].translation = float3(-30.0, -50.0, 30.0);
			mtlArr[i].type = 0b1000;
			mtlArr[i].transmittivity = 0.99f;

			break;
		case(M_DODECAHEDRON):
			objArr[i].translation = float3(-30.0f, 40.0f, 25.0f);
			objArr[i].scale = 0.25f;
			// objArr[i].rotation = getRotationAsQuternion({ 0.7, 1, 0 }, 30.0f);
			mtlArr[i].type = 0b1000;
			mtlArr[i].albedo = float3(0.1, 0.1, 0.7);
			mtlArr[i].roughness = 0.3f;
			mtlArr[i].reflectivity = 0.1f;
			break;
		
		case(M_MUSCHROOM):
			//objArr[i].translation = float3(80.0f, 0.0f, 20.0f); // Comparison of Hidden Surface Removal
			//objArr[i].scale = 2.8f;
			objArr[i].translation = float3(-30.0f, 45.0f, 20.0f);
			objArr[i].scale = 5.0f;
			objArr[i].twoSided = true;
			objArr[i].textureIdx = 0;
			mtlArr[i].type = Glass;
			mtlArr[i].albedo = float3(1.0f);
			mtlArr[i].reflectivity = 0.01f;
			mtlArr[i].roughness = 0.02f;
			TexMat = cv::imread("./data/img/muschroom_shader.png", cv::IMREAD_UNCHANGED);
			break;

		case(M_HAND):
			objArr[i].translation = float3(-30.0f, 45.0f, 20.0f);
			objArr[i].scale = 2.5f;
			objArr[i].twoSided = true;
			objArr[i].textureIdx = 0;
			mtlArr[i].type = Glass;
			mtlArr[i].albedo = float3(1.0f);
			mtlArr[i].reflectivity = 0.01f;
			mtlArr[i].roughness = 0.02f;
			TexMat = cv::imread("./data/img/BunnyTex.png", cv::IMREAD_UNCHANGED);
			break;

		case(M_CUP):
			//objArr[i].translation = float3(80.0f, 0.0f, 20.0f); // Comparison of Hidden Surface Removal
			//objArr[i].scale = 2.8f;
			objArr[i].translation = float3(-30.0f, 45.0f, 20.0f);
			objArr[i].scale = 400.0f;
			objArr[i].twoSided = true;
			objArr[i].textureIdx = 0;
			mtlArr[i].type = Glass;
			mtlArr[i].albedo = float3(1.0f);
			mtlArr[i].reflectivity = 0.01f;
			mtlArr[i].roughness = 0.02f;
			//TexMat = cv::imread("./data/img/Flat.png", cv::IMREAD_UNCHANGED);
			break;
		
		case(M_DEER):
			//objArr[i].translation = float3(-30.0f, 0.0f, 20.0f); // Comparison of Hidden Surface Removal
			objArr[i].translation = float3(80.0f, 0.0f, 20.0f);//objArr[i].scale = 2.8f;
			//objArr[i].translation = float3(-30.0f, 45.0f, 20.0f);
			objArr[i].scale = 40.0f;
			objArr[i].twoSided = true;
			objArr[i].textureIdx = 0;
			mtlArr[i].type = Glass;
			mtlArr[i].albedo = float3(1.0f);
			mtlArr[i].reflectivity = 0.01f;
			mtlArr[i].roughness = 0.02f;
			TexMat = cv::imread("./data/img/deer.png", cv::IMREAD_UNCHANGED);
			break;
		default:
			break;
		}
	}

	objArr.swap(scene->objArr);
	for (uint i = 0; i < numObjects; ++i)
		scene->objArr[i].materialIdx = i;

#endif


	// ------------------------< texture >------------------------
	cv::Mat materialTex = TexMat;
	cv::resize(materialTex, materialTex, cv::Size(1024, 1024));

	//cv::cvtColor(materialTex, materialTex, cv::COLOR_RGB2BGRA);
	texArr.resize(1);
	texArr[0].format = DXGI_FORMAT_B8G8R8A8_UNORM;
	texArr[0].width = 1024;
	texArr[0].height = 1024;
	texArr[0].dataSize = texArr[0].width * texArr[0].height * _bpp(DXGI_FORMAT_B8G8R8A8_UNORM);
	texArr[0].img = materialTex;
	//texArr[0].img = cv::Mat(cv::Size(texArr[0].width, texArr[0].height), CV_8UC4, cv::Scalar(0, 0, 0, 255));
	texArr.swap(scene->texArr);

	return scene;
}