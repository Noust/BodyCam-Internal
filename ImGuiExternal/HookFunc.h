#pragma once
#include "include.h"

enum offset {
	persistent_level        = 0x30,
	game_state              = 0x160,
	game_instance           = 0x1D8,

	local_player            = 0x38,
	player_controller       = 0x30,
	aspect_axis_constraint  = 0xB8,

	acknowledged_pawn       = 0x350,
	camera_manager          = 0x360,

	default_fov             = 0x2C0,
	camera_cache            = 0x1410,
	pov_info                = 0x1420,
	camera_cache_last       = 0x1C50,
	pov_info_last           = 0x1C60,

	mvi_location            = 0x00,
	mvi_rotation            = 0x18,
	mvi_fov                 = 0x30,
	mvi_aspect_ratio        = 0x5C,
	mvi_flags               = 0x68,

	player_array            = 0x2C0,
	player_array_data       = 0x00,
	player_array_num        = 0x08,
	player_array_stride     = 0x08,

	ps_pawn                 = 0x320,
	ps_name                 = 0x340,

	ps_team_id              = 0x388,
	ps_kills                = 0x38C,
	ps_deaths               = 0x390,

	root_component          = 0x1B8,

	relative_location       = 0x128,
	relative_rotation       = 0x140,
	velocity                = 0x170,
	component_to_world      = 0x1D0,
	c2w_translation         = 0x1F0,
	c2w_scale               = 0x210,

	remote_view_pitch       = 0x2BA,
	player_state            = 0x2C8,
	pawn_controller         = 0x2D8,
	control_rotation        = 0x320,

	rotation_input          = 0x528,
	rotation_input_pitch    = 0x528,
	rotation_input_yaw      = 0x530,
	rotation_input_roll     = 0x538,
	input_yaw_scale         = 0x540,
	input_pitch_scale       = 0x544,
	input_roll_scale        = 0x548,
	pc_player_input         = 0x420,
	pc_target_view_rotation = 0x378,

	skeletal_mesh_component = 0x328,
	character_movement      = 0x330,
	capsule_component       = 0x338,

	capsule_half_height     = 0x508,
	capsule_radius          = 0x50C,

	skeletal_mesh_asset_old = 0x520,
	skinned_asset           = 0x528,
	leader_pose_component   = 0x530,
	active_bone_array       = 0x598,
	bone_array_stride       = 0x10,
	bone_edit_index         = 0x5DC,
	bone_buffer_index       = 0x5E0,
	leader_bone_map         = 0x608,
	bone_stride             = 0x60,

	ft_rotation             = 0x00,
	ft_translation          = 0x20,
	ft_scale                = 0x40,

	bc_ability_system       = 0x658,
	bc_pawn_ext             = 0x660,
	bc_character_set        = 0x668,

	attr_health             = 0x88,
	attr_max_health         = 0x98,
	attr_gadget_cooldown    = 0xA8,
	attr_healing            = 0xB8,
	attr_damage             = 0xC8,
	attr_current_value      = 0x0C,
	attr_base_value         = 0x08,

	health_current          = 0x94,
	max_health_current      = 0xA4,

	refskel_raw_bone_info   = 0x00,
	refskel_raw_bone_pose   = 0x10,
	refskel_final_bone_info = 0x20,
	refskel_final_bone_pose = 0x30,
	mesh_bone_info_stride   = 0x0C,
	mesh_bone_info_parent   = 0x08,
};

enum EAspectAxis : int {
	AspectAxis_MaintainYFOV   = 0,
	AspectAxis_MaintainXFOV   = 1,
	AspectAxis_MajorAxisFOV   = 2,
};

namespace limits {
	inline constexpr int   kMaxPlayers   = 128;
	inline constexpr int   kMaxBones     = 1024;
	inline constexpr int   kMaxNameLen   = 64;
	inline constexpr int   kMaxParents   = 256;
	inline constexpr float kMinHealth    = 0.0f;
	inline constexpr float kMaxHealth    = 100000.0f;
}

class hookclass {
public:
	bool Detour32(void* src, void* dst, int len);
	void Patch(BYTE* addr, BYTE* bytes, unsigned int size);
	bool Hook(BYTE* pTarget, BYTE* pHook, UINT Length);
	MODULEINFO GetModuleInfo(char* szModule);
	DWORD64 FindPattern(char* module, char* pattern, char* mask);
	void GetAddreses();
};

inline hookclass* hooks = nullptr;
