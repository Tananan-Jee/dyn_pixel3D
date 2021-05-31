#pragma once
#include "../util/pch.h"
#include "../util/IGRTframe/Scene.h"


class SceneManager
{
	Scene* scene = new Scene;

	void initializeGeometryFromMeshes(Scene* scene, const Array<Mesh*>& meshes);
	void computeModelMatrices(Scene* scene);

public:

	std::vector<int> object_number;
	Scene* getScene() const { return scene; }
	int setRotation(int objectId, float4 rot);
	int setTranslation(int objectId, float3 trans);
	Scene* update();
	Scene* initScene();
	enum ObjectType {
		BACK, 
		M_PANDA, M_BUNNY, M_HUMAN, M_MOUNTAIN, M_TREE, M_TOYPOODLE,
		CUBE, SPHERE,M_DODECAHEDRON, M_MUSCHROOM, M_HAND,M_CUP,M_DEER
	};
};
