#pragma once
#include "include.h"

class Name
{
public:
	char pad_0000[832]; //0x0000
	class Nameptr* ptr1; //0x0340
}; //Size: 0x0580

class Nameptr
{
public:
	wchar_t Name[22]; //0x0000
}; //Size: 0x00BC

Name* name;

