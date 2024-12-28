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
	DWORD64 bonearray;
	DWORD64 bonearray2;

	if (!read<DWORD64>(skeletal_mesh + 0x610, bonearray) ||
		!read<DWORD64>(skeletal_mesh + 0x620, bonearray2)) {
		return fvector(0, 0, 0);
	}

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
	if (!adresses.uworld || !offset::game_instance || !offset::local_player ||
		!offset::player_controller || !adresses.player_controller) {
		return {};
	}

	DWORD64 location_pointer;
	DWORD64 rotation_pointer;

	if (!read<DWORD64>(adresses.uworld + 0x110, location_pointer) ||
		!read<DWORD64>(adresses.uworld + 0x120, rotation_pointer)) {
		return {};
	}

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
	float last_submit;
	float last_render;

	if (!read<float>(skeletal_mesh + 0x358, last_submit) ||
		!read<float>(skeletal_mesh + 0x360, last_render)) {
		return false;
	}

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
	left_foot = 53,
	left_foot_up = 52,
	left_knee = 50,
	left_pelvis = 49,
	penis = 1,
	right_pelvis = 55,
	right_knee = 56,
	right_foot_up = 58,
	right_foot = 59,
	up_penis = 2,
	stomage = 3,
	chest = 4,
	neck = 5,
	head = 48,
	right_shoulder = 27,
	right_elbow = 28,
	right_hand = 29,
	left_shoulder = 25,
	left_elbow = 7,
	left_hand = 21,
};
