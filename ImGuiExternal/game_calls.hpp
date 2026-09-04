#pragma once
#include "include.h"
#include <cmath>
#include <cstring>

namespace GameCalls {

	inline constexpr DWORD64 kRVA_AddPitchInput = 0x3CB83C0;
	inline constexpr DWORD64 kRVA_AddRollInput  = 0x3CB8450;
	inline constexpr DWORD64 kRVA_AddYawInput   = 0x3CB85D0;

	inline constexpr size_t kFnSize = 0x8C;
	inline constexpr size_t kTailOffset = 0x73;
	inline constexpr size_t kTailLen = 25;

	using AddInputFn = void(__fastcall*)(void* playerController, float value);

	struct Resolved {
		AddInputFn addPitch = nullptr;
		AddInputFn addYaw = nullptr;
		bool  attempted = false;
		bool  ok = false;
		bool  foundByScan = false;
		DWORD scanMs = 0;
		DWORD64 rvaPitch = 0;
		DWORD64 rvaYaw = 0;
		char  status[192] = "not resolved yet";
	};
	inline Resolved g_Calls;

	inline double g_YawScale = 1.0;
	inline double g_PitchScale = 1.0;
	inline bool   g_YawCalibrated = false;
	inline bool   g_PitchCalibrated = false;

	inline bool IsExecutable(uintptr_t addr, size_t size) {
		MEMORY_BASIC_INFORMATION mbi{};
		if (!VirtualQuery(reinterpret_cast<LPCVOID>(addr), &mbi, sizeof(mbi))) return false;
		if (mbi.State != MEM_COMMIT) return false;
		if (addr + size > reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize) return false;
		const DWORD exec = PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
		return (mbi.Protect & exec) != 0;
	}

	inline bool VerifySignature(uintptr_t fn, uint32_t wantOffset) {
		if (!IsValidPtr(fn) || !IsExecutable(fn, kFnSize)) return false;

		uint8_t b[kFnSize] = {};
		if (!readBytes(fn, b, sizeof(b))) return false;

		static const uint8_t kProlog[12] = {
			0x40, 0x53, 0x48, 0x83, 0xEC, 0x30, 0x48, 0x8B, 0x01, 0x48, 0x8B, 0xD9
		};
		if (memcmp(b, kProlog, sizeof(kProlog)) != 0) return false;

		const uint8_t* t = b + kTailOffset;
		if (!(t[0] == 0x0F && t[1] == 0x5A && t[2] == 0xC0)) return false;
		if (!(t[3] == 0xF2 && t[4] == 0x0F && t[5] == 0x58 && t[6] == 0x83)) return false;
		uint32_t off1 = 0; memcpy(&off1, t + 7, 4);
		if (off1 != wantOffset) return false;
		if (!(t[11] == 0xF2 && t[12] == 0x0F && t[13] == 0x11 && t[14] == 0x83)) return false;
		uint32_t off2 = 0; memcpy(&off2, t + 15, 4);
		if (off2 != wantOffset) return false;
		if (!(t[19] == 0x48 && t[20] == 0x83 && t[21] == 0xC4 && t[22] == 0x30 &&
			  t[23] == 0x5B && t[24] == 0xC3)) return false;
		return true;
	}

	struct FnScanCtx {
		uint32_t  wantPitch;
		uint32_t  wantYaw;
		uintptr_t pitch;
		uintptr_t yaw;
	};

