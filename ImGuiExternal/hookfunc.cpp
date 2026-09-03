#include "include.h"

bool hookclass::Detour32(void* src, void* dst, int len)
{
	if (!src || !dst || len < 5) return false;

	const ptrdiff_t delta = (ptrdiff_t)((uintptr_t)dst - (uintptr_t)src) - 5;
	if (delta > INT32_MAX || delta < INT32_MIN) return false;
	const int32_t rel = (int32_t)delta;

	DWORD curProtection = 0;
	if (!VirtualProtect(src, len, PAGE_EXECUTE_READWRITE, &curProtection)) return false;

	memset(src, 0x90, len);
	*(BYTE*)src = 0xE9;
	memcpy((BYTE*)src + 1, &rel, sizeof(rel));

	DWORD temp = 0;
	VirtualProtect(src, len, curProtection, &temp);
	FlushInstructionCache(GetCurrentProcess(), src, len);
	return true;
}
void hookclass::Patch(BYTE* addr, BYTE* bytes, unsigned int size) {
	DWORD oProc;
	VirtualProtect(addr, size, PAGE_EXECUTE_READWRITE, &oProc);
	memcpy(addr, bytes, size);
	VirtualProtect(addr, size, oProc, &oProc);;
}

bool hookclass::Hook(BYTE* pTarget, BYTE* pHook, UINT Length)
{
	if (!pTarget || !pHook || Length < 14)

		return false;

	DWORD dwOld = 0;
	if (!VirtualProtect(pTarget, Length, PAGE_EXECUTE_READWRITE, &dwOld))
		return false;
	memset(pTarget, 0x90, Length);

#ifdef _WIN64
	memset(pTarget, 0x00, 14);
	*(pTarget + 0x00) = 0xFF;
	*(pTarget + 0x01) = 0x25;
	*reinterpret_cast<BYTE**>(pTarget + 0x06) = pHook;
#else
	* pTarget = 0xE9;
	*reinterpret_cast<DWORD*>(pTarget + 0x01) = pHook - pTarget - 5;
#endif
	VirtualProtect(pTarget, Length, dwOld, &dwOld);
	return false;
}

MODULEINFO hookclass::GetModuleInfo(char* szModule)
{
	MODULEINFO modinfo = { 0 };
	HMODULE hModule = GetModuleHandle(szModule);
	if (hModule == 0)
		return modinfo;
	GetModuleInformation(GetCurrentProcess(), hModule, &modinfo, sizeof(MODULEINFO));
	return modinfo;
}

DWORD64 hookclass::FindPattern(char* module, char* pattern, char* mask)
{
	MODULEINFO mInfo = GetModuleInfo(module);

	DWORD64 base = (DWORD64)mInfo.lpBaseOfDll;
	DWORD64 size = (DWORD64)mInfo.SizeOfImage;

	DWORD64 patternLength = (DWORD64)strlen(mask);

	for (DWORD64 i = 0; i < size - patternLength; i++)
	{
		bool found = true;
		for (DWORD64 j = 0; j < patternLength; j++)
		{
			found &= mask[j] == '?' || pattern[j] == *(char*)(base + i + j);
		}
		if (found)
		{
			return base + i;
		}
	}

	return NULL;
}

void hookclass::GetAddreses() {
	Uworld = 0;
}
