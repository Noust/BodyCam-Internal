#pragma once
#include "include.h"

/* ---------------------------------------------------------------------------
 *  Lectura de memoria in-process.
 *
 *  Esto corre DENTRO del juego: un fallo aqui tumba el juego, no al overlay.
 *  Reglas:
 *    - Nunca desreferenciar un puntero del juego directamente.
 *    - Validar rango primero (barato) y envolver el acceso en SEH (coste cero
 *      en x64 cuando no salta la excepcion, porque es table-driven).
 *    - __try/__except NO admite objetos con destructor en la misma funcion
 *      (error C2712), por eso SafeCopy esta aislada y no usa nada de C++.
 *
 *  El codigo anterior hacia un VirtualQuery por cada lectura. Con 20 huesos x
 *  10 jugadores eso son 200 syscalls por frame solo para el esqueleto.
 * ------------------------------------------------------------------------- */

 /* Rango canonico de user-mode en x64. Descarta nulos, valores centinela y
  * punteros con la mitad alta puesta (kernel), que es la basura mas comun. */
inline bool IsValidPtr(uintptr_t p) {
    return p >= 0x10000 && p < 0x00007FFFFFFFFFFFull;
}

/* Copia protegida. Sin objetos C++ dentro: requisito de __try/__except. */
__declspec(noinline) inline bool SafeCopy(void* dst, const void* src, size_t n) {
    __try {
        memcpy(dst, src, n);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

/* Lectura tipada. Deja output a cero si falla, para que el llamante nunca vea
 * datos del intento anterior. */
template<typename T>
inline bool read(uintptr_t address, T& output) {
    static_assert(std::is_trivially_copyable<T>::value, "read<T> requiere un tipo trivialmente copiable");
    output = T{};
    if (!IsValidPtr(address)) return false;
    if (!IsValidPtr(address + sizeof(T) - 1)) return false;
    return SafeCopy(&output, reinterpret_cast<const void*>(address), sizeof(T));
}

/* Igual que read<T>, pero para tipos que no son trivialmente copiables por
 * tener constructor (fvector, FMinimalViewInfo...). El layout sigue siendo POD. */
template<typename T>
inline bool readRaw(uintptr_t address, T& output) {
    if (!IsValidPtr(address)) return false;
    if (!IsValidPtr(address + sizeof(T) - 1)) return false;
    return SafeCopy(&output, reinterpret_cast<const void*>(address), sizeof(T));
}

/* Lee un puntero y lo valida de una vez. Es el patron mas repetido de todo el
 * proyecto: encadenar punteros sin comprobar cada eslabon es como se crashea. */
inline bool readPtr(uintptr_t address, uintptr_t& out) {
    out = 0;
    if (!read<uintptr_t>(address, out)) return false;
    if (!IsValidPtr(out)) { out = 0; return false; }
    return true;
}

/* Bloque arbitrario (arrays de huesos, etc). */
inline bool readBytes(uintptr_t address, void* dst, size_t n) {
    if (!dst || n == 0) return false;
    if (!IsValidPtr(address)) return false;
    if (!IsValidPtr(address + n - 1)) return false;
    return SafeCopy(dst, reinterpret_cast<const void*>(address), n);
}

/* --------------------------- ESCRITURA -----------------------------------
 * Solo la usa el aimbot sobre ControlRotation. Misma disciplina: validar y
 * proteger. No se cambia la proteccion de pagina: ControlRotation vive en el
 * heap del objeto y ya es escribible; si no lo fuera, forzarlo seria mas
 * peligroso que fallar.
 * ------------------------------------------------------------------------ */
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
    static_assert(std::is_trivially_copyable<T>::value, "write<T> requiere un tipo trivialmente copiable");
    if (!IsValidPtr(address)) return false;
    if (!IsValidPtr(address + sizeof(T) - 1)) return false;
    if (address % alignof(T) != 0) return false;
    return SafeWrite(reinterpret_cast<void*>(address), &value, sizeof(T));
}

/* --------------------------- CADENA DEL MUNDO ---------------------------- */

/* En que eslabon fallo la resolucion. Sin esto, un "no se ve nada" no se puede
 * diagnosticar: hay nueve sitios donde puede romperse. */
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
    NoLocalPawn,        /* no es fatal: pasa al estar muerto o en menu */
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

/* ---------------------------------------------------------------------------
 *  Localización de GWorld
 *
 *  Un RVA fijo para GWorld es frágil por dos motivos: cambia con cada parche, y
 *  derivarlo por estadística es traicionero. La global más leída del binario
 *  (29.689 lecturas, 1 escritura) resultó ser `__security_cookie`, no GWorld:
 *  ese perfil "muchas lecturas, una escritura" lo cumple el cookie, porque se
 *  carga en el prólogo de casi todas las funciones.
 *
 *  Así que en vez de confiar en un número, el mundo se BUSCA y se VALIDA:
 *  se recorren las secciones de datos del módulo y se prueba cada puntero
 *  candidato contra la cadena real del juego. Solo el UWorld verdadero tiene
 *  un GameInstance con LocalPlayers, con un PlayerController, con un
 *  PlayerCameraManager. Un falso positivo es prácticamente imposible.
 *
 *  Ventaja: sobrevive a las actualizaciones del juego sin tocar nada.
 * ------------------------------------------------------------------------- */

 /* Comprobación fuerte: exige la cadena hasta la cámara. Es la que vale en
  * partida y la que se usa para dar por bueno un candidato. */
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

/* Comprobación laxa: sirve en menú o carga, donde aún no hay
 * PlayerController. Solo se acepta si no apareció ningún candidato estricto. */
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

/* Resultado del último escaneo, para poder enseñarlo en el menú. */
struct WorldScanInfo {
    uintptr_t globalAddr = 0;   /* direccion de la global que contiene el UWorld* */
    uintptr_t worldPtr = 0;
    int       candidates = 0;
    bool      strict = false;
    bool      done = false;
    DWORD     scanMs = 0;
};
inline WorldScanInfo g_WorldScan;

/* Recorre las secciones de datos del módulo buscando la global de GWorld.
 * Se lee por bloques para no hacer una llamada protegida por cada qword. */
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
        /* Solo datos escribibles y no ejecutables: ahi viven las globales. */
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
                /* Prefiltro barato: puntero canónico, alineado y fuera del propio módulo. */
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

/* Devuelve la dirección de la global de GWorld, buscándola si hace falta.
 * Se revalida en cada uso: al cambiar de mapa el puntero cambia, pero la
 * global sigue siendo la misma, así que normalmente no hay que reescanear. */
inline int g_WorldFailStreak = 0;

inline uintptr_t ResolveGWorld() {
    /* 1) Si ya tenemos la global, revalidarla. Es el caso normal y es barato. */
    if (Uworld) {
        uintptr_t w = 0;
        if (readPtr(Uworld, w) && (ValidateWorldStrict(w) || ValidateWorldLoose(w))) {
            g_WorldFailStreak = 0;
            return Uworld;
        }
        /* Un fallo suelto no significa nada: al morir, reaparecer, entrar en un
         * dron o cambiar de zona hay frames en los que la cadena no resuelve.
         * Se toleran unos cuantos antes de dar la global por mala. */
        if (++g_WorldFailStreak < 20) return Uworld;
    }

    /* 2) Reescanear, con tope de frecuencia para no quemar CPU en el menu.
     *
     * El fallo anterior estaba aqui: si el escaneo no encontraba nada se
     * devolvia el `Uworld` viejo, que ya se sabia invalido, y como seguia
     * siendo != 0 el codigo se quedaba "encontrado pero fallando" para siempre.
     * Ahora, si no se encuentra mundo, la global se pone a cero y el siguiente
     * intento vuelve a escanear de verdad. */
    /* Espera progresiva. Un escaneo completo cuesta del orden de 150-200 ms; en
     * el menu principal no hay mundo que encontrar, y repetirlo dos veces por
     * segundo se comeria los FPS del overlay sin ganar nada. Se empieza en
     * 500 ms y se va doblando hasta 5 s mientras siga sin aparecer. En cuanto
     * se encuentra, la espera vuelve al minimo. */
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
        Uworld = 0;              /* nunca conservar un puntero que ya fallo */
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
    uintptr_t player_state;        /* PlayerState del pawn local     */
    uintptr_t attribute_set;       /* UCharacterAttributeSet local   */
    uintptr_t game_state;
    uintptr_t player_array;        /* Data del TArray                */
    int       player_count;
    int       local_team;
    uint8_t   aspect_axis;         /* ULocalPlayer::AspectRatioAxisConstraint */
    ChainStage stage;
};
inline world adresses;

/* Resuelve la cadena entera cada frame. Los punteros de objetos UE NUNCA se
 * cachean entre frames: el juego los reasigna al morir, cambiar de mapa o
 * entrar en espectador, y un puntero viejo apunta a memoria liberada. */
inline bool ReadValues() {
    world w{};
    w.stage = ChainStage::Ok;
    /* -1 = equipo desconocido. Debe quedar asi tambien cuando la cadena falla
     * antes de llegar al PlayerState; con 0 (el valor por defecto del struct)
     * los jugadores del equipo 0 pasarian por aliados. */
    w.local_team = -1;

    /* Localiza la global de GWorld, buscandola si aun no se conoce o si dejo
     * de apuntar a un mundo valido. */
    const uintptr_t gw = ResolveGWorld();
    if (!gw) { adresses = w; adresses.stage = ChainStage::NoGWorld; return false; }

    if (!readPtr(gw, w.uworld)) { w.stage = ChainStage::NoWorld; adresses = w; return false; }

    if (!readPtr(w.uworld + offset::game_instance, w.game_instance)) {
        w.stage = ChainStage::NoGameInstance; adresses = w; return false;
    }

    /* LocalPlayers es un TArray<ULocalPlayer*>: hay que leer Data y luego [0]. */
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

    /* El pawn local puede no existir (muerto, espectador, cargando). No es un
     * fallo de la cadena: el ESP sigue funcionando sin el. */
    w.local_team = -1;
    if (readPtr(w.player_controller + offset::acknowledged_pawn, w.acknowledged_pawn)) {
        readPtr(w.acknowledged_pawn + offset::bc_character_set, w.attribute_set);
        if (readPtr(w.acknowledged_pawn + offset::player_state, w.player_state)) {
            /* read<T> deja el destino a CERO si falla, no conserva el -1 de
             * arriba. Y 0 es un equipo valido: sin comprobar el retorno, un
             * fallo de lectura haria pasar por aliados a todos los del equipo 0.
             * -1 significa "equipo desconocido" y desactiva el filtro. */
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

/* Vida actual desde el AttributeSet de GAS. Devuelve false si no se pudo leer,
 * que es distinto de "vida 0": un jugador sin AttributeSet resuelto no debe
 * contarse como muerto. */
inline bool ReadHealth(uintptr_t pawn, float& health, float& maxHealth) {
    health = 0.0f; maxHealth = 0.0f;
    uintptr_t attrs = 0;
    if (!readPtr(pawn + offset::bc_character_set, attrs)) return false;
    if (!read<float>(attrs + offset::health_current, health)) return false;
    read<float>(attrs + offset::max_health_current, maxHealth);

    /* NaN e infinitos existen si el offset se desalinea tras un parche. */
    if (!(health == health) || health < limits::kMinHealth || health > limits::kMaxHealth) return false;
    if (!(maxHealth == maxHealth) || maxHealth <= 0.0f || maxHealth > limits::kMaxHealth)
        maxHealth = 100.0f;
    return true;
}

/* PlayerNamePrivate es un FString = TArray<wchar_t>. Se lee Data y Num, y se
 * acota: el codigo anterior copiaba 22 wchar_t a ciegas desde el puntero. */
inline bool ReadPlayerName(uintptr_t playerState, char* out, size_t outSize) {
    if (!out || outSize == 0) return false;
    out[0] = '\0';

    uintptr_t data = 0;
    int num = 0;
    if (!readPtr(playerState + offset::ps_name, data)) return false;
    if (!read<int>(playerState + offset::ps_name + 0x08, num)) return false;
    if (num <= 1 || num > limits::kMaxNameLen) return false;   /* num incluye el '\0' */

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
