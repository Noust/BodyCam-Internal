#pragma once
#include "include.h"

struct camera_position_s {
	fvector location{};
	fvector rotation{};
	float fov{};
};
inline camera_position_s camera_postion{};
camera_position_s camera;

struct FNRot {
	double a, b, c;
};
FNRot fnRot;

fvector get_bone_3d(uintptr_t skeletal_mesh, int bone_index)
{
	DWORD64 bonearray = *(DWORD64*)(skeletal_mesh + 0x610);
	DWORD64 bonearray2 = *(DWORD64*)(skeletal_mesh + 0x620);

	DWORD64 temp_bone_ptr = !bonearray ? bonearray2 : bonearray;

	if (!temp_bone_ptr)
	{
		return fvector(0, 0, 0);
	}

	FTransform bone = *(FTransform*)(temp_bone_ptr + (bone_index * 0x60));

	FTransform ComponentToWorld = *(FTransform*)(skeletal_mesh + 0x240);

	D3DMATRIX Matrix;
	Matrix = MatrixMultiplication(bone.ToMatrixWithScale(), ComponentToWorld.ToMatrixWithScale());

	return fvector(Matrix._41, Matrix._42, Matrix._43);
}


camera_position_s get_camera() {
	if (!adresses.uworld || !offset::game_instance || !offset::local_player || !offset::player_controller || !adresses.player_controller) {
		return {};
	}

	DWORD64 location_pointer = *(DWORD64*)(adresses.uworld + 0x110);
	DWORD64 rotation_pointer = *(DWORD64*)(adresses.uworld + 0x120);

	if (!location_pointer || !rotation_pointer)
		return {};

	fnRot.a = *(double*)(rotation_pointer);
	fnRot.b = *(double*)(rotation_pointer + 0x20);
	fnRot.c = *(double*)(rotation_pointer + 0x1d0);

	camera.location = *(fvector*)(location_pointer);
	camera.rotation.x = asin(fnRot.c) * (180.0 / M_PI);
	camera.rotation.y = ((atan2(fnRot.a * -1, fnRot.b) * (180.0 / M_PI)) * -1) * -1;
	camera.fov = *(float*)((DWORD64)adresses.player_controller + 0x394) * 90.f;

	return camera;
}

bool is_visible(uintptr_t skeletal_mesh) {
	//despues de BoundsScale
	float last_submit = *(float*)(skeletal_mesh + 0x358);  // 0x358
	float last_render = *(float*)(skeletal_mesh + 0x360);  // 0x360
	return (bool)(last_render + 0.06f >= last_submit);
}

inline fvector2d w2s(fvector WorldLocation) {

	if (WorldLocation.x == 0 || WorldLocation.y == 0 || WorldLocation.z == 0)
		return fvector2d(0, 0);

	_MATRIX tempMatrix = Matrix(camera_postion.rotation);

	fvector vAxisX = fvector(tempMatrix.m[0][0], tempMatrix.m[0][1], tempMatrix.m[0][2]);
	fvector vAxisY = fvector(tempMatrix.m[1][0], tempMatrix.m[1][1], tempMatrix.m[1][2]);
	fvector vAxisZ = fvector(tempMatrix.m[2][0], tempMatrix.m[2][1], tempMatrix.m[2][2]);
	fvector vDelta = WorldLocation - camera_postion.location;
	fvector vTransformed = fvector(vDelta.dot(vAxisY), vDelta.dot(vAxisZ), vDelta.dot(vAxisX));

	if (vTransformed.z < 1.f)
		vTransformed.z = 1.f;

	if (widthscreen <= 0 || heightscreen <= 0) {
		return fvector2d(0, 0);
	}

	return fvector2d(
		(widthscreen / 2.0f) + vTransformed.x * (((widthscreen / 2.0f) / tanf(camera_postion.fov * (float)M_PI / 360.f))) / vTransformed.z,
		(heightscreen / 2.0f) - vTransformed.y * (((widthscreen / 2.0f) / tanf(camera_postion.fov * (float)M_PI / 360.f))) / vTransformed.z
	);
}

enum bone : uint32_t {
	Root = 0,
	left_foot = 69,
	left_foot_up = 68,
	left_knee = 65,
	left_pelvis = 64,
	penis = 1,
	right_pelvis = 73,
	right_knee = 74,
	right_foot_up = 85,
	right_foot = 78,
	up_penis = 2,
	stomage = 3,
	chest = 4,
	neck = 61,
	head = 62,
	right_shoulder = 59,
	right_elbow = 35,
	right_hand = 36,
	left_shoulder = 31,
	left_elbow = 7,
	left_hand = 25,
};
