#pragma once
#include "include.h"
#include <cstring>
#include <cctype>

/* ---------------------------------------------------------------------------
 *  Resolución de FName (GNames / FNamePool)
 *
 *  Necesario para dos cosas:
 *    - saber cómo se llama cada hueso, y así dibujar solo el esqueleto del
 *      cuerpo en vez de los ~150 huesos del modelo (dedos, twists, IK, armas);
 *    - saber de qué clase es un pawn, para distinguir a un jugador de un dron.
 *
 *  GNames localizado por ASM (RVA 0x99C1840). El código del juego hace:
 *      shr   edx, 10h                 ; block  = idx >> 16
 *      lea   ecx, [rax+rax]           ; offset = (idx & 0xFFFF) * 2
 *      add   rcx, [r8+rdx*8+10h]      ; + Blocks[block]   -> Blocks en +0x10
 *      movzx ebx, word ptr [rcx]      ; header
 *      shr   ebx, 6                   ; len = header >> 6
 *
 *  FNameEntry: header uint16 en +0x00 (bit 0 = ancho/UTF-16, len = header>>6),
 *  y los caracteres a partir de +0x02.
 * ------------------------------------------------------------------------- */

namespace Names {

	inline constexpr DWORD64 kRVA_GNames = 0x99C1840;
	inline constexpr int kBlocksOffset = 0x10;

	inline uintptr_t g_Pool = 0;
	inline bool      g_Ready = false;
	inline char      g_Status[96] = "not initialized";

	inline bool Init() {
		if (g_Ready) return true;
		const DWORD64 base = (DWORD64)GetModuleHandleA("Bodycam-Win64-Shipping.exe");
		if (!base) { strcpy_s(g_Status, "game module not found"); return false; }
		g_Pool = (uintptr_t)base + kRVA_GNames;

		/* Comprobacion de cordura: el primer bloque debe existir. Si no, o el
		 * RVA es erroneo o el pool aun no esta inicializado. */
		uintptr_t block0 = 0;
		if (!readPtr(g_Pool + kBlocksOffset, block0)) {
			strcpy_s(g_Status, "FNamePool not ready (Blocks[0] unreadable)");
			return false;
		}
		g_Ready = true;
		strcpy_s(g_Status, "FNamePool resolved");
		return true;
	}

	/* Convierte un ComparisonIndex de FName en texto ASCII.
	 * Devuelve false sin tocar `out` mas alla del terminador si algo no cuadra. */
	inline bool Resolve(uint32_t comparisonIndex, char* out, size_t outSize) {
		if (!out || outSize == 0) return false;
		out[0] = '\0';
		if (!g_Ready && !Init()) return false;

		const uint32_t block = comparisonIndex >> 16;
		const uint32_t offset = comparisonIndex & 0xFFFF;
		if (block > 8192) return false;                    /* FNameMaxBlocks */

		uintptr_t blockPtr = 0;
		if (!readPtr(g_Pool + kBlocksOffset + (uintptr_t)block * 8, blockPtr)) return false;

		const uintptr_t entry = blockPtr + (uintptr_t)offset * 2;
		uint16_t header = 0;
		if (!read<uint16_t>(entry, header)) return false;

		const bool wide = (header & 1) != 0;
		const int  len = header >> 6;
		if (len <= 0 || len > 250) return false;

		/* Tope explicito al tamano del buffer temporal. `len` ya viene acotado a
		 * 250, pero dejarlo implicito es la clase de detalle que se rompe si
		 * alguien toca ese limite mas adelante. */
		constexpr size_t kTmpChars = 256;
		size_t cap = (size_t)len;
		if (cap > outSize - 1) cap = outSize - 1;
		if (cap > kTmpChars - 1) cap = kTmpChars - 1;
		if (cap == 0) return false;

		if (wide) {
			wchar_t wbuf[kTmpChars] = {};
			if (!readBytes(entry + 2, wbuf, cap * sizeof(wchar_t))) return false;
			const int n = WideCharToMultiByte(CP_UTF8, 0, wbuf, (int)cap,
			                                  out, (int)outSize - 1, nullptr, nullptr);
			if (n <= 0) { out[0] = '\0'; return false; }
			out[n] = '\0';
		}
		else {
			if (!readBytes(entry + 2, out, cap)) { out[0] = '\0'; return false; }
			out[cap] = '\0';
		}
		return out[0] != '\0';
	}

	/* Lee un FName situado en `addr` (ComparisonIndex en +0, Number en +4)
	 * y lo resuelve. El sufijo _N no se añade: no hace falta para comparar. */
	inline bool ReadFName(uintptr_t addr, char* out, size_t outSize) {
		if (!out || outSize == 0) return false;
		out[0] = '\0';
		uint32_t idx = 0;
		if (!read<uint32_t>(addr, idx)) return false;
		return Resolve(idx, out, outSize);
	}

	/* --------------------- offsets de UObject ---------------------------
	 * InternalIndex@0x0C esta VERIFICADO por ASM (ver §2 del .md). El layout
	 * de UObjectBase es contiguo y de tamaños fijos:
	 *     vtable(8) | ObjectFlags(4) | InternalIndex(4) | Class(8) | Name(8) | Outer(8)
	 * asi que con InternalIndex en 0x0C, Class solo puede estar en 0x10 y Name
	 * en 0x18. Aun asi todo va validado: si un puntero no cuadra, no se
	 * clasifica y ya esta, nunca se desreferencia a ciegas.
	 * ------------------------------------------------------------------- */
	inline constexpr int kUObject_Class = 0x10;
	inline constexpr int kUObject_Name = 0x18;

	inline bool GetObjectName(uintptr_t obj, char* out, size_t outSize) {
		if (!IsValidPtr(obj)) { if (out && outSize) out[0] = '\0'; return false; }
		return ReadFName(obj + kUObject_Name, out, outSize);
	}

	/* Nombre de la clase de un objeto: obj->Class->Name. */
	inline bool GetClassName(uintptr_t obj, char* out, size_t outSize) {
		if (out && outSize) out[0] = '\0';
		uintptr_t cls = 0;
		if (!readPtr(obj + kUObject_Class, cls)) return false;
		return ReadFName(cls + kUObject_Name, out, outSize);
	}

	/* Compara sin distinguir mayusculas; `needle` en minusculas. */
	inline bool ContainsCI(const char* hay, const char* needle) {
		if (!hay || !needle || !*needle) return false;
		for (const char* h = hay; *h; ++h) {
			const char* a = h; const char* b = needle;
			while (*a && *b && (char)tolower((unsigned char)*a) == *b) { ++a; ++b; }
			if (!*b) return true;
		}
		return false;
	}

} // namespace Names
