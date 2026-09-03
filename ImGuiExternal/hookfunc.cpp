#include "include.h"

/* NOTA: ninguna de las funciones de hook de este archivo se usa actualmente.
 * Se conservan por si mas adelante hace falta un hook (ver §9.2 del .md), pero
 * NO sirven tal cual para hookear una funcion y seguir llamando a la original:
 * escriben encima sin conservar las instrucciones desplazadas. Para eso haria
 * falta un trampolin de verdad. */

bool hookclass::Detour32(void* src, void* dst, int len)
{
	if (!src || !dst || len < 5) return false;

	/* Un `jmp rel32` lleva un desplazamiento de 4 BYTES, no de 8. El codigo
	 * original escribia un uintptr_t entero, machacando 4 bytes de mas ademas
	 * de los `len` reservados. Sobre codigo del juego eso lo corrompe.
	 * Ademas hay que comprobar que el salto cabe en 32 bits con signo: en x64
	 * dos modulos pueden estar a mas de 2 GB de distancia. */
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

/* ---------------------------------------------------------------------------
 *  GWorld ya NO se fija por RVA.
 *
 *  Se intento derivarlo estaticamente y fallo dos veces:
 *    - 0x9934600 : resulto ser `__security_cookie`. El perfil "muchisimas
 *                  lecturas, una sola escritura" que parecia identificar a
 *                  GWorld lo cumple el cookie, porque se carga en el prologo de
 *                  casi todas las funciones del binario.
 *    - 0x9C1DAA0 : es `GEngine`. Se usa con +0xFB0, offset que ni siquiera cabe
 *                  en UWorld (sizeof 0x908).
 *
 *  En vez de seguir adivinando, el mundo se localiza en runtime recorriendo las
 *  secciones de datos del modulo y validando cada candidato contra la cadena
 *  real del juego (ver ScanForGWorld en reader.hpp). Solo el UWorld verdadero
 *  tiene GameInstance -> LocalPlayers -> PlayerController -> CameraManager.
 *
 *  Ventaja anadida: esto sobrevive a los parches del juego sin tocar codigo.
 * ------------------------------------------------------------------------- */

void hookclass::GetAddreses() {
	/* Se deja a cero a proposito: ResolveGWorld() hara la busqueda en el primer
	 * frame y guardara aqui la direccion de la global. */
	Uworld = 0;
}