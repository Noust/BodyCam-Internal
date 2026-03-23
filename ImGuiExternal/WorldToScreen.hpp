#pragma once
#include "include.h"

//fvector get_bone_3d(uintptr_t skeletal_mesh, int bone_index)
//{
//	DWORD64 bonearray;
//	DWORD64 bonearray2;
//
//	if (!read<DWORD64>(skeletal_mesh + offset::bone_array, bonearray) ||
//		!read<DWORD64>(skeletal_mesh + offset::bone_array_2, bonearray2)) {
//		return fvector(0, 0, 0);
//	}
//
//	DWORD64 temp_bone_ptr = !bonearray ? bonearray2 : bonearray;
//
//	if (!temp_bone_ptr)
//	{
//		return fvector(0, 0, 0);
//	}
//
//	FTransform bone = *(FTransform*)(temp_bone_ptr + (bone_index * offset::bone_stride));
//
//	FTransform ComponentToWorld = *(FTransform*)(skeletal_mesh + offset::component_to_world);
//
//	D3DMATRIX Matrix;
//	Matrix = MatrixMultiplication(bone.ToMatrixWithScale(), ComponentToWorld.ToMatrixWithScale());
//
//	return fvector(Matrix._41, Matrix._42, Matrix._43);
//}

//bool is_visible(uintptr_t skeletal_mesh) {
//	float last_submit;
//	float last_render;
//
//	if (!read<float>(skeletal_mesh + offset::last_submit_time, last_submit) ||
//		!read<float>(skeletal_mesh + offset::last_render_time, last_render)) {
//		return false;
//	}
//
//	return (bool)(last_render + 0.06f >= last_submit);
//}

inline fvector2d w2s(fvector WorldLocation) {

	if (WorldLocation.x == 0 || WorldLocation.y == 0 || WorldLocation.z == 0)
		return fvector2d(0, 0);

	D3DXMATRIX tempMatrix = Matrix(POV.Rotation);

	fvector vAxisX = fvector(tempMatrix.m[0][0], tempMatrix.m[0][1], tempMatrix.m[0][2]);
	fvector vAxisY = fvector(tempMatrix.m[1][0], tempMatrix.m[1][1], tempMatrix.m[1][2]);
	fvector vAxisZ = fvector(tempMatrix.m[2][0], tempMatrix.m[2][1], tempMatrix.m[2][2]);
	fvector vDelta = WorldLocation - POV.Location;
	fvector vTransformed = fvector(vDelta.dot(vAxisY), vDelta.dot(vAxisZ), vDelta.dot(vAxisX));

	if (vTransformed.z < 1.f)
		vTransformed.z = 1.f;

	if (widthscreen <= 0 || heightscreen <= 0) {
		return fvector2d(0, 0);
	}

	return fvector2d(
		(widthscreen / 2.0f) + vTransformed.x * (((widthscreen / 2.0f) / tanf(POV.FOV * (float)M_PI / 360.f))) / vTransformed.z,
		(heightscreen / 2.0f) - vTransformed.y * (((widthscreen / 2.0f) / tanf(POV.FOV * (float)M_PI / 360.f))) / vTransformed.z
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