	inline bool FnScanChunk(const uint8_t* b, size_t len, uintptr_t va, void* ctx) {
		FnScanCtx* c = static_cast<FnScanCtx*>(ctx);
		if (len < kTailLen) return false;
		const size_t last = len - kTailLen;
		for (size_t i = 0; i <= last; ++i) {
			if (b[i] != 0x0F || b[i + 1] != 0x5A || b[i + 2] != 0xC0) continue;
			if (b[i + 3] != 0xF2 || b[i + 4] != 0x0F || b[i + 5] != 0x58 || b[i + 6] != 0x83) continue;
			if (b[i + 11] != 0xF2 || b[i + 12] != 0x0F || b[i + 13] != 0x11 || b[i + 14] != 0x83) continue;
			if (b[i + 19] != 0x48 || b[i + 20] != 0x83 || b[i + 21] != 0xC4 ||
			    b[i + 22] != 0x30 || b[i + 23] != 0x5B || b[i + 24] != 0xC3) continue;

			uint32_t o1 = 0, o2 = 0;
			memcpy(&o1, b + i + 7, sizeof(o1));
			memcpy(&o2, b + i + 15, sizeof(o2));
			if (o1 != o2) continue;

			const uintptr_t tail = va + i;
			if (tail < kTailOffset) continue;
			const uintptr_t fn = tail - kTailOffset;

			if (o1 == c->wantPitch && !c->pitch && VerifySignature(fn, o1)) c->pitch = fn;
			else if (o1 == c->wantYaw && !c->yaw && VerifySignature(fn, o1)) c->yaw = fn;

			if (c->pitch && c->yaw) return true;
		}
		return false;
	}

	inline void Resolve() {
		if (g_Calls.attempted) return;
		g_Calls.attempted = true;

		const uintptr_t base = GameModuleBase();
		if (!base) {
			strcpy_s(g_Calls.status, "game module not found (not injected?)");
			return;
		}

		uintptr_t pitch = base + static_cast<uintptr_t>(kRVA_AddPitchInput);
		uintptr_t yaw = base + static_cast<uintptr_t>(kRVA_AddYawInput);

		bool okPitch = VerifySignature(pitch, offset::rotation_input_pitch);
		bool okYaw = VerifySignature(yaw, offset::rotation_input_yaw);

		if (!okPitch || !okYaw) {
			FnScanCtx c{ offset::rotation_input_pitch, offset::rotation_input_yaw, 0, 0 };
			const DWORD t0 = GetTickCount();
			ScanCodeChunks(&FnScanChunk, &c);
			g_Calls.scanMs = GetTickCount() - t0;

			if (c.pitch && c.yaw) {
				pitch = c.pitch; yaw = c.yaw;
				g_Calls.foundByScan = true;
			}
			else {
				sprintf_s(g_Calls.status,
				          "AddPitch/AddYawInput not found (hint %d/%d, scan %d/%d, %u ms) - using direct write",
				          (int)okPitch, (int)okYaw, (int)(c.pitch != 0), (int)(c.yaw != 0), g_Calls.scanMs);
				return;
			}
		}

		g_Calls.addPitch = reinterpret_cast<AddInputFn>(pitch);
		g_Calls.addYaw = reinterpret_cast<AddInputFn>(yaw);
		g_Calls.rvaPitch = static_cast<DWORD64>(pitch - base);
		g_Calls.rvaYaw = static_cast<DWORD64>(yaw - base);
		g_Calls.ok = true;
		sprintf_s(g_Calls.status, "AddPitchInput 0x%llX / AddYawInput 0x%llX verified (%s)",
		          (unsigned long long)g_Calls.rvaPitch, (unsigned long long)g_Calls.rvaYaw,
		          g_Calls.foundByScan ? "by signature scan" : "known RVA");
	}

	__declspec(noinline) inline bool SafeInvoke(AddInputFn fn, void* pc, float v) {
		__try {
			fn(pc, v);
			return true;
		}
		__except (EXCEPTION_EXECUTE_HANDLER) {
			return false;
		}
	}

	inline void CalibrateFrom(double before, double after, float sent,
	                          double& scale, bool& calibrated) {
		if (fabsf(sent) < 1e-5f) return;
		const double applied = after - before;
		if (fabs(applied) < 1e-9) return;

		const double measured = applied / (double)sent;
		if (!(measured == measured)) return;
		if (fabs(measured) < 0.01 || fabs(measured) > 100.0) return;

		if (!calibrated) { scale = measured; calibrated = true; return; }

		if ((measured < 0.0) != (scale < 0.0)) return;
		const double ratio = fabs(measured / scale);
		if (ratio < 0.34 || ratio > 3.0) return;

		scale = scale * 0.75 + measured * 0.25;
	}

