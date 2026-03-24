#pragma once
#include "include.h"

enum offset {
	game_instance = 0x1D8,
	game_state = 0x160,
	local_player = 0x38,
	player_controller = 0x30,
	camera_manager = 0x360,
	acknowledged_pawn = 0x350,
	skeletal_mesh_component = 0x328,  // ACharacter::Mesh (USkeletalMeshComponent*)
	player_state = 0x2C8,
	root_component = 0x1B8,
	velocity = 0x170,
	relative_location = 0x128,
	relative_rotation = 0x140,
	player_array = 0x338,
	SurvivorStatus = 0x668,
	control_rotation = 0x320,

	// Bone system offsets (USkinnedMeshComponent / USceneComponent)
	component_to_world = 0x1D0,       // USceneComponent::ComponentToWorld (FTransform, 96 bytes)
	cached_bone_array = 0x960,         // USkinnedMeshComponent::CachedComponentSpaceTransforms (TArray<FTransform>) - cache, often empty
	active_bone_array = 0x598,         // USkinnedMeshComponent::ComponentSpaceTransformsArray[0] (TArray<FTransform>) - live double buffer
	bone_buffer_index = 0x5E0,         // USkinnedMeshComponent::CurrentReadComponentTransforms (int32 - 0 or 1)
	leader_pose_component = 0x530,     // USkinnedMeshComponent::LeaderPoseComponent (TObjectPtr<USkinnedMeshComponent>)
	bone_stride = 0x60,                // sizeof(FTransform) = 96 bytes

	health = 0xB0,
	team_index_pawn = 0xF49,
	pov_info = 0x1420,
	player_array_data = 0x8,
	player_array_stride = 0x318

};

class hookclass {
public:
	bool Detour32(void* src, void* dst, int len);
	void Patch(BYTE* addr, BYTE* bytes, unsigned int size);
	bool Hook(BYTE* pTarget, BYTE* pHook, UINT Length);
	MODULEINFO GetModuleInfo(char* szModule);
	DWORD64 FindPattern(char* module, char* pattern, char* mask);
	void GetAddreses();
};

hookclass* hooks;