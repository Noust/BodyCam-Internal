#pragma once
#include "include.h"

enum offset {
	game_instance = 0x1b8,
	game_state = 0x158,
	local_player = 0x38,
	player_controller = 0x30,
	acknowledged_pawn = 0x338,
	skeletal_mesh = 0x318,
	player_state = 0x2B0,
	root_component = 0x198,
	velocity = 0x168,
	relative_location = 0x120,
	relative_rotation = 0x138,
	team_index = 0x1211,
	player_array = 0x2A8,
	pawn_private = 0x308,
	component_to_world = 0x240,
	b_Allow_Targeting = 0xe40,
	location_under_reticle = 0x2530,

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