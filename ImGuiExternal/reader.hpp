#pragma once
#include "include.h"

template <class T>
bool read(DWORD64 Addr, int offset, T& value) {
	__try {
		if (Addr == NULL || !Addr)
			return false;

		value = *(T*)(Addr + offset);

		if (value == NULL || !value)
			return false;

		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
	}
}

struct world {
	uintptr_t uworld;
	DWORD64 game_instance;
	DWORD64 local_player;
	DWORD64 player_controller;
	DWORD64 acknowledged_pawn;
	DWORD64 player_state;
	DWORD64 game_state;
	DWORD64 player_array;
};
world adresses;


bool ReadUWorld();
bool ReadGameInstance();
bool ReadLocalPlayer();
bool ReadPlayerController();
bool ReadAcknowledgedPawn();
bool ReadPlayerState();
bool ReadGameState();
bool ReadPlayerArray();

bool ReadValues() {
    if (!ReadUWorld())
        return false;

    if (!ReadGameInstance())
        return false;

    if (!ReadLocalPlayer())
        return false;

    if (!ReadPlayerController())
        return false;

    if (!ReadAcknowledgedPawn())
        return false;

    if (!ReadPlayerState())
        return false;

    if (!ReadGameState())
        return false;

    if (!ReadPlayerArray())
        return false;

    return true;
}

bool ReadUWorld() {
    return read<uintptr_t>(Uworld, 0, adresses.uworld);
}

bool ReadGameInstance() {
    return read<DWORD64>(adresses.uworld, offset::game_instance, adresses.game_instance);
}

bool ReadLocalPlayer() {
    return read<DWORD64>(*(DWORD64*)(adresses.game_instance + offset::local_player), 0, adresses.local_player);
}

bool ReadPlayerController() {
    return read<DWORD64>(adresses.local_player, offset::player_controller, adresses.player_controller);
}

bool ReadAcknowledgedPawn() {
    return read<DWORD64>(adresses.player_controller, offset::acknowledged_pawn, adresses.acknowledged_pawn);
}

bool ReadPlayerState() {
    return read<DWORD64>(adresses.acknowledged_pawn, offset::player_state, adresses.player_state);
}

bool ReadGameState() {
    return read<DWORD64>(adresses.uworld, offset::game_state, adresses.game_state);
}

bool ReadPlayerArray() {
    return read<DWORD64>(adresses.game_state, offset::player_array, adresses.player_array);
}