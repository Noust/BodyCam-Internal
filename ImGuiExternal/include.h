#pragma once
#include <Windows.h>
#include <unordered_map>
#include <algorithm>
#include <iostream>
#include <cstdio>
#include <TlHelp32.h>
#include <vector>
#include <Psapi.h>
#include <dwmapi.h>
#include <DirectX/d3d9.h>
#include <ImGui/imgui.h>
#include <ImGui/imgui_impl_dx9.h>
#include <ImGui/imgui_impl_win32.h>
#include <string>
#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "dwmapi.lib")

float widthscreen;
float heightscreen;

DWORD64 Uworld = NULL;

#include "HookFunc.h"
#include "hooks.h"
#include "vector.h"
#include "reader.hpp"
#include "WorldToScreen.hpp"
#include "Overlay.hpp"


#define P(Addr,bytes,size) hooks->Patch((BYTE*)Addr,(BYTE*)bytes,size)
#define H(Addr,hook,size) hooks->Hook((BYTE*)Addr,(BYTE*)hook,size)
#define GetAddr(Addr) (DWORD64)GetModuleHandleA("Bodycam-Win64-Shipping.exe") + Addr