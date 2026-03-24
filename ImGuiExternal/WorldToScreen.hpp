#pragma once
#include "include.h"

static char g_boneDebug[256] = "waiting...";

static DWORD64 resolve_mesh_component(DWORD64 meshComp)
{
	DWORD64 leader = 0;
	if (read<DWORD64>(meshComp + offset::leader_pose_component, leader) && leader != 0)
		return leader;
	return meshComp;
}

fvector get_bone_3d(uintptr_t pawn, int bone_index)
{
	DWORD64 meshComp = 0;
	if (!read<DWORD64>(pawn + offset::skeletal_mesh_component, meshComp) || !meshComp) {
		sprintf_s(g_boneDebug, "FAIL: no meshComp");
		return fvector(0, 0, 0);
	}

	DWORD64 boneMesh = resolve_mesh_component(meshComp);

	// Read active buffer index and try both double-buffer slots
	DWORD64 boneDataPtr = 0;
	int boneCount = 0;
	int bufferUsed = -1;

	int bufIdx = 0;
	read<int>(boneMesh + offset::bone_buffer_index, bufIdx);
	if (bufIdx < 0 || bufIdx > 1) bufIdx = 0;

	for (int attempt = 0; attempt < 2; attempt++) {
		int buf = (bufIdx + attempt) & 1;
		DWORD64 arrOff = offset::active_bone_array + (buf * 0x10);
		DWORD64 tmpPtr = 0; int tmpCount = 0;
		if (read<DWORD64>(boneMesh + arrOff, tmpPtr) && tmpPtr != 0 &&
			read<int>(boneMesh + arrOff + 0x8, tmpCount) && tmpCount > 0 &&
			bone_index >= 0 && bone_index < tmpCount) {
			boneDataPtr = tmpPtr;
			boneCount = tmpCount;
			bufferUsed = buf;
			break;
		}
	}

	// Fallback: CachedComponentSpaceTransforms
	if (!boneDataPtr) {
		DWORD64 tmpPtr = 0; int tmpCount = 0;
		if (read<DWORD64>(boneMesh + offset::cached_bone_array, tmpPtr) && tmpPtr != 0 &&
			read<int>(boneMesh + offset::cached_bone_array + 0x8, tmpCount) && tmpCount > 0 &&
			bone_index >= 0 && bone_index < tmpCount) {
			boneDataPtr = tmpPtr;
			boneCount = tmpCount;
			bufferUsed = 99;
		}
	}

	if (!boneDataPtr) {
		sprintf_s(g_boneDebug, "FAIL: no bone data (buf=%d)", bufIdx);
		return fvector(0, 0, 0);
	}

	FTransform bone{};
	if (!readRaw<FTransform>(boneDataPtr + (bone_index * offset::bone_stride), bone)) {
		sprintf_s(g_boneDebug, "FAIL: bone read idx=%d", bone_index);
		return fvector(0, 0, 0);
	}

	FTransform c2w{};
	if (!readRaw<FTransform>(meshComp + offset::component_to_world, c2w)) {
		sprintf_s(g_boneDebug, "FAIL: c2w read");
		return fvector(0, 0, 0);
	}

	D3DMATRIX result = MatrixMultiplication(bone.ToMatrixWithScale(), c2w.ToMatrixWithScale());

	sprintf_s(g_boneDebug, "OK buf=%d cnt=%d head(%.0f,%.0f,%.0f)",
		bufferUsed, boneCount, bone.translation.x, bone.translation.y, bone.translation.z);

	return fvector(result._41, result._42, result._43);
}

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
