#pragma once
#include "include.h"
#include <ImGui/imgui.h>
#include <ImGui/imgui_impl_dx9.h>
#include <ImGui/imgui_impl_win32.h>
#include <ImGui/imgui_internal.h>
#include <ctime>

void inputHandler();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

/* Antes hacia "x->Release(); delete x;". Release() ya destruye el objeto COM;
 * el delete despues liberaba memoria que no era nuestra. Doble liberacion,
 * crash al cerrar. */
#define clearVariable(x) do { if (x) { (x)->Release(); (x) = nullptr; } } while (0)

/* Todas inline: este header lo incluyen Source.cpp y hookfunc.cpp. */

inline std::string generateRandomString(int length) {
    const std::string characters = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::string randomString;
    for (int i = 0; i < length; ++i)
        randomString += characters[std::rand() % characters.length()];
    return randomString;
}

inline int generateRandomInt(int min, int max) {
    if (max <= min) return min;
    return min + std::rand() % (max - min + 1);
}

inline DWORD getProcessID(std::string processName) {
    PROCESSENTRY32 processInfo{};
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return 0;
    processInfo.dwSize = sizeof(PROCESSENTRY32);
    if (Process32First(hSnapshot, &processInfo)) {
        do {
            if (!lstrcmpi(processInfo.szExeFile, processName.c_str())) {
                CloseHandle(hSnapshot);
                return processInfo.th32ProcessID;
            }
        } while (Process32Next(hSnapshot, &processInfo));
    }
    CloseHandle(hSnapshot);
    return 0;
}

typedef struct { DWORD R, G, B, A; } RGBA;

/* Partículas del fondo del menú. Solo decorativo, y solo con el menú abierto. */
inline void DrawBackgroundAnimation() {
    static std::vector<ImVec2> particles;
    static std::vector<float> particleAngles;
    static std::vector<float> particleSpeeds;
    static bool initialized = false;
    static float spawnTimer = 0.0f;
    const float SPAWN_INTERVAL = 0.5f;

    const int W = (int)widthscreen, H = (int)heightscreen;
    if (W <= 0 || H <= 0) return;

    if (!initialized) {
        for (int i = 0; i < 100; i++) {
            particles.push_back(ImVec2((float)(rand() % W), (float)(rand() % H)));
            particleAngles.push_back((float)rand() / RAND_MAX * 2 * 3.14159f);
            particleSpeeds.push_back(0.2f + (float)rand() / RAND_MAX * 0.3f);
        }
        initialized = true;
    }

    spawnTimer += ImGui::GetIO().DeltaTime;
    if (spawnTimer >= SPAWN_INTERVAL && particles.size() < 150) {
        particles.push_back(ImVec2((float)(rand() % W), (float)(rand() % H)));
        particleAngles.push_back((float)rand() / RAND_MAX * 2 * 3.14159f);
        particleSpeeds.push_back(0.2f + (float)rand() / RAND_MAX * 0.3f);
        spawnTimer = 0.0f;
    }

    ImVec2 mousePos = ImGui::GetMousePos();
    ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
    if (!draw_list) return;
    float deltaTime = ImGui::GetIO().DeltaTime;

    for (size_t i = 0; i < particles.size(); i++) {
        auto& particle = particles[i];
        auto& angle = particleAngles[i];
        auto& speed = particleSpeeds[i];

        particle.x += cosf(angle) * speed;
        particle.y += sinf(angle) * speed;
        angle += deltaTime * 0.3f;

        float dx = particle.x - mousePos.x;
        float dy = particle.y - mousePos.y;
        float dist = sqrtf(dx * dx + dy * dy);

        const float MAX_INFLUENCE_DIST = 150.0f;
        const float MIN_INFLUENCE_DIST = 50.0f;

        if (dist < MAX_INFLUENCE_DIST && dist > 0.001f) {
            float f = (dist < MIN_INFLUENCE_DIST) ? 1.0f
                    : 1.0f - ((dist - MIN_INFLUENCE_DIST) / (MAX_INFLUENCE_DIST - MIN_INFLUENCE_DIST));
            particle.x += (dx / dist) * f * 3.0f;
            particle.y += (dy / dist) * f * 3.0f;
        }

        if (particle.x < 0) particle.x = widthscreen;
        if (particle.x > widthscreen) particle.x = 0;
        if (particle.y < 0) particle.y = heightscreen;
        if (particle.y > heightscreen) particle.y = 0;

        float opacity = 0.4f + 0.2f * sinf((float)ImGui::GetTime() * speed * 2.0f);
        draw_list->AddCircleFilled(particle, 2.0f, ImColor(255, 255, 255, (int)(opacity * 255)));
    }
}
