#pragma once
#include "include.h"
#include <ImGui/imgui.h>
#include <ImGui/imgui_impl_dx9.h>
#include <ImGui/imgui_impl_win32.h>

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

void DrawBones(DWORD64 sekeltalmesh, bool visible) {
    if (!sekeltalmesh) {
        throw std::runtime_error("Mesh is not valid.");
    }

    ImColor color = visible == true ? ImColor(0, 255, 0) : ImColor(255, 0, 0);

    try {
        fvector2d head = w2s(get_bone_3d(sekeltalmesh, bone::head));
        fvector2d neck = w2s(get_bone_3d(sekeltalmesh, bone::neck));
        fvector2d chest = w2s(get_bone_3d(sekeltalmesh, bone::chest));
        fvector2d stomach = w2s(get_bone_3d(sekeltalmesh, bone::stomage));
        fvector2d up_penis = w2s(get_bone_3d(sekeltalmesh, bone::up_penis));
        fvector2d penis = w2s(get_bone_3d(sekeltalmesh, bone::penis));

        fvector2d left_shoulder = w2s(get_bone_3d(sekeltalmesh, bone::left_shoulder));
        fvector2d left_elbow = w2s(get_bone_3d(sekeltalmesh, bone::left_elbow));
        fvector2d left_hand = w2s(get_bone_3d(sekeltalmesh, bone::left_hand));

        fvector2d right_shoulder = w2s(get_bone_3d(sekeltalmesh, bone::right_shoulder));
        fvector2d right_elbow = w2s(get_bone_3d(sekeltalmesh, bone::right_elbow));
        fvector2d right_hand = w2s(get_bone_3d(sekeltalmesh, bone::right_hand));

        fvector2d left_pelvis = w2s(get_bone_3d(sekeltalmesh, bone::left_pelvis));
        fvector2d left_knee = w2s(get_bone_3d(sekeltalmesh, bone::left_knee));
        fvector2d left_foot_up = w2s(get_bone_3d(sekeltalmesh, bone::left_foot_up));
        fvector2d left_foot = w2s(get_bone_3d(sekeltalmesh, bone::left_foot));

        fvector2d right_pelvis = w2s(get_bone_3d(sekeltalmesh, bone::right_pelvis));
        fvector2d right_knee = w2s(get_bone_3d(sekeltalmesh, bone::right_knee));
        fvector2d right_foot_up = w2s(get_bone_3d(sekeltalmesh, bone::right_foot_up));
        fvector2d right_foot = w2s(get_bone_3d(sekeltalmesh, bone::right_foot));


        float radio = (neck.y - head.y) * 1.8f;
        DrawCircle(head, radio, 0, color);
        DrawLine(head, neck, color, 0, true);
        DrawLine(neck, chest, color, 0, true);
        DrawLine(chest, stomach, color, 0, true);
        DrawLine(stomach, up_penis, color, 0, true);
        DrawLine(up_penis, penis, color, 0, true);

        DrawLine(neck, left_shoulder, color, 0, true);
        DrawLine(left_shoulder, left_elbow, color, 0, true);
        DrawLine(left_elbow, left_hand, color, 0, true);

        DrawLine(neck, right_shoulder, color, 0, true);
        DrawLine(right_shoulder, right_elbow, color, 0, true);
        DrawLine(right_elbow, right_hand, color, 0, true);

        DrawLine(penis, left_pelvis, color, 0, true);
        DrawLine(left_pelvis, left_knee, color, 0, true);
        DrawLine(left_knee, left_foot_up, color, 0, true);
        DrawLine(left_foot_up, left_foot, color, 0, true);

        DrawLine(penis, right_pelvis, color, 0, true);
        DrawLine(right_pelvis, right_knee, color, 0, true);
        DrawLine(right_knee, right_foot_up, color, 0, true);
        DrawLine(right_foot_up, right_foot, color, 0, true);
    }
    catch (const std::exception& e) {
        std::cerr << "Error al dibujar los huesos: " << e.what() << std::endl;
    }
}