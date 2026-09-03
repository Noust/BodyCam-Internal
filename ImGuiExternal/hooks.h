#pragma once
#include "include.h"

/* Structs heredados del codigo anterior. El nombre del jugador ya NO se lee
 * asi: PlayerNamePrivate es un FString (TArray<wchar_t>) y copiar 22 wchar_t a
 * ciegas desde el puntero podia leer fuera del buffer. Usar ReadPlayerName()
 * de reader.hpp, que respeta el Num del FString. Se conservan por si algun
 * codigo viejo los referencia. */

class Nameptr
{
public:
	wchar_t Name[22]; //0x0000
}; //Size: 0x002C

class Name
{
public:
	char pad_0000[832]; //0x0000
	class Nameptr* ptr1; //0x0340
}; //Size: 0x0348
