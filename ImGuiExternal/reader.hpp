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

inline constexpr char kGameModuleName[] = "Bodycam-Win64-Shipping.exe";

inline uintptr_t GameModuleBase() {
    static uintptr_t s_base = 0;
    if (!s_base) s_base = reinterpret_cast<uintptr_t>(GetModuleHandleA(kGameModuleName));
    return s_base;
}

struct SectionRange {
    uintptr_t start = 0;
    size_t    size = 0;
};

inline constexpr int kMaxSections = 32;

inline int EnumSections(bool wantExecutable, SectionRange* out, int maxOut) {
    if (!out || maxOut <= 0) return 0;
    const uintptr_t base = GameModuleBase();
    if (!base) return 0;

    IMAGE_DOS_HEADER dos{};
    if (!read<IMAGE_DOS_HEADER>(base, dos) || dos.e_magic != IMAGE_DOS_SIGNATURE) return 0;
    if (dos.e_lfanew <= 0 || dos.e_lfanew > 0x1000) return 0;

    IMAGE_NT_HEADERS64 nt{};
    if (!read<IMAGE_NT_HEADERS64>(base + dos.e_lfanew, nt)) return 0;
    if (nt.Signature != IMAGE_NT_SIGNATURE) return 0;

    const int nsec = nt.FileHeader.NumberOfSections;
    if (nsec <= 0 || nsec > 96) return 0;
    const WORD optSize = nt.FileHeader.SizeOfOptionalHeader;
    if (optSize < sizeof(IMAGE_OPTIONAL_HEADER64) || optSize > 0x400) return 0;

    const uintptr_t secBase = base + dos.e_lfanew + 4 + sizeof(IMAGE_FILE_HEADER) + optSize;

    int n = 0;
    for (int i = 0; i < nsec && n < maxOut; ++i) {
        IMAGE_SECTION_HEADER sh{};
        if (!read<IMAGE_SECTION_HEADER>(secBase + static_cast<uintptr_t>(i) * sizeof(IMAGE_SECTION_HEADER), sh))
            continue;
        const bool isExec = (sh.Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0;
        const bool isWrite = (sh.Characteristics & IMAGE_SCN_MEM_WRITE) != 0;
        if (wantExecutable) { if (!isExec) continue; }
        else { if (isExec || !isWrite) continue; }

        size_t size = sh.Misc.VirtualSize;
        if (size == 0 || size > 0x10000000) continue;
        out[n].start = base + sh.VirtualAddress;
        out[n].size = size;
        ++n;
    }
    return n;
}

inline bool AddressInModuleData(uintptr_t addr) {
    SectionRange secs[kMaxSections];
    const int n = EnumSections(false, secs, kMaxSections);
    for (int i = 0; i < n; ++i)
        if (addr >= secs[i].start && addr < secs[i].start + secs[i].size) return true;
    return false;
}

inline constexpr size_t kScanChunk = 0x10000;
inline constexpr size_t kScanOverlap = 0x40;

using ChunkScanFn = bool (*)(const uint8_t* buf, size_t len, uintptr_t va, void* ctx);

inline void ScanCodeChunks(ChunkScanFn fn, void* ctx) {
    if (!fn) return;
    SectionRange secs[kMaxSections];
    const int n = EnumSections(true, secs, kMaxSections);
    if (n <= 0) return;

    static std::vector<uint8_t> buf;
    buf.resize(kScanChunk + kScanOverlap);

    for (int s = 0; s < n; ++s) {
        const uintptr_t start = secs[s].start;
        const size_t size = secs[s].size;
        for (size_t off = 0; off < size; off += kScanChunk) {
            size_t want = size - off;
            if (want > kScanChunk + kScanOverlap) want = kScanChunk + kScanOverlap;
            if (!readBytes(start + off, buf.data(), want)) continue;
            if (fn(buf.data(), want, start + off, ctx)) return;
        }
    }
}

inline uintptr_t FindDataSlotHolding(uintptr_t value) {
    if (!IsValidPtr(value)) return 0;
    SectionRange secs[kMaxSections];
    const int n = EnumSections(false, secs, kMaxSections);
    if (n <= 0) return 0;

    static std::vector<uint8_t> buf;
    buf.resize(kScanChunk);

    for (int s = 0; s < n; ++s) {
        const uintptr_t start = secs[s].start;
        const size_t size = secs[s].size;
        for (size_t off = 0; off < size; off += kScanChunk) {
            const size_t chunk = (size - off > kScanChunk) ? kScanChunk : (size - off);
            if (!readBytes(start + off, buf.data(), chunk)) continue;
            const size_t cnt = chunk / 8;
            const uint64_t* q = reinterpret_cast<const uint64_t*>(buf.data());
            for (size_t k = 0; k < cnt; ++k) {
                if (static_cast<uintptr_t>(q[k]) == value)
                    return start + off + k * 8;
            }
        }
    }
    return 0;
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
    WorldUnproven,
};

inline const char* ChainStageName(ChainStage s) {
    switch (s) {
    case ChainStage::Ok:                 return "OK";
    case ChainStage::NoGWorld:           return "No world anchor found";
    case ChainStage::NoWorld:            return "UWorld not loaded (main menu?)";
    case ChainStage::NoGameInstance:     return "UWorld->OwningGameInstance failed";
    case ChainStage::NoLocalPlayer:      return "GameInstance->LocalPlayers[0] failed";
    case ChainStage::NoPlayerController: return "LocalPlayer->PlayerController failed";
    case ChainStage::NoCameraManager:    return "PlayerController->PlayerCameraManager failed";
    case ChainStage::NoGameState:        return "UWorld->GameState failed";
    case ChainStage::NoPlayerArray:      return "GameState->PlayerArray failed";
    case ChainStage::NoLocalPawn:        return "No local pawn (dead / spectating)";
    case ChainStage::WorldUnproven:      return "World not confirmed yet (joining?)";
    }
    return "?";
}

struct RosterView {
    uintptr_t game_state = 0;
    uintptr_t data = 0;
    int       num = 0;
    bool      ok = false;
};

inline RosterView ReadRoster(uintptr_t worldPtr) {
    RosterView r;
    uintptr_t gs = 0;
    if (!readPtr(worldPtr + offset::game_state, gs)) return r;
    r.game_state = gs;

    uintptr_t data = 0;
    int num = 0;
    read<uintptr_t>(gs + offset::player_array + offset::player_array_data, data);
    if (!read<int>(gs + offset::player_array + offset::player_array_num, num)) return r;
    if (num < 0 || num > limits::kMaxPlayers) return r;
    if (num > 0 && !IsValidPtr(data)) return r;

    r.data = IsValidPtr(data) ? data : 0;
    r.num = num;
    r.ok = true;
    return r;
}

inline bool RosterContains(const RosterView& r, uintptr_t playerState) {
    if (!r.ok || !r.data || !playerState) return false;
    for (int i = 0; i < r.num; ++i) {
        uintptr_t e = 0;
        if (!readPtr(r.data + static_cast<uintptr_t>(i) * offset::player_array_stride, e)) continue;
        if (e == playerState) return true;
    }
    return false;
}

struct ChainProbe {
    uintptr_t game_instance = 0;
    uintptr_t local_player = 0;
    uintptr_t player_controller = 0;
    uintptr_t camera_manager = 0;
    uintptr_t player_state = 0;
    uintptr_t outer_world = 0;
    uint8_t   aspect_axis = 0;
    ChainStage stage = ChainStage::Ok;
};

inline bool ProbeFromSeed(uintptr_t seed, ChainProbe& p) {
    p = ChainProbe{};
    if (!IsValidPtr(seed) || (seed & 7)) { p.stage = ChainStage::NoWorld; return false; }

    if (!readPtr(seed + offset::game_instance, p.game_instance)) {
        p.stage = ChainStage::NoGameInstance; return false;
    }

    uintptr_t lpData = 0;
    int lpNum = 0;
    if (!readPtr(p.game_instance + offset::local_player, lpData) ||
        !read<int>(p.game_instance + offset::local_player + 0x08, lpNum) ||
        lpNum < 1 || lpNum > 8 ||
        !readPtr(lpData, p.local_player)) {
        p.stage = ChainStage::NoLocalPlayer; return false;
    }

    read<uint8_t>(p.local_player + offset::aspect_axis_constraint, p.aspect_axis);

    if (!readPtr(p.local_player + offset::player_controller, p.player_controller)) {
        p.stage = ChainStage::NoPlayerController; return false;
    }
    if (!readPtr(p.player_controller + offset::camera_manager, p.camera_manager)) {
        p.stage = ChainStage::NoCameraManager; return false;
    }

    readPtr(p.player_controller + offset::ac_player_state, p.player_state);

    uintptr_t level = 0;
    if (readPtr(p.player_controller + offset::uobject_outer, level))
        readPtr(level + offset::level_owning_world, p.outer_world);

    return true;
}

inline bool SeedIsUsable(uintptr_t seed) {
    if (!IsValidPtr(seed) || (seed & 7)) return false;
    uintptr_t gi = 0;
    if (!readPtr(seed + offset::game_instance, gi)) return false;
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
    bool      done = false;
    DWORD     scanMs = 0;
    int       tier = 0;
    int       fullScans = 0;
    int       reanchors = 0;
    int       rescues = 0;
};
inline WorldScanInfo g_WorldScan;

inline constexpr DWORD kWorldScanBudgetMs = 1000;

inline uintptr_t ScanForWorldAnchor() {
    const DWORD t0 = GetTickCount();

    SectionRange secs[kMaxSections];
    const int nsec = EnumSections(false, secs, kMaxSections);
    if (nsec <= 0) return 0;

    static std::vector<uint8_t> buf;
    buf.resize(kScanChunk);

    uintptr_t bestAddr = 0, bestWorld = 0;
    int bestTier = 0;
    int candidates = 0;
    bool outOfTime = false;

    for (int s = 0; s < nsec && bestTier < 3 && !outOfTime; ++s) {
        const uintptr_t start = secs[s].start;
        const size_t size = secs[s].size;
        for (size_t off = 0; off < size && bestTier < 3; off += kScanChunk) {
            if (GetTickCount() - t0 > kWorldScanBudgetMs) { outOfTime = true; break; }
            const size_t chunk = (size - off > kScanChunk) ? kScanChunk : (size - off);
            if (!readBytes(start + off, buf.data(), chunk)) continue;

            const size_t n = chunk / 8;
            const uint64_t* q = reinterpret_cast<const uint64_t*>(buf.data());
            for (size_t k = 0; k < n; ++k) {
                const uintptr_t cand = static_cast<uintptr_t>(q[k]);
                if (cand < 0x10000 || cand >= 0x00007FFFFFFFFFFFull) continue;
                if (cand & 7) continue;
                if (!SeedIsUsable(cand)) continue;
                ++candidates;

                int tier = 1;
                ChainProbe p;
                if (ProbeFromSeed(cand, p)) {
                    tier = 2;
                    if (p.player_state && RosterContains(ReadRoster(cand), p.player_state))
                        tier = 3;
                }
                if (tier > bestTier) {
                    bestTier = tier;
                    bestAddr = start + off + k * 8;
                    bestWorld = cand;
                    if (tier >= 3) break;
                }
            }
        }
    }

    g_WorldScan.candidates = candidates;
    g_WorldScan.scanMs = GetTickCount() - t0;
    g_WorldScan.done = true;
    g_WorldScan.tier = bestTier;
    g_WorldScan.fullScans++;
    if (bestAddr) {
        g_WorldScan.globalAddr = bestAddr;
        g_WorldScan.worldPtr = bestWorld;
    }
    return bestAddr;
}

inline bool RescanWorldAnchor() {
    static DWORD s_last = 0;
    static DWORD s_wait = 500;
    const DWORD now = GetTickCount();
    if (s_last != 0 && now - s_last < s_wait) return false;
    s_last = now;

    const uintptr_t found = ScanForWorldAnchor();
    if (found) {
        Uworld = found;
        s_wait = 500;
        return true;
    }

    uintptr_t cur = 0;
    if (!(Uworld && readPtr(Uworld, cur) && SeedIsUsable(cur)))
        Uworld = 0;

    s_wait = (s_wait < 5000) ? s_wait * 2 : 5000;
    return false;
}

inline constexpr int kRecoverLimitMin = 120;
inline constexpr int kRecoverLimitMax = 7680;

inline int g_RecoverStreak = 0;
inline int g_RecoverLimit = kRecoverLimitMin;

inline void NoteWorldHealthy() {
    g_RecoverStreak = 0;
    g_RecoverLimit = kRecoverLimitMin;
}

inline void NoteWorldUnhealthy() {
    if (++g_RecoverStreak < g_RecoverLimit) return;
    g_RecoverStreak = 0;
    if (RescanWorldAnchor() && g_RecoverLimit < kRecoverLimitMax)
        g_RecoverLimit *= 2;
}

inline void ReanchorTo(uintptr_t worldPtr) {
    static DWORD s_last = 0;
    const DWORD now = GetTickCount();
    if (s_last != 0 && now - s_last < 2000) return;
    s_last = now;

    const uintptr_t slot = FindDataSlotHolding(worldPtr);
    if (!slot) return;
    Uworld = slot;
    g_WorldScan.globalAddr = slot;
    g_WorldScan.worldPtr = worldPtr;
    g_WorldScan.reanchors++;
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
    bool      proven;
    ChainStage stage;
};
inline world adresses;

inline bool ReadValues() {
    world w{};
    w.stage = ChainStage::Ok;
    w.local_team = -1;

    uintptr_t seed = 0;
    ChainProbe p;

    bool haveSeed = (Uworld != 0) && readPtr(Uworld, seed) && SeedIsUsable(seed);
    if (!haveSeed) {
        RescanWorldAnchor();
        seed = 0;
        haveSeed = (Uworld != 0) && readPtr(Uworld, seed) && SeedIsUsable(seed);
        if (!haveSeed) {
            adresses = w;
            adresses.stage = ChainStage::NoGWorld;
            return false;
        }
    }

    if (!ProbeFromSeed(seed, p)) {
        NoteWorldUnhealthy();
        adresses = w;
        adresses.uworld = seed;
        adresses.game_instance = p.game_instance;
        adresses.local_player = p.local_player;
        adresses.stage = p.stage;
        return false;
    }

    uintptr_t chosen = seed;
    bool proven = false;

    RosterView roster = ReadRoster(seed);
    if (p.player_state && RosterContains(roster, p.player_state)) {
        proven = true;
    }
    else if (p.outer_world && p.outer_world != seed && IsValidPtr(p.outer_world)) {
        const RosterView alt = ReadRoster(p.outer_world);
        if (p.player_state && RosterContains(alt, p.player_state)) {
            chosen = p.outer_world;
            roster = alt;
            proven = true;
            g_WorldScan.rescues++;
            ReanchorTo(chosen);
        }
    }

    if (proven)
        NoteWorldHealthy();
    else if (p.player_state && roster.ok && roster.num > 0)
        NoteWorldUnhealthy();

    w.uworld = chosen;
    w.game_instance = p.game_instance;
    w.local_player = p.local_player;
    w.player_controller = p.player_controller;
    w.camera_manager = p.camera_manager;
    w.player_state = p.player_state;
    w.aspect_axis = p.aspect_axis;
    w.proven = proven;

    if (!roster.ok) {
        w.stage = roster.game_state ? ChainStage::NoPlayerArray : ChainStage::NoGameState;
        adresses = w;
        return false;
    }

    w.game_state = roster.game_state;
    w.player_array = roster.data;
    w.player_count = roster.num;

    if (p.player_state) {
        if (!read<int>(p.player_state + offset::ps_team_id, w.local_team))
            w.local_team = -1;
    }

    if (readPtr(w.player_controller + offset::acknowledged_pawn, w.acknowledged_pawn))
        readPtr(w.acknowledged_pawn + offset::bc_character_set, w.attribute_set);
    else
        w.stage = ChainStage::NoLocalPawn;

    if (!proven && w.stage == ChainStage::Ok)
        w.stage = ChainStage::WorldUnproven;

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
