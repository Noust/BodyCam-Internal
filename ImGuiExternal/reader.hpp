#pragma once
#include "include.h"

template<typename T>
class MemoryReader {
private:
    // Verifica si el tipo es válido para leer
    static constexpr bool IsValidType() {
        return std::is_trivially_copyable<T>::value;
    }

    static bool IsAddressValid(uintptr_t address, size_t size) {
        // Verificar rango de direcciones válido
        if (address == 0 || address == UINTPTR_MAX || 
            address + size < address || // Overflow check
            address + size > UINTPTR_MAX) {
            return false;
        }

        // Verificar que el rango completo está en la misma página de memoria
        MEMORY_BASIC_INFORMATION mbi;
        if (!VirtualQuery(reinterpret_cast<LPCVOID>(address), &mbi, sizeof(mbi))) {
            return false;
        }

        // Verificar que todo el rango está dentro de la región
        if (address + size > (uintptr_t)mbi.BaseAddress + mbi.RegionSize) {
            return false;
        }

        // Verificar permisos de memoria
        if (!(mbi.State == MEM_COMMIT &&
            mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE))) {
            return false;
        }

        return true;
    }

public:
    static bool ReadMemory(uintptr_t address, T& output) {
        // Verificaciones iniciales
        if (!IsValidType()) {
            return false;
        }

        if (!IsAddressValid(address, sizeof(T))) {
            return false;
        }

        __try {
            const T* ptr = reinterpret_cast<const T*>(address);

            // Verificar alineación de memoria
            if (reinterpret_cast<uintptr_t>(ptr) % alignof(T) != 0) {
                return false;
            }

            output = {}; // Inicializar output antes de la lectura
            memcpy(&output, ptr, sizeof(T));
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            output = {}; // Limpiar output en caso de excepción
            return false;
        }
    }

    // Sobrecarga que retorna un valor por defecto si falla
    static T ReadMemorySafe(uintptr_t address, const T& defaultValue = T()) {
        T result;
        return ReadMemory(address, result) ? result : defaultValue;
    }
};

template<typename T>
inline bool read(uintptr_t address, T& output) {
    return MemoryReader<T>::ReadMemory(address, output);
}

/*template<typename T>
inline T readIn(uintptr_t address, const T& defaultValue = T()) {
    return MemoryReader<T>::ReadMemorySafe(address, defaultValue);
}*/

template<typename T>
class MemoryWriter {
private:
    static constexpr bool IsValidType() {
        return std::is_trivially_copyable<T>::value;
    }

    // Función auxiliar para cambiar la protección de memoria
    static bool ModifyMemoryProtection(uintptr_t address, size_t size, DWORD newProtection, DWORD& oldProtection) {
        return VirtualProtect(reinterpret_cast<LPVOID>(address), size, newProtection, &oldProtection);
    }

public:
    static bool WriteMemory(uintptr_t address, const T& value) {
        // Verificaciones iniciales
        if (!IsValidType()) {
            return false;
        }

        if (address == 0 || address == UINTPTR_MAX) {
            return false;
        }

        // Verificar alineación
        if (address % alignof(T) != 0) {
            return false;
        }

        DWORD oldProtection;
        bool protectionChanged = false;

        __try {
            // Verificar permisos de memoria actuales
            MEMORY_BASIC_INFORMATION mbi;
            if (!VirtualQuery(reinterpret_cast<LPCVOID>(address), &mbi, sizeof(mbi))) {
                return false;
            }

            // Si la memoria no es escribible, intentar cambiar la protección
            if (!(mbi.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE))) {
                if (!ModifyMemoryProtection(address, sizeof(T), PAGE_EXECUTE_READWRITE, oldProtection)) {
                    return false;
                }
                protectionChanged = true;
            }

            // Escribir el valor
            *reinterpret_cast<T*>(address) = value;

            // Restaurar la protección original si fue cambiada
            if (protectionChanged) {
                ModifyMemoryProtection(address, sizeof(T), oldProtection, oldProtection);
            }

            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            if (protectionChanged) {
                ModifyMemoryProtection(address, sizeof(T), oldProtection, oldProtection);
            }
            return false;
        }
    }

    // Versión para escribir arrays
    static bool WriteMemoryArray(uintptr_t address, const T* values, size_t count) {
        if (!values || count == 0) {
            return false;
        }

        DWORD oldProtection;
        bool protectionChanged = false;

        __try {
            // Cambiar protección para todo el array
            if (!ModifyMemoryProtection(address, sizeof(T) * count, PAGE_EXECUTE_READWRITE, oldProtection)) {
                return false;
            }
            protectionChanged = true;

            // Copiar datos
            memcpy(reinterpret_cast<void*>(address), values, sizeof(T) * count);

            // Restaurar protección
            ModifyMemoryProtection(address, sizeof(T) * count, oldProtection, oldProtection);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            if (protectionChanged) {
                ModifyMemoryProtection(address, sizeof(T) * count, oldProtection, oldProtection);
            }
            return false;
        }
    }

    // Versión para escribir bytes (útil para patches)
    static bool WriteBytes(uintptr_t address, const std::vector<uint8_t>& bytes) {
        return WriteMemoryArray(address, bytes.data(), bytes.size());
    }
};

// Funciones helper para uso más simple
template<typename T>
inline bool write(uintptr_t address, const T& value) {
    return MemoryWriter<T>::WriteMemory(address, value);
}

template<typename T>
inline bool WriteMemArraySafe(uintptr_t address, const T* values, size_t count) {
    return MemoryWriter<T>::WriteMemoryArray(address, values, count);
}

inline bool WriteByteSafe(uintptr_t address, const std::vector<uint8_t>& bytes) {
    return MemoryWriter<uint8_t>::WriteBytes(address, bytes);
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
    return read<uintptr_t>(Uworld, adresses.uworld);
}

bool ReadGameInstance() {
    if (adresses.uworld == 0) return false;
    return read<DWORD64>(adresses.uworld + offset::game_instance, adresses.game_instance);
}

bool ReadLocalPlayer() {
    if (adresses.game_instance == 0) return false;
    DWORD64 localPlayerPtr = 0;
    if (!read<DWORD64>(adresses.game_instance + offset::local_player, localPlayerPtr)) return false;
    if (localPlayerPtr == 0) return false;
    return read<DWORD64>(localPlayerPtr, adresses.local_player);
}

bool ReadPlayerController() {
    if (adresses.local_player == 0) return false;
    return read<DWORD64>(adresses.local_player + offset::player_controller, adresses.player_controller);
}

bool ReadAcknowledgedPawn() {
    if (adresses.player_controller == 0) return false;
    return read<DWORD64>(adresses.player_controller + offset::acknowledged_pawn, adresses.acknowledged_pawn);
}

bool ReadPlayerState() {
    if (adresses.acknowledged_pawn == 0) return false;
    return read<DWORD64>(adresses.acknowledged_pawn + offset::player_state, adresses.player_state);
}

bool ReadGameState() {
    if (adresses.uworld == 0) return false;
    return read<DWORD64>(adresses.uworld + offset::game_state, adresses.game_state);
}

bool ReadPlayerArray() {
    if (adresses.game_state == 0) return false;
    return read<DWORD64>(adresses.game_state + offset::player_array, adresses.player_array);
}