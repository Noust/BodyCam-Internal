#pragma once
#include "include.h"

class Name
{
public:
	char pad_0000[904]; //0x0000
	class Nameptr* ptr1; //0x0388
}; //Size: 0x0390

class Nameptr
{
public:
	wchar_t Name[22]; //0x0000
}; //Size: 0x00AC

Name* name;