	inline void ProbeScale(uintptr_t pc, bool yaw) {
		if (!g_Calls.ok || !IsValidPtr(pc)) return;
		AddInputFn fn = yaw ? g_Calls.addYaw : g_Calls.addPitch;
		if (!fn) return;
		const int off = yaw ? offset::rotation_input_yaw : offset::rotation_input_pitch;
		double& scale = yaw ? g_YawScale : g_PitchScale;
		bool& done = yaw ? g_YawCalibrated : g_PitchCalibrated;

		const float probe = 0.05f;
		double before = 0.0, after = 0.0;
		if (!read<double>(pc + off, before)) return;
		if (!SafeInvoke(fn, reinterpret_cast<void*>(pc), probe)) return;
		if (!read<double>(pc + off, after)) return;
		CalibrateFrom(before, after, probe, scale, done);
	}

	inline bool AddLookInput(uintptr_t pc, double degYaw, double degPitch, double maxStepDeg) {
		if (!g_Calls.ok || !IsValidPtr(pc)) return false;

		if (!g_YawCalibrated && fabs(degYaw) > 1e-4) ProbeScale(pc, true);
		if (!g_PitchCalibrated && fabs(degPitch) > 1e-4) ProbeScale(pc, false);

		if (maxStepDeg < 0.1) maxStepDeg = 0.1;
		if (degYaw > maxStepDeg) degYaw = maxStepDeg;
		if (degYaw < -maxStepDeg) degYaw = -maxStepDeg;
		if (degPitch > maxStepDeg) degPitch = maxStepDeg;
		if (degPitch < -maxStepDeg) degPitch = -maxStepDeg;

		void* pcPtr = reinterpret_cast<void*>(pc);
		bool any = false;

		auto clampInput = [](double x) -> float {
			if (x > 4000.0) x = 4000.0;
			if (x < -4000.0) x = -4000.0;
			return (float)x;
		};

		if (fabs(degYaw) > 1e-4) {
			const double sc = (fabs(g_YawScale) > 1e-6) ? g_YawScale : 1.0;
			const float  v = clampInput(degYaw / sc);
			double before = 0.0, after = 0.0;
			const bool okBefore = read<double>(pc + offset::rotation_input_yaw, before);
			if (!SafeInvoke(g_Calls.addYaw, pcPtr, v)) return false;
			const bool okAfter = read<double>(pc + offset::rotation_input_yaw, after);
			if (okBefore && okAfter)
				CalibrateFrom(before, after, v, g_YawScale, g_YawCalibrated);
			any = true;
		}

		if (fabs(degPitch) > 1e-4) {
			const double sc = (fabs(g_PitchScale) > 1e-6) ? g_PitchScale : 1.0;
			const float  v = clampInput(degPitch / sc);
			double before = 0.0, after = 0.0;
			const bool okBefore = read<double>(pc + offset::rotation_input_pitch, before);
			if (!SafeInvoke(g_Calls.addPitch, pcPtr, v)) return false;
			const bool okAfter = read<double>(pc + offset::rotation_input_pitch, after);
			if (okBefore && okAfter)
				CalibrateFrom(before, after, v, g_PitchScale, g_PitchCalibrated);
			any = true;
		}

		return any;
	}

	inline bool AddLookInputDirect(uintptr_t pc, double degYaw, double degPitch, double maxStepDeg) {
		if (!IsValidPtr(pc)) return false;
		if (maxStepDeg < 0.1) maxStepDeg = 0.1;
		if (degYaw > maxStepDeg) degYaw = maxStepDeg;
		if (degYaw < -maxStepDeg) degYaw = -maxStepDeg;
		if (degPitch > maxStepDeg) degPitch = maxStepDeg;
		if (degPitch < -maxStepDeg) degPitch = -maxStepDeg;

		bool ok = false;
		if (fabs(degYaw) > 1e-4) {
			double cur = 0.0;
			if (read<double>(pc + offset::rotation_input_yaw, cur))
				ok |= write<double>(pc + offset::rotation_input_yaw, cur + degYaw);
		}
		if (fabs(degPitch) > 1e-4) {
			double cur = 0.0;
			if (read<double>(pc + offset::rotation_input_pitch, cur))
				ok |= write<double>(pc + offset::rotation_input_pitch, cur + degPitch);
		}
		return ok;
	}

}
