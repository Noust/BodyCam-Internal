#pragma once
#include "include.h"

class Nameptr
{
public:
	wchar_t Name[22];
};

class Name
{
public:
	char pad_0000[832];
	class Nameptr* ptr1;
};
