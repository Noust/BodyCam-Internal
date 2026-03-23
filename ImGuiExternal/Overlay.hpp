#pragma once
#include "include.h"
#include <ImGui/imgui.h>
#include <ImGui/imgui_impl_dx9.h>
#include <ImGui/imgui_impl_win32.h>
#include <ImGui/imgui_internal.h>

void inputHandler();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#define clearVariable(x) if (x) { x->Release(); x = nullptr; delete x;}

std::string generateRandomString(int length) {
    const std::string characters = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::string randomString;
    std::srand(std::time(0));
    for (int i = 0; i < length; ++i) {
        randomString += characters[std::rand() % characters.length()];
    }
    return randomString;
}

int generateRandomInt(int min, int max) {
    std::srand(std::time(0));
    return min + std::rand() % (max - min + 1);
}

DWORD getProcessID(std::string processName) {
    PROCESSENTRY32 processInfo;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
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

typedef struct
{
    DWORD R;
    DWORD G;
    DWORD B;
    DWORD A;
}RGBA;

std::string stringToUTF8(const std::string& str) {
    int nwLen = ::MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, NULL, 0);
    wchar_t* pwBuf = new wchar_t[nwLen + 1];
    ZeroMemory(pwBuf, nwLen * 2 + 2);
    ::MultiByteToWideChar(CP_ACP, 0, str.c_str(), str.length(), pwBuf, nwLen);
    int nLen = ::WideCharToMultiByte(CP_UTF8, 0, pwBuf, -1, NULL, NULL, NULL, NULL);
    char* pBuf = new char[nLen + 1];
    ZeroMemory(pBuf, nLen + 1);
    ::WideCharToMultiByte(CP_UTF8, 0, pwBuf, nwLen, pBuf, nLen, NULL, NULL);
    std::string retStr(pBuf);
    delete[]pwBuf;
    delete[]pBuf;
    pwBuf = NULL;
    pBuf = NULL;
    return retStr;
}

void drawStrokeText(int x, int y, RGBA* color, const char* str) {
    ImFont a;
    std::string utf_8_1 = std::string(str);
    std::string utf_8_2 = stringToUTF8(utf_8_1);
    ImGui::GetForegroundDrawList()->AddText(ImVec2(x, y - 1), ImGui::ColorConvertFloat4ToU32(ImVec4(1 / 255.0, 1 / 255.0, 1 / 255.0, 255 / 255.0)), utf_8_2.c_str());
    ImGui::GetForegroundDrawList()->AddText(ImVec2(x, y + 1), ImGui::ColorConvertFloat4ToU32(ImVec4(1 / 255.0, 1 / 255.0, 1 / 255.0, 255 / 255.0)), utf_8_2.c_str());
    ImGui::GetForegroundDrawList()->AddText(ImVec2(x - 1, y), ImGui::ColorConvertFloat4ToU32(ImVec4(1 / 255.0, 1 / 255.0, 1 / 255.0, 255 / 255.0)), utf_8_2.c_str());
    ImGui::GetForegroundDrawList()->AddText(ImVec2(x + 1, y), ImGui::ColorConvertFloat4ToU32(ImVec4(1 / 255.0, 1 / 255.0, 1 / 255.0, 255 / 255.0)), utf_8_2.c_str());
    ImGui::GetForegroundDrawList()->AddText(ImVec2(x, y), ImGui::ColorConvertFloat4ToU32(ImVec4(color->R / 255.0, color->G / 255.0, color->B / 255.0, color->A / 255.0)), utf_8_2.c_str());
}

void DrawLine(fvector2d pos1, fvector2d pos2, ImColor color, float Thicknes, bool checkpoints) {
    if (checkpoints) {
        if (pos1.x >= 0 && pos1.y >= 0 && pos1.x <= widthscreen && pos1.y <= heightscreen && pos2.x >= 0 && pos2.y >= 0 && pos2.x <= widthscreen && pos2.y <= heightscreen) {
            ImGui::GetBackgroundDrawList()->AddLine(ImVec2(pos1.x, pos1.y), ImVec2(pos2.x, pos2.y), ImColor(0, 0, 0), Thicknes + 2);
            ImGui::GetBackgroundDrawList()->AddLine(ImVec2(pos1.x, pos1.y), ImVec2(pos2.x, pos2.y), color, Thicknes);
        }
    }
    else {
        ImGui::GetBackgroundDrawList()->AddLine(ImVec2(pos1.x, pos1.y), ImVec2(pos2.x, pos2.y), ImColor(0, 0, 0), Thicknes + 2);
        ImGui::GetBackgroundDrawList()->AddLine(ImVec2(pos1.x, pos1.y), ImVec2(pos2.x, pos2.y), color, Thicknes);
    }
}

void drawNewText(int x, int y, RGBA* color, const char* str) {
    ImFont a;
    std::string utf_8_1 = std::string(str);
    std::string utf_8_2 = stringToUTF8(utf_8_1);
    ImGui::GetForegroundDrawList()->AddText(ImVec2(x, y), ImGui::ColorConvertFloat4ToU32(ImVec4(color->R / 255.0, color->G / 255.0, color->B / 255.0, color->A / 255.0)), utf_8_2.c_str());
}

void DrawCircle(fvector2d pos, int radious, int thickness, ImColor color) {
    if (pos.x >= 0 && pos.y >= 0 && pos.x <= widthscreen && pos.y <= heightscreen) {
        ImGui::GetBackgroundDrawList()->AddCircle(ImVec2(pos.x, pos.y), radious, ImColor(0, 0, 0), 0, thickness + 2);
        ImGui::GetBackgroundDrawList()->AddCircle(ImVec2(pos.x, pos.y), radious, color, 0, thickness);
    }
}

void DrawCornerEsp(float W, float H, Vector2 pos, ImColor color, int thickness) {
    float lineW = (W / 5);
    float lineH = (H / 6);

    //outline
    DrawLine({ pos.x - W / 2, pos.y - H }, { pos.x - W / 2 + lineW, pos.y - H }, color, thickness, false);//top left
    DrawLine({ pos.x - W / 2, pos.y - H }, { pos.x - W / 2, pos.y - H + lineH }, color, thickness, false);
    DrawLine({ pos.x - W / 2, pos.y - lineH }, { pos.x - W / 2, pos.y }, color, thickness, false); //bot left
    DrawLine({ pos.x - W / 2, pos.y }, { pos.x - W / 2 + lineW, pos.y }, color, thickness, false);
    DrawLine({ pos.x + W / 2 - lineW, pos.y - H }, { pos.x + W / 2, pos.y - H }, color, thickness, false); // top right
    DrawLine({ pos.x + W / 2, pos.y - H }, { pos.x + W / 2, pos.y - H + lineH }, color, thickness, false);
    DrawLine({ pos.x + W / 2, pos.y - lineH }, { pos.x + W / 2, pos.y }, color, thickness, false); // bot right
    DrawLine({ pos.x + W / 2 - lineW, pos.y }, { pos.x + W / 2, pos.y }, color, thickness, false);
}

void DrawFilledRect(Vector2 pos, float height, float width, ImColor color) {
    ImGui::GetBackgroundDrawList()->AddRectFilled({ pos.x - width,pos.y - height }, { pos.x + width, pos.y }, color);
}

void drawbox(fvector2d pos, float height, float width, ImColor color, float thickness) {
    DrawLine({ pos.x + width, pos.y }, { pos.x - width,pos.y }, color, thickness, false);
    DrawLine({ pos.x + width, pos.y }, { pos.x + width,pos.y - height }, color, thickness, false);
    DrawLine({ pos.x + width,pos.y - height }, { pos.x - width,pos.y - height }, color, thickness, false);
    DrawLine({ pos.x - width,pos.y - height }, { pos.x - width,pos.y }, color, thickness, false);
}

void DrawT(fvector2d pos, const char* text, float divide, ImColor color) {
    ImGui::GetBackgroundDrawList()->AddText(ImVec2(pos.x - ImGui::CalcTextSize(text).x / 2, pos.y - (ImGui::CalcTextSize(text).y / divide)), color, text);
}

void DrawBackgroundAnimation() {
    static std::vector<ImVec2> particles;
    static std::vector<float> particleAngles;
    static std::vector<float> particleSpeeds;
    static bool initialized = false;
    static float spawnTimer = 0.0f;
    const float SPAWN_INTERVAL = 0.5f;

    // Initialize particles if not done yet
    if (!initialized) {
        for (int i = 0; i < 100; i++) {
            particles.push_back(ImVec2(
                static_cast<float>(rand() % static_cast<int>(widthscreen)),
                static_cast<float>(rand() % static_cast<int>(heightscreen))
            ));
            particleAngles.push_back(static_cast<float>(rand()) / RAND_MAX * 2 * 3.14159f);
            particleSpeeds.push_back(0.2f + static_cast<float>(rand()) / RAND_MAX * 0.3f);
        }
        initialized = true;
    }

    // Spawn new particles over time
    spawnTimer += ImGui::GetIO().DeltaTime;
    if (spawnTimer >= SPAWN_INTERVAL && particles.size() < 150) {
        particles.push_back(ImVec2(
            static_cast<float>(rand() % static_cast<int>(widthscreen)),
            static_cast<float>(rand() % static_cast<int>(heightscreen))
        ));
        particleAngles.push_back(static_cast<float>(rand()) / RAND_MAX * 2 * 3.14159f);
        particleSpeeds.push_back(0.2f + static_cast<float>(rand()) / RAND_MAX * 0.3f);
        spawnTimer = 0.0f;
    }

    ImVec2 mousePos = ImGui::GetMousePos();
    ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
    float deltaTime = ImGui::GetIO().DeltaTime;

    // Update and draw each particle
    for (size_t i = 0; i < particles.size(); i++) {
        auto& particle = particles[i];
        auto& angle = particleAngles[i];
        auto& speed = particleSpeeds[i];

        // Floating motion
        particle.x += cos(angle) * speed;
        particle.y += sin(angle) * speed;

        // Slowly rotate angle for smooth wave-like motion
        angle += deltaTime * 0.3f;

        // Mouse interaction
        float dx = particle.x - mousePos.x;
        float dy = particle.y - mousePos.y;
        float dist = sqrt(dx * dx + dy * dy);

        const float MAX_INFLUENCE_DIST = 150.0f;
        const float MIN_INFLUENCE_DIST = 50.0f;

        if (dist < MAX_INFLUENCE_DIST) {
            float movement_factor;
            if (dist < MIN_INFLUENCE_DIST) {
                movement_factor = 1.0f;
            }
            else {
                movement_factor = 1.0f - ((dist - MIN_INFLUENCE_DIST) / (MAX_INFLUENCE_DIST - MIN_INFLUENCE_DIST));
            }

            particle.x += (dx / dist) * movement_factor * 3.0f;
            particle.y += (dy / dist) * movement_factor * 3.0f;
        }

        // Screen wrapping
        if (particle.x < 0) particle.x = widthscreen;
        if (particle.x > widthscreen) particle.x = 0;
        if (particle.y < 0) particle.y = heightscreen;
        if (particle.y > heightscreen) particle.y = 0;

        // Draw particle with pulsating opacity
        float opacity = 0.4f + 0.2f * sin(ImGui::GetTime() * speed * 2.0f);
        draw_list->AddCircleFilled(particle, 2.0f, ImColor(255, 255, 255, static_cast<int>(opacity * 255)));
    }
}

//void DrawBones(DWORD64 sekeltalmesh, bool visible, FMinimalViewInfo camera) {
//    if (!sekeltalmesh) return;
//
//    static ImColor green(0, 255, 0);
//    static ImColor red(255, 0, 0);
//    ImColor color = visible ? green : red;
//
//    try {
//        // Pre-allocate vector capacity
//        std::vector<fvector2d> bones;
//        bones.reserve(20);  // Reservar espacio exacto que necesitamos
//
//        // Cache bone positions in one go
//        fvector head_pos = get_bone_3d(sekeltalmesh, bone::head);
//        bones.push_back(w2s(head_pos));
//
//        static const int bone_ids[] = {
//            bone::neck, bone::chest, bone::stomage, bone::up_penis, bone::penis,
//            bone::left_shoulder, bone::left_elbow, bone::left_hand,
//            bone::right_shoulder, bone::right_elbow, bone::right_hand,
//            bone::left_pelvis, bone::left_knee, bone::left_foot_up, bone::left_foot,
//            bone::right_pelvis, bone::right_knee, bone::right_foot_up, bone::right_foot
//        };
//
//        // Usar array estático en lugar de vector para connections
//        for (int id : bone_ids) {
//            bones.push_back(w2s(get_bone_3d(sekeltalmesh, id)));
//        }
//
//        // Draw head circle - calcular radio una sola vez
//        float radius = std::clamp(static_cast<float>(15.0f / (camera.Location.distance(head_pos) * 0.002f)), 3.0f, 15.0f);
//        DrawCircle(bones[0], radius, 0, color);
//
//        // Connections como array estático
//        static const std::pair<int, int> connections[] = {
//            {0,1}, {1,2}, {2,3}, {3,4}, {4,5},  // Spine
//            {1,6}, {6,7}, {7,8},   // Left arm
//            {1,9}, {9,10}, {10,11}, // Right arm
//            {5,12}, {12,13}, {13,14}, {14,15},  // Left leg
//            {5,16}, {16,17}, {17,18}, {18,19}   // Right leg
//        };
//
//        // Draw all lines
//        for (const auto& [first, second] : connections) {
//            DrawLine(bones[first], bones[second], color, 0, true);
//        }
//    }
//    catch (...) {}
//}