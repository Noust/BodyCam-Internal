#pragma once
/* windows.h define min/max como macros y rompen std::min / std::max
 * (el sintoma es un "illegal token on right side of ::"). */
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <unordered_map>
#include <algorithm>
#include <iostream>
#include <cstdio>
#include <cstdint>
#include <cstddef>
#include <type_traits>
#include <TlHelp32.h>
#include <vector>
#include <Psapi.h>
#include <dwmapi.h>
#include <DirectX/d3d9.h>
#include <DirectX/d3dx9math.h>
#include <ImGui/imgui.h>
#include <ImGui/imgui_impl_dx9.h>
#include <ImGui/imgui_impl_win32.h>
#include <string>
#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "dwmapi.lib")

/* Globales compartidas por Source.cpp y hookfunc.cpp.
 *
 * `inline` es obligatorio: sin el, cada .cpp define su propio simbolo y el
 * proyecto solo enlaza gracias a /FORCE:MULTIPLE, que resuelve el conflicto
 * quedandose con uno arbitrario. Eso significaba que GetAddreses() (hookfunc.cpp)
 * podia estar escribiendo en un Uworld distinto del que lee ReadValues()
 * (Source.cpp). Con inline hay una sola instancia garantizada. */

 /* Tamano del CLIENTE del juego, no del escritorio. Se refresca cada frame
  * desde ImGui::GetIO().DisplaySize. Usar GetSystemMetrics(SM_CXSCREEN) daba
  * una proyeccion incorrecta con el juego en ventana o a otra resolucion. */
inline float widthscreen = 0.0f;
inline float heightscreen = 0.0f;

/* Direccion de la global GWorld del juego (base del modulo + RVA). */
inline DWORD64 Uworld = 0;

#include "HookFunc.h"        /* offsets                                   */
#include "hooks.h"           /* structs heredados                         */
#include "vector.h"          /* fvector, FTransform, matrices             */
#include "reader.hpp"        /* lectura segura + cadena del mundo         */
#include "game_names.hpp"    /* resolucion de FName (GNames)              */
#include "game_calls.hpp"    /* llamadas verificadas a funciones del juego*/
#include "esp_render.hpp"    /* primitivas de dibujado                    */
#include "WorldToScreen.hpp" /* camara, proyeccion, huesos                */
#include "Overlay.hpp"       /* utilidades de ventana                     */

#define P(Addr,bytes,size) hooks->Patch((BYTE*)Addr,(BYTE*)bytes,size)
#define H(Addr,hook,size) hooks->Hook((BYTE*)Addr,(BYTE*)hook,size)
#define GetAddr(Addr) ((DWORD64)GetModuleHandleA("Bodycam-Win64-Shipping.exe") + (Addr))
