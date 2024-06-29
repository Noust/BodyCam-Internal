#pragma once
#include "include.h"

struct camera_position_s {
	fvector location{};
	fvector rotation{};
	float fov{};
};
inline camera_position_s camera_postion{};

struct FNRot {
	double a, b, c;
};

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

	camera_position_s camera;

	DWORD64 location_pointer = *(DWORD64*)(adresses.uworld + 0x110);
	DWORD64 rotation_pointer = *(DWORD64*)(adresses.uworld + 0x120);

	if (!location_pointer || !rotation_pointer)
		return {};

	FNRot fnRot;
	fnRot.a = *(double*)(rotation_pointer);
	fnRot.b = *(double*)(rotation_pointer + 0x20);
	fnRot.c = *(double*)(rotation_pointer + 0x1d0);

	camera.location = *(fvector*)(location_pointer);
	camera.rotation.x = asin(fnRot.c) * (180.0 / M_PI);
	camera.rotation.y = ((atan2(fnRot.a * -1, fnRot.b) * (180.0 / M_PI)) * -1) * -1;
	camera.fov = *(float*)((DWORD64)adresses.player_controller + 0x394) * 90.f;

	return camera;
}

inline fvector2d w2s(fvector WorldLocation) {
	camera_postion = get_camera();

	if (WorldLocation.x == 0)
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
	pelvis = 1,
	spine_01 = 2,
	spine_02 = 3,
	spine_03 = 4,
	clavicle_l = 5,
	upperarm_l = 6,
	lowerarm_l = 7,
	hand_l = 8,
	index_01_l = 9,
	index_02_l = 10,
	index_03_l = 11,
	index_03_l_end = 12,
	middle_01_l = 13,
	middle_02_l = 14,
	middle_03_l = 15,
	middle_03_l_end = 16,
	pinky_01_l = 17,
	pinky_02_l = 18,
	pinky_03_l = 19,
	pinky_03_l_end = 20,
	ring_01_l = 21,
	ring_02_l = 22,
	ring_03_l = 23,
	ring_03_l_end = 24,
	thumb_01_l = 25,
	thumb_02_l = 26,
	thumb_03_l = 27,
	thumb_03_l_end = 28,
	lowerarm_twist_01_l = 29,
	lowerarm_twist_01_l_end = 30,
	upperarm_twist_01_l = 31,
	upperarm_twist_01_l_end = 32,
	clavicle_r = 33,
	upperarm_r = 34,
	lowerarm_r = 35,
	hand_r = 36,
	index_01_r = 37,
	index_02_r = 38,
	index_03_r = 39,
	index_03_r_end = 40,
	middle_01_r = 41,
	middle_02_r = 42,
	middle_03_r = 43,
	middle_03_r_end = 44,
	pinky_01_r = 45,
	pinky_02_r = 46,
	pinky_03_r = 47,
	pinky_03_r_end = 48,
	ring_01_r = 49,
	ring_02_r = 50,
	ring_03_r = 51,
	ring_03_r_end = 52,
	thumb_01_r = 53,
	thumb_02_r = 54,
	thumb_03_r = 55,
	thumb_03_r_end = 56,
	lowerarm_twist_01_r = 57,
	lowerarm_twist_01_r_end = 58,
	upperarm_twist_01_r = 59,
	upperarm_twist_01_r_end = 60,
	neck_01 = 61,
	head = 62,
	head_end = 63,
	thigh_l = 64,
	calf_l = 65,
	calf_twist_01_l = 66,
	calf_twist_01_l_end = 67,
	foot_l = 68,
	ball_l = 69,
	ball_l_end = 70,
	thigh_twist_01_l = 71,
	thigh_twist_01_l_end = 72,
	thigh_r = 73,
	calf_r = 74,
	calf_twist_01_r = 75,
	calf_twist_01_r_end = 76,
	foot_r = 77,
	ball_r = 78,
	ball_r_end = 79,
	thigh_twist_01_r = 80,
	thigh_twist_01_r_end = 81,
	ik_foot_root = 82,
	ik_foot_l = 83,
	ik_foot_l_end = 84,
	ik_foot_r = 85,
	ik_foot_r_end = 86,
	ik_hand_root = 87,
	ik_hand_gun = 88,
	ik_hand_l = 89,
	ik_hand_l_end = 90,
	ik_hand_r = 91,
	ik_hand_r_end = 92,
};