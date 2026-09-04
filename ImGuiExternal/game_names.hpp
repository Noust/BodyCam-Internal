#pragma once
#include "include.h"
#include <cstring>
#include <cctype>

namespace Names {

	inline constexpr DWORD64 kRVA_GNames_Hint = 0x99C29C0;
	inline constexpr int kBlocksOffset = 0x10;
	inline constexpr int kMaxBlockIndex = 8192;
	inline constexpr int kMaxNameChars = 250;

	inline uintptr_t g_Pool = 0;
	inline bool      g_Ready = false;
	inline bool      g_FoundByScan = false;
	inline DWORD     g_ScanMs = 0;
	inline int       g_InitTries = 0;
	inline DWORD     g_LastInitTry = 0;
	inline char      g_Status[128] = "not initialized";

	inline constexpr int   kMaxInitTries = 3;
	inline constexpr DWORD kInitRetryMs = 2000;

	inline bool ResolveIn(uintptr_t pool, uint32_t comparisonIndex, char* out, size_t outSize) {
		if (!out || outSize == 0) return false;
		out[0] = '\0';
		if (!IsValidPtr(pool)) return false;

		const uint32_t block = comparisonIndex >> 16;
		const uint32_t entryOff = comparisonIndex & 0xFFFF;
		if (block > static_cast<uint32_t>(kMaxBlockIndex)) return false;

		uintptr_t blockPtr = 0;
		if (!readPtr(pool + kBlocksOffset + static_cast<uintptr_t>(block) * 8, blockPtr)) return false;

		const uintptr_t entry = blockPtr + static_cast<uintptr_t>(entryOff) * 2;
		uint16_t header = 0;
		if (!read<uint16_t>(entry, header)) return false;

		const bool wide = (header & 1) != 0;
		const int  len = header >> 6;
		if (len <= 0 || len > kMaxNameChars) return false;

		constexpr size_t kTmpChars = 256;
		size_t cap = static_cast<size_t>(len);
		if (cap > outSize - 1) cap = outSize - 1;
		if (cap > kTmpChars - 1) cap = kTmpChars - 1;
		if (cap == 0) return false;

		if (wide) {
			wchar_t wbuf[kTmpChars] = {};
			if (!readBytes(entry + 2, wbuf, cap * sizeof(wchar_t))) return false;
			const int n = WideCharToMultiByte(CP_UTF8, 0, wbuf, static_cast<int>(cap),
			                                  out, static_cast<int>(outSize) - 1, nullptr, nullptr);
			if (n <= 0) { out[0] = '\0'; return false; }
			out[n] = '\0';
		}
		else {
			if (!readBytes(entry + 2, out, cap)) { out[0] = '\0'; return false; }
			out[cap] = '\0';
		}
		return out[0] != '\0';
	}

	inline bool PoolPlausible(uintptr_t pool) {
		if (!IsValidPtr(pool) || (pool & 7)) return false;
		if (!AddressInModuleData(pool)) return false;
		uintptr_t block0 = 0;
		if (!readPtr(pool + kBlocksOffset, block0)) return false;
		char tmp[64];
		return ResolveIn(pool, 0, tmp, sizeof(tmp));
	}

	inline bool PoolCertain(uintptr_t pool) {
		if (!PoolPlausible(pool)) return false;
		char tmp[64];
		if (!ResolveIn(pool, 0, tmp, sizeof(tmp))) return false;
		return strcmp(tmp, "None") == 0;
	}

	struct PoolScanCtx { uintptr_t pool; };

	inline bool PoolScanChunk(const uint8_t* b, size_t len, uintptr_t va, void* ctx) {
		PoolScanCtx* c = static_cast<PoolScanCtx*>(ctx);
		if (len < 33) return false;
		const size_t last = len - 33;
		for (size_t i = 0; i <= last; ++i) {
			if (b[i] != 0x74 || b[i + 1] != 0x09) continue;
			if (b[i + 2] != 0x4C || b[i + 3] != 0x8D || b[i + 4] != 0x05) continue;
			if (b[i + 9] != 0xEB || b[i + 10] != 0x16) continue;
			if (b[i + 11] != 0x48 || b[i + 12] != 0x8D || b[i + 13] != 0x0D) continue;
			if (b[i + 18] != 0xE8) continue;
			if (b[i + 23] != 0x4C || b[i + 24] != 0x8B || b[i + 25] != 0xC0) continue;
			if (b[i + 26] != 0xC6 || b[i + 27] != 0x05) continue;
			if (b[i + 32] != 0x01) continue;

			int32_t rel1 = 0, rel2 = 0;
			memcpy(&rel1, b + i + 5, sizeof(rel1));
			memcpy(&rel2, b + i + 14, sizeof(rel2));
			const uintptr_t t1 = va + i + 9 + static_cast<intptr_t>(rel1);
			const uintptr_t t2 = va + i + 18 + static_cast<intptr_t>(rel2);
			if (t1 != t2) continue;
			if (!PoolPlausible(t1)) continue;

			c->pool = t1;
			return true;
		}
		return false;
	}

