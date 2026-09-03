#pragma once
#include "include.h"

inline bool IsValidPtr(uintptr_t p) {
    return p >= 0x10000 && p < 0x00007FFFFFFFFFFFull;
}

__declspec(noinline) inline bool SafeCopy(void* dst, const void* src, size_t n) {
    __try {
        memcpy(dst, src, n);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

template<typename T>
inline bool read(uintptr_t address, T& output) {
    static_assert(std::is_trivially_copyable<T>::value, "read<T> requires a trivially copyable type");
    output = T{};
    if (!IsValidPtr(address)) return false;
    if (!IsValidPtr(address + sizeof(T) - 1)) return false;
    return SafeCopy(&output, reinterpret_cast<const void*>(address), sizeof(T));
}

template<typename T>
inline bool readRaw(uintptr_t address, T& output) {
    if (!IsValidPtr(address)) return false;
    if (!IsValidPtr(address + sizeof(T) - 1)) return false;
    return SafeCopy(&output, reinterpret_cast<const void*>(address), sizeof(T));
}

inline bool readPtr(uintptr_t address, uintptr_t& out) {
    out = 0;
    if (!read<uintptr_t>(address, out)) return false;
    if (!IsValidPtr(out)) { out = 0; return false; }
    return true;
}

inline bool readBytes(uintptr_t address, void* dst, size_t n) {
    if (!dst || n == 0) return false;
    if (!IsValidPtr(address)) return false;
    if (!IsValidPtr(address + n - 1)) return false;
    return SafeCopy(dst, reinterpret_cast<const void*>(address), n);
}

__declspec(noinline) inline bool SafeWrite(void* dst, const void* src, size_t n) {
    __try {
        memcpy(dst, src, n);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

template<typename T>
inline bool write(uintptr_t address, const T& value) {
    static_assert(std::is_trivially_copyable<T>::value, "write<T> requires a trivially copyable type");
    if (!IsValidPtr(address)) return false;
    if (!IsValidPtr(address + sizeof(T) - 1)) return false;
    if (address % alignof(T) != 0) return false;
    return SafeWrite(reinterpret_cast<void*>(address), &value, sizeof(T));
}

enum class ChainStage : int {
    Ok = 0,
    NoGWorld,
    NoWorld,
    NoGameInstance,
    NoLocalPlayer,
    NoPlayerController,
    NoCameraManager,
    NoGameState,
    NoPlayerArray,
    NoLocalPawn,
};

inline const char* ChainStageName(ChainStage s) {
    switch (s) {
    case ChainStage::Ok:                 return "OK";
    case ChainStage::NoGWorld:           return "GWorld address is null (bad RVA?)";
    case ChainStage::NoWorld:            return "UWorld not loaded (main menu?)";
    case ChainStage::NoGameInstance:     return "UWorld->OwningGameInstance failed";
    case ChainStage::NoLocalPlayer:      return "GameInstance->LocalPlayers[0] failed";
    case ChainStage::NoPlayerController: return "LocalPlayer->PlayerController failed";
    case ChainStage::NoCameraManager:    return "PlayerController->PlayerCameraManager failed";
    case ChainStage::NoGameState:        return "UWorld->GameState failed";
    case ChainStage::NoPlayerArray:      return "GameState->PlayerArray failed";
    case ChainStage::NoLocalPawn:        return "No local pawn (dead / spectating)";
    }
    return "?";
}

inline bool ValidateWorldStrict(uintptr_t w) {
    if (!IsValidPtr(w) || (w & 7)) return false;

    uintptr_t gi = 0;
    if (!readPtr(w + offset::game_instance, gi)) return false;

    uintptr_t lpData = 0;
    int lpNum = 0;
    if (!readPtr(gi + offset::local_player, lpData)) return false;
    if (!read<int>(gi + offset::local_player + 0x08, lpNum)) return false;
    if (lpNum < 1 || lpNum > 8) return false;

    uintptr_t lp = 0;
    if (!readPtr(lpData, lp)) return false;

    uintptr_t pc = 0;
    if (!readPtr(lp + offset::player_controller, pc)) return false;

    uintptr_t cam = 0;
    if (!readPtr(pc + offset::camera_manager, cam)) return false;

    return true;
}

inline bool ValidateWorldLoose(uintptr_t w) {
    if (!IsValidPtr(w) || (w & 7)) return false;
    uintptr_t gi = 0;
    if (!readPtr(w + offset::game_instance, gi)) return false;
    uintptr_t lpData = 0;
    int lpNum = 0;
    if (!readPtr(gi + offset::local_player, lpData)) return false;
    if (!read<int>(gi + offset::local_player + 0x08, lpNum)) return false;
    return lpNum >= 1 && lpNum <= 8;
}

struct WorldScanInfo {
    uintptr_t globalAddr = 0;
    uintptr_t worldPtr = 0;
    int       candidates = 0;
    bool      strict = false;
    bool      done = false;
    DWORD     scanMs = 0;
};
inline WorldScanInfo g_WorldScan;

inline uintptr_t ScanForGWorld() {
    const HMODULE mod = GetModuleHandleA("Bodycam-Win64-Shipping.exe");
    if (!mod) return 0;

    const DWORD t0 = GetTickCount();
    const uintptr_t base = reinterpret_cast<uintptr_t>(mod);

    IMAGE_DOS_HEADER dos{};
    if (!read<IMAGE_DOS_HEADER>(base, dos) || dos.e_magic != IMAGE_DOS_SIGNATURE) return 0;

    IMAGE_NT_HEADERS64 nt{};
    if (!read<IMAGE_NT_HEADERS64>(base + dos.e_lfanew, nt) || nt.Signature != IMAGE_NT_SIGNATURE)
        return 0;

    const int nsec = nt.FileHeader.NumberOfSections;
    if (nsec <= 0 || nsec > 96) return 0;
    const uintptr_t secBase = base + dos.e_lfanew + sizeof(IMAGE_NT_HEADERS64);

    static std::vector<uint8_t> buf;
    buf.resize(0x10000);

    uintptr_t looseHit = 0;
    int candidates = 0;

    for (int i = 0; i < nsec; ++i) {
        IMAGE_SECTION_HEADER sh{};
        if (!read<IMAGE_SECTION_HEADER>(secBase + (uintptr_t)i * sizeof(IMAGE_SECTION_HEADER), sh))
            continue;
        if (!(sh.Characteristics & IMAGE_SCN_MEM_WRITE)) continue;
        if (sh.Characteristics & IMAGE_SCN_MEM_EXECUTE) continue;

        const uintptr_t start = base + sh.VirtualAddress;
        size_t size = sh.Misc.VirtualSize;
        if (size == 0 || size > 0x8000000) continue;

        for (size_t off = 0; off < size; off += 0x10000) {
            const size_t chunk = (size - off > 0x10000) ? 0x10000 : (size - off);
            if (!readBytes(start + off, buf.data(), chunk)) continue;

            const size_t n = chunk / 8;
            const uint64_t* q = reinterpret_cast<const uint64_t*>(buf.data());
            for (size_t k = 0; k < n; ++k) {
                const uintptr_t cand = (uintptr_t)q[k];
                if (cand < 0x10000 || cand >= 0x00007FFFFFFFFFFFull) continue;
                if (cand & 7) continue;
                ++candidates;
                if (ValidateWorldStrict(cand)) {
                    g_WorldScan.globalAddr = start + off + k * 8;
                    g_WorldScan.worldPtr = cand;
                    g_WorldScan.candidates = candidates;
                    g_WorldScan.strict = true;
                    g_WorldScan.done = true;
                    g_WorldScan.scanMs = GetTickCount() - t0;
                    return g_WorldScan.globalAddr;
                }
                if (!looseHit && ValidateWorldLoose(cand))
                    looseHit = start + off + k * 8;
            }
        }
    }

    g_WorldScan.candidates = candidates;
    g_WorldScan.scanMs = GetTickCount() - t0;
    g_WorldScan.done = true;
    if (looseHit) {
        g_WorldScan.globalAddr = looseHit;
        readPtr(looseHit, g_WorldScan.worldPtr);
        g_WorldScan.strict = false;
        return looseHit;
    }
    return 0;
}

inline int g_WorldFailStreak = 0;

inline uintptr_t ResolveGWorld() {
    if (Uworld) {
        uintptr_t w = 0;
        if (readPtr(Uworld, w) && (ValidateWorldStrict(w) || ValidateWorldLoose(w))) {
            g_WorldFailStreak = 0;
            return Uworld;
        }
        if (++g_WorldFailStreak < 20) return Uworld;
    }

    static DWORD s_last = 0;
    static DWORD s_wait = 500;
    const DWORD now = GetTickCount();
    if (s_last != 0 && now - s_last < s_wait) return Uworld;
    s_last = now;

    const uintptr_t found = ScanForGWorld();
    if (found) {
        Uworld = found;
        g_WorldFailStreak = 0;
        s_wait = 500;
    }
    else {
        Uworld = 0;
        s_wait = (s_wait < 5000) ? s_wait * 2 : 5000;
    }
    return Uworld;
}

struct world {
    uintptr_t uworld;
    uintptr_t game_instance;
    uintptr_t local_player;
    uintptr_t player_controller;
    uintptr_t camera_manager;
    uintptr_t acknowledged_pawn;
    uintptr_t player_state;
    uintptr_t attribute_set;
    uintptr_t game_state;
    uintptr_t player_array;
    int       player_count;
    int       local_team;
    uint8_t   aspect_axis;
    ChainStage stage;
};
inline world adresses;

inline bool ReadValues() {
    world w{};
    w.stage = ChainStage::Ok;
    w.local_team = -1;

    const uintptr_t gw = ResolveGWorld();
    if (!gw) { adresses = w; adresses.stage = ChainStage::NoGWorld; return false; }

    if (!readPtr(gw, w.uworld)) { w.stage = ChainStage::NoWorld; adresses = w; return false; }

    if (!readPtr(w.uworld + offset::game_instance, w.game_instance)) {
        w.stage = ChainStage::NoGameInstance; adresses = w; return false;
    }

    uintptr_t lpData = 0;
    int lpNum = 0;
    if (!readPtr(w.game_instance + offset::local_player, lpData) ||
        !read<int>(w.game_instance + offset::local_player + 0x08, lpNum) ||
        lpNum <= 0 || lpNum > 8 ||
        !readPtr(lpData, w.local_player)) {
        w.stage = ChainStage::NoLocalPlayer; adresses = w; return false;
    }

    read<uint8_t>(w.local_player + offset::aspect_axis_constraint, w.aspect_axis);

    if (!readPtr(w.local_player + offset::player_controller, w.player_controller)) {
        w.stage = ChainStage::NoPlayerController; adresses = w; return false;
    }

    if (!readPtr(w.player_controller + offset::camera_manager, w.camera_manager)) {
        w.stage = ChainStage::NoCameraManager; adresses = w; return false;
    }

    if (!readPtr(w.uworld + offset::game_state, w.game_state)) {
        w.stage = ChainStage::NoGameState; adresses = w; return false;
    }

    if (!readPtr(w.game_state + offset::player_array, w.player_array) ||
        !read<int>(w.game_state + offset::player_array + offset::player_array_num, w.player_count)) {
        w.stage = ChainStage::NoPlayerArray; adresses = w; return false;
    }
    if (w.player_count < 0 || w.player_count > limits::kMaxPlayers) w.player_count = 0;

    w.local_team = -1;
    if (readPtr(w.player_controller + offset::acknowledged_pawn, w.acknowledged_pawn)) {
        readPtr(w.acknowledged_pawn + offset::bc_character_set, w.attribute_set);
        if (readPtr(w.acknowledged_pawn + offset::player_state, w.player_state)) {
            if (!read<int>(w.player_state + offset::ps_team_id, w.local_team))
                w.local_team = -1;
        }
    }
    else {
        w.stage = ChainStage::NoLocalPawn;
    }

    adresses = w;
    return true;
}

inline bool ReadHealth(uintptr_t pawn, float& health, float& maxHealth) {
    health = 0.0f; maxHealth = 0.0f;
    uintptr_t attrs = 0;
    if (!readPtr(pawn + offset::bc_character_set, attrs)) return false;
    if (!read<float>(attrs + offset::health_current, health)) return false;
    read<float>(attrs + offset::max_health_current, maxHealth);

    if (!(health == health) || health < limits::kMinHealth || health > limits::kMaxHealth) return false;
    if (!(maxHealth == maxHealth) || maxHealth <= 0.0f || maxHealth > limits::kMaxHealth)
        maxHealth = 100.0f;
    return true;
}

inline bool ReadPlayerName(uintptr_t playerState, char* out, size_t outSize) {
    if (!out || outSize == 0) return false;
    out[0] = '\0';

    uintptr_t data = 0;
    int num = 0;
    if (!readPtr(playerState + offset::ps_name, data)) return false;
    if (!read<int>(playerState + offset::ps_name + 0x08, num)) return false;
    if (num <= 1 || num > limits::kMaxNameLen) return false;

    wchar_t buf[limits::kMaxNameLen + 1] = {};
    const int chars = num - 1;
    if (!readBytes(data, buf, static_cast<size_t>(chars) * sizeof(wchar_t))) return false;
    buf[chars] = L'\0';

    const int n = WideCharToMultiByte(CP_UTF8, 0, buf, chars,
                                      out, static_cast<int>(outSize) - 1, nullptr, nullptr);
    if (n <= 0) { out[0] = '\0'; return false; }
    out[n] = '\0';
    return true;
}
