#pragma once
#include "include.h"

class Name
{
public:
	char pad_0000[904]; //0x0000
	class NamePtr* ptr1; //0x0388
}; //Size: 0x0390

class NamePtr
{
public:
	char Names[22]; //0x0000
	char pad_0016[8]; //0x0016
}; //Size: 0x001E


Name* name;