	inline uintptr_t ScanForPool() {
		PoolScanCtx c{ 0 };
		const DWORD t0 = GetTickCount();
		ScanCodeChunks(&PoolScanChunk, &c);
		g_ScanMs = GetTickCount() - t0;
		return c.pool;
	}

	inline bool Init() {
		if (g_Ready) return true;
		if (g_InitTries >= kMaxInitTries) return false;

		const DWORD now = GetTickCount();
		if (g_LastInitTry != 0 && now - g_LastInitTry < kInitRetryMs) return false;
		g_LastInitTry = now;
		++g_InitTries;

		const uintptr_t base = GameModuleBase();
		if (!base) { strcpy_s(g_Status, "game module not found"); return false; }

		const uintptr_t hint = base + static_cast<uintptr_t>(kRVA_GNames_Hint);
		if (PoolCertain(hint)) {
			g_Pool = hint;
			g_Ready = true;
			g_FoundByScan = false;
			sprintf_s(g_Status, "FNamePool at known RVA 0x%llX (verified)",
			          static_cast<unsigned long long>(kRVA_GNames_Hint));
			return true;
		}

		const uintptr_t found = ScanForPool();
		if (found) {
			g_Pool = found;
			g_Ready = true;
			g_FoundByScan = true;
			sprintf_s(g_Status, "FNamePool found by signature, RVA 0x%llX (%u ms)",
			          static_cast<unsigned long long>(found - base), g_ScanMs);
			return true;
		}

		if (PoolPlausible(hint)) {
			g_Pool = hint;
			g_Ready = true;
			g_FoundByScan = false;
			sprintf_s(g_Status, "FNamePool at known RVA 0x%llX (unconfirmed)",
			          static_cast<unsigned long long>(kRVA_GNames_Hint));
			return true;
		}

		sprintf_s(g_Status, "FNamePool not found - game patched? (scan %u ms)", g_ScanMs);
		return false;
	}

	inline bool Resolve(uint32_t comparisonIndex, char* out, size_t outSize) {
		if (!out || outSize == 0) return false;
		out[0] = '\0';
		if (!g_Ready && !Init()) return false;
		return ResolveIn(g_Pool, comparisonIndex, out, outSize);
	}

	inline bool ReadFName(uintptr_t addr, char* out, size_t outSize) {
		if (!out || outSize == 0) return false;
		out[0] = '\0';
		uint32_t idx = 0;
		if (!read<uint32_t>(addr, idx)) return false;
		return Resolve(idx, out, outSize);
	}

	inline constexpr int kUObject_Class = offset::uobject_class;
	inline constexpr int kUObject_Name = offset::uobject_name;

	inline bool GetObjectName(uintptr_t obj, char* out, size_t outSize) {
		if (!IsValidPtr(obj)) { if (out && outSize) out[0] = '\0'; return false; }
		return ReadFName(obj + kUObject_Name, out, outSize);
	}

	inline bool GetClassName(uintptr_t obj, char* out, size_t outSize) {
		if (out && outSize) out[0] = '\0';
		uintptr_t cls = 0;
		if (!readPtr(obj + kUObject_Class, cls)) return false;
		return ReadFName(cls + kUObject_Name, out, outSize);
	}

	inline bool ContainsCI(const char* hay, const char* needle) {
		if (!hay || !needle || !*needle) return false;
		for (const char* h = hay; *h; ++h) {
			const char* a = h; const char* b = needle;
			while (*a && *b && static_cast<char>(tolower(static_cast<unsigned char>(*a))) == *b) { ++a; ++b; }
			if (!*b) return true;
		}
		return false;
	}

}
