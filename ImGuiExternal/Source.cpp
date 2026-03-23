#include "include.h"
bool esp = false;
bool team = false;
bool showDebug = false;

bool isInitialized = false;
bool isMenuVisible = true;
char cdistance[25];
char chealth[25];
float distancelimit = 50;
int numPlayers;

std::unordered_map<DWORD64, bool> playerHealthStates;

std::string WideToString(const wchar_t* wide) {
	if (!wide) return "";
	int size = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
	std::string str(size - 1, 0);
	WideCharToMultiByte(CP_UTF8, 0, wide, -1, &str[0], size, nullptr, nullptr);
	return str;
}

void Colors() {
	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowPadding = ImVec2(11, 12);
	style.FrameRounding = 4.0f;
	style.ItemSpacing = { 14.0f,4.0f };
	style.ScrollbarSize = 11.0f;
	style.WindowBorderSize = 0.0f;
	style.WindowRounding = 12.0f;
	style.ChildRounding = 12.0f;
	style.ScrollbarRounding = 12.0f;
	style.WindowTitleAlign = { 0.50f,0.50f };

	style.Colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
	style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
	style.Colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
	style.Colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	style.Colors[ImGuiCol_PopupBg] = ImVec4(0.19f, 0.19f, 0.19f, 0.92f);
	style.Colors[ImGuiCol_Border] = ImVec4(0.19f, 0.19f, 0.19f, 0.29f);
	style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.24f);
	style.Colors[ImGuiCol_FrameBg] = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
	style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.19f, 0.19f, 0.19f, 0.54f);
	style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
	style.Colors[ImGuiCol_TitleBg] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
	style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
	style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
	style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
	style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
	style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.34f, 0.34f, 0.34f, 0.54f);
	style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.40f, 0.40f, 0.54f);
	style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.56f, 0.56f, 0.56f, 0.54f);
	style.Colors[ImGuiCol_CheckMark] = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
	style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.34f, 0.34f, 0.34f, 0.54f);
	style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.56f, 0.56f, 0.56f, 0.54f);
	style.Colors[ImGuiCol_Button] = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
	style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.19f, 0.19f, 0.19f, 0.54f);
	style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
	style.Colors[ImGuiCol_Header] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
	style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.00f, 0.00f, 0.00f, 0.36f);
	style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.20f, 0.22f, 0.23f, 0.33f);
	style.Colors[ImGuiCol_Separator] = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
	style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.44f, 0.44f, 0.44f, 0.29f);
	style.Colors[ImGuiCol_SeparatorActive] = ImVec4(0.40f, 0.44f, 0.47f, 1.00f);
	style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
	style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.44f, 0.44f, 0.44f, 0.29f);
	style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.40f, 0.44f, 0.47f, 1.00f);
	style.Colors[ImGuiCol_Tab] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
	style.Colors[ImGuiCol_TabHovered] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
	style.Colors[ImGuiCol_TabActive] = ImVec4(0.20f, 0.20f, 0.20f, 0.36f);
	style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
	style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
	style.Colors[ImGuiCol_PlotLines] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
	style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
	style.Colors[ImGuiCol_PlotHistogram] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
	style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
	style.Colors[ImGuiCol_TableHeaderBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
	style.Colors[ImGuiCol_TableBorderStrong] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
	style.Colors[ImGuiCol_TableBorderLight] = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
	style.Colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	style.Colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
	style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
	style.Colors[ImGuiCol_DragDropTarget] = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
	style.Colors[ImGuiCol_NavHighlight] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
	style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 0.00f, 0.00f, 0.70f);
	style.Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(1.00f, 0.00f, 0.00f, 0.20f);
	style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(1.00f, 0.00f, 0.00f, 0.35f);

}

struct WindowInfo {
	int Width;
	int Height;
	int Left;
	int Right;
	int Top;
	int Bottom;
};

WindowInfo* windowInfo;
WNDCLASSEX windowClass;
HWND targetWindow;
HWND overlayWindow;

std::string targetProcessName = "Bodycam-Win64-Shipping.exe";
std::string ovarlayName = generateRandomString(generateRandomInt(30, 100));

IDirect3DDevice9Ex* pDevice = nullptr;
IDirect3D9Ex* pDirect = nullptr;
D3DPRESENT_PARAMETERS gD3DPresentParams = { NULL };

void drawItem() {
	char fpsInfo[64];
	RGBA textColor = { 255,255,255,255 };
	snprintf(fpsInfo, sizeof(fpsInfo), "Overlay FPS: %.0f", ImGui::GetIO().Framerate);
	drawStrokeText(30, 44, &textColor, fpsInfo);
}

void renderImGui() {
	if (!isInitialized) {
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();

		ImGui_ImplWin32_Init(overlayWindow);
		ImGui_ImplDX9_Init(pDevice);
		ImGui_ImplDX9_CreateDeviceObjects();
		isInitialized = true;
	}

	if (GetAsyncKeyState(VK_INSERT) & 1) {
		isMenuVisible = !isMenuVisible;
		ImGui::GetIO().MouseDrawCursor = isMenuVisible;
	}

	Colors();
	ImGui_ImplDX9_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	SetWindowLong(overlayWindow, GWL_EXSTYLE, isMenuVisible ? WS_EX_TOOLWINDOW : (WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW));
	UpdateWindow(overlayWindow);
	if (isMenuVisible) {
		DrawBackgroundAnimation();
		inputHandler();
		drawItem();

		ImGui::Begin("NOVA", 0, ImGuiWindowFlags_NoCollapse);

		ImVec2 windowSize = ImGui::GetContentRegionAvail();

		if (ImGui::BeginTabBar("MainTabs")) {
			if (ImGui::BeginTabItem("Visuals")) {
				ImGui::BeginChild("VisualsChild", ImVec2(windowSize.x - 16, windowSize.y - 40), true);

				ImGui::Text("ESP Options");
				ImGui::Separator();

				ImGui::Checkbox("Enable ESP", &esp);
				if (esp) {
					ImGui::Indent(20);
					ImGui::Checkbox("Ignore Team Members", &team);
					ImGui::SliderFloat("Max Distance", &distancelimit, 10, 100, "%.0f m");
					ImGui::Unindent(20);
				}

				ImGui::EndChild();
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Settings")) {
				ImGui::BeginChild("SettingsChild", ImVec2(windowSize.x - 16, windowSize.y - 40), true);

				ImGui::Text("Debug Options");
				ImGui::Separator();
				ImGui::Checkbox("Show Debug Info", &showDebug);
				ImGui::Text("Players Found: %d", numPlayers);

				ImGui::EndChild();
				ImGui::EndTabItem();
			}

			ImGui::EndTabBar();
		}

		ImGui::End();
		SetFocus(overlayWindow);
	}
	if (esp) {
		// Debug counters
		static int totalPlayersDetected = 0;
		static int playersShown = 0;
		static int failedCurrentPlayerActor = 0;
		static int failedSkeletalMesh = 0;
		static int skippedSelfPlayer = 0;
		static int failedSurvivorStatus = 0;
		static int failedHealth = 0;
		static int skippedLowHealth = 0;
		static int skippedSameTeam = 0;
		static int failedBonePosition = 0;
		static int skippedDistance = 0;
		static int failedName = 0;
		static int skippedOffScreen = 0;
		static int failedPlayerState = 0;
		
		// Individual player debug info
		static std::vector<std::string> playerDebugInfo;
		
		// Reset counters and debug info each frame
		totalPlayersDetected = 0;
		playersShown = 0;
		failedCurrentPlayerActor = 0;
		failedSkeletalMesh = 0;
		skippedSelfPlayer = 0; 
		failedPlayerState = 0;
		failedSurvivorStatus = 0;
		failedHealth = 0;
		skippedLowHealth = 0;
		skippedSameTeam = 0;
		failedBonePosition = 0;
		skippedDistance = 0;
		failedName = 0;
		skippedOffScreen = 0;
		playerDebugInfo.clear();
		
		if (ReadValues()) {
			double healthL;
			read<double>(adresses.survivor_status + offset::health, healthL);
			if (healthL > 1) {
				if (read<int>(adresses.game_state + (offset::player_array + sizeof(uintptr_t)), numPlayers)) {
					totalPlayersDetected = numPlayers;
					int ackteamid = *(int*)(adresses.acknowledged_pawn + offset::team_index_pawn);
					readRaw<FMinimalViewInfo>(adresses.camera_manager + offset::pov_info, POV);
					for (int i = 0; i < numPlayers; ++i) {
						char playerInfo[200];

						DWORD64 currentPlayerActor;
						if (!read<DWORD64>((adresses.player_array + offset::player_array_data) + (i * offset::player_array_stride), currentPlayerActor)) {
							failedCurrentPlayerActor++;
							sprintf_s(playerInfo, sizeof(playerInfo), "Player %d: FAILED - No CurrentPlayerActor", i);
							playerDebugInfo.push_back(std::string(playerInfo));
							continue;
						}

						DWORD64 playerState;
						if (!read<DWORD64>(currentPlayerActor + offset::player_state, playerState)) {
							failedPlayerState++;
							sprintf_s(playerInfo, sizeof(playerInfo), "Player %d: FAILED - No PlayerState", i);
							playerDebugInfo.push_back(std::string(playerInfo));
							continue;
						}

						/*DWORD64 skeletalMesh;
						if (!read<DWORD64>(currentPlayerActor + offset::skeletal_mesh, skeletalMesh)) {
							failedSkeletalMesh++;
							sprintf_s(playerInfo, sizeof(playerInfo), "Player %d: FAILED - No SkeletalMesh", i);
							playerDebugInfo.push_back(std::string(playerInfo));
							continue;
						}*/

						if (!adresses.acknowledged_pawn || currentPlayerActor == adresses.acknowledged_pawn) {
							skippedSelfPlayer++;
							sprintf_s(playerInfo, sizeof(playerInfo), "Player %d: SKIPPED - Self Player", i);
							playerDebugInfo.push_back(std::string(playerInfo));
							continue;
						}

						DWORD64 survivorStatus;
						if (!read<DWORD64>(currentPlayerActor + offset::SurvivorStatus, survivorStatus)) {
							failedSurvivorStatus++;
							sprintf_s(playerInfo, sizeof(playerInfo), "Player %d: FAILED - No SurvivorStatus", i);
							playerDebugInfo.push_back(std::string(playerInfo));
							continue;
						}

						double health;
						if (!read<double>(survivorStatus + offset::health, health)) {
							failedHealth++;
							sprintf_s(playerInfo, sizeof(playerInfo), "Player %d: FAILED - No Health", i);
							playerDebugInfo.push_back(std::string(playerInfo));
							continue;
						}

						auto& showHealth = playerHealthStates[currentPlayerActor];

						if (health <= 1) {
							showHealth = false;
						}
						else if (health >= 100) {
							showHealth = true;
						}

						if (!showHealth) {
							skippedLowHealth++;
							sprintf_s(playerInfo, sizeof(playerInfo), "Player %d: SKIPPED - Low Health (%.1f)", i, health);
							playerDebugInfo.push_back(std::string(playerInfo));
							continue;
						}

						int teamIndex = *(int*)(currentPlayerActor + offset::team_index_pawn);

						if (team && teamIndex == ackteamid) {
							skippedSameTeam++;
							sprintf_s(playerInfo, sizeof(playerInfo), "Player %d: SKIPPED - Same Team (%d)", i, teamIndex);
							playerDebugInfo.push_back(std::string(playerInfo));
							continue;
						}

						/*if (!skeletalMesh) {
							failedSkeletalMesh++;
							sprintf_s(playerInfo, sizeof(playerInfo), "Player %d: FAILED - Null SkeletalMesh", i);
							playerDebugInfo.push_back(std::string(playerInfo));
							continue;
						}*/

						/*fvector base = get_bone_3d(skeletalMesh, bone::Root);
						fvector top = get_bone_3d(skeletalMesh, bone::Root);
						top.z += 180;
						if (base.x == 0 && base.y == 0 && base.z == 0) {
							failedBonePosition++;
							sprintf_s(playerInfo, sizeof(playerInfo), "Player %d: FAILED - Invalid Base Position", i);
							playerDebugInfo.push_back(std::string(playerInfo));
							continue;
						}
						if (top.x == 0 && top.y == 0 && top.z == 0) {
							failedBonePosition++;
							sprintf_s(playerInfo, sizeof(playerInfo), "Player %d: FAILED - Invalid Top Position", i);
							playerDebugInfo.push_back(std::string(playerInfo));
							continue;
						}*/

						DWORD64 rootComponent;
						if (!read<DWORD64>(currentPlayerActor + offset::root_component, rootComponent)) {
							failedBonePosition++;
							sprintf_s(playerInfo, sizeof(playerInfo), "Player %d: FAILED - No Root Component", i);
							playerDebugInfo.push_back(std::string(playerInfo));
							continue;
						}

						fvector base(0, 0, 0);
						if (!readRaw<fvector>(rootComponent + offset::relative_location, base)) {
							failedBonePosition++;
							sprintf_s(playerInfo, sizeof(playerInfo), "Player %d: FAILED - Invalid Base Position", i);
							playerDebugInfo.push_back(std::string(playerInfo));
							continue;
						}
						if (base.x == 0 && base.y == 0 && base.z == 0) {
							failedBonePosition++;
							sprintf_s(playerInfo, sizeof(playerInfo), "Player %d: FAILED - Invalid Base Position", i);
							playerDebugInfo.push_back(std::string(playerInfo));
							continue;
						}

						base.z -= 90;
						fvector top = base;
						top.z += 180;

						fvector2d root = w2s(base);
						fvector2d head = w2s(top);

						float distance = POV.Location.distance(base) / 100;

						if (distance > distancelimit) {
							skippedDistance++;
							sprintf_s(playerInfo, sizeof(playerInfo), "Player %d: SKIPPED - Too Far (%.1fm > %.1fm)", i, distance, distancelimit);
							playerDebugInfo.push_back(std::string(playerInfo));
							continue;
						}

						// Lectura segura en dos pasos: leer ptr1 y luego el struct Nameptr
						DWORD64 nameptr1 = 0;
						Nameptr nameData{};
						bool validName = read<DWORD64>(playerState + 0x340, nameptr1) &&
							nameptr1 != 0 &&
							read<Nameptr>(nameptr1, nameData);
						if (!validName) {
							failedName++;
							sprintf_s(playerInfo, sizeof(playerInfo), "Player %d: FAILED - No Name", i);
							playerDebugInfo.push_back(std::string(playerInfo));
						}

						if (root.x > 0 && root.y > 0 && root.x < widthscreen && root.y < heightscreen) {
							playersShown++;
							sprintf_s(playerInfo, sizeof(playerInfo), "Player %d: SHOWN - HP:%.1f Dist:%.1fm Team:%d", i, health, distance, teamIndex);
							playerDebugInfo.push_back(std::string(playerInfo));

							float textSpacing = 15.0f;
							fvector2d namePos = head;
							namePos.y -= (textSpacing - 5);

							fvector2d healthPos = root;
							healthPos.y += textSpacing;

							fvector2d distancePos = healthPos;
							distancePos.y += textSpacing;

							sprintf_s(cdistance, sizeof(cdistance), "[%0.fm]", distance);
							sprintf_s(chealth, sizeof(chealth), "HP:%0.f", health);

							ImColor playerColor = teamIndex == ackteamid ? ImColor(0, 0, 255) : ImColor(255, 0, 0);

							if (validName)
								DrawT(namePos, WideToString(nameData.Name).c_str(), 1, playerColor);
							DrawT(healthPos, chealth, 1, playerColor);
							DrawT(distancePos, cdistance, 1, playerColor);

							//DrawBones(skeletalMesh, is_visible(skeletalMesh), POV);
							drawbox(root, (root.y - head.y), (root.y - head.y) / 4, playerColor, 0);
						}
						else {
							skippedOffScreen++;
							sprintf_s(playerInfo, sizeof(playerInfo), "Player %d: SKIPPED - Off Screen (%.1f,%.1f)", i, root.x, root.y);
							playerDebugInfo.push_back(std::string(playerInfo));
						}
					}
				}
			}
		}
		
		// Draw individual player debug information (only if enabled)
		if (showDebug) {
			fvector2d debugPos = {160, 10};
			float lineSpacing = 12.0f;
			char debugLine[100];
			
			// Title and summary
			DrawT(debugPos, "=== PLAYER DEBUG INFO ===", 1, ImColor(255, 255, 0));
			debugPos.y += lineSpacing;
			
			sprintf_s(debugLine, sizeof(debugLine), "Total: %d | Shown: %d", totalPlayersDetected, playersShown);
			DrawT(debugPos, debugLine, 1, ImColor(255, 255, 255));
			debugPos.y += lineSpacing + 3;

			// POV info
			DrawT(debugPos, "=== POV INFO ===", 1, ImColor(255, 255, 0));
			debugPos.y += lineSpacing;
			sprintf_s(debugLine, sizeof(debugLine), "Loc: X=%.1f Y=%.1f Z=%.1f", POV.Location.x, POV.Location.y, POV.Location.z);
			DrawT(debugPos, debugLine, 1, ImColor(0, 200, 255));
			debugPos.y += lineSpacing;
			sprintf_s(debugLine, sizeof(debugLine), "Rot: P=%.2f Y=%.2f R=%.2f", POV.Rotation.x, POV.Rotation.y, POV.Rotation.z);
			DrawT(debugPos, debugLine, 1, ImColor(0, 200, 255));
			debugPos.y += lineSpacing;
			sprintf_s(debugLine, sizeof(debugLine), "FOV: %.1f", POV.FOV);
			DrawT(debugPos, debugLine, 1, ImColor(0, 200, 255));
			debugPos.y += lineSpacing + 3;

			// Individual player information
			for (size_t i = 0; i < playerDebugInfo.size() && i < 15; ++i) { // Limit to 15 players to avoid screen overflow
				ImColor color;
				if (playerDebugInfo[i].find("SHOWN") != std::string::npos) {
					color = ImColor(0, 255, 0); // Green for shown players
				} else if (playerDebugInfo[i].find("FAILED") != std::string::npos) {
					color = ImColor(255, 0, 0); // Red for failed players
				} else {
					color = ImColor(255, 165, 0); // Orange for skipped players
				}
				
				DrawT(debugPos, playerDebugInfo[i].c_str(), 1, color);
				debugPos.y += lineSpacing;
			}
			
			// Show if there are more players
			if (playerDebugInfo.size() > 15) {
				sprintf_s(debugLine, sizeof(debugLine), "... and %d more players", (int)(playerDebugInfo.size() - 15));
				DrawT(debugPos, debugLine, 1, ImColor(128, 128, 128));
			}
		}
	}
	ImGui::EndFrame();

	pDevice->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0);
	if (pDevice->BeginScene() >= 0) {
		ImGui::Render();
		if (ImGui::GetDrawData()) {
			ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
		}
		pDevice->EndScene();
	}

	if (pDevice->Present(NULL, NULL, NULL, NULL) == D3DERR_DEVICELOST && pDevice->TestCooperativeLevel() == D3DERR_DEVICENOTRESET) {
		ImGui_ImplDX9_InvalidateDeviceObjects();
		pDevice->Reset(&gD3DPresentParams);
		ImGui_ImplDX9_CreateDeviceObjects();
	}
}

void mainLoop() {
	static MSG msg;
	static RECT oldRect;
	ZeroMemory(&msg, sizeof(MSG));
	while (!GetAsyncKeyState(VK_DELETE) && msg.message != WM_QUIT && GetWindow(targetWindow, GW_HWNDPREV)) {
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		if (GetForegroundWindow() == targetWindow) {
			SetWindowPos(overlayWindow, GetWindow(GetForegroundWindow(), GW_HWNDPREV), 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
		}

		RECT windowRect;
		POINT windowPoint;
		ZeroMemory(&windowRect, sizeof(RECT));
		ZeroMemory(&windowPoint, sizeof(POINT));

		GetClientRect(targetWindow, &windowRect);
		ClientToScreen(targetWindow, &windowPoint);

		if (memcmp(&windowPoint, &oldRect.top, sizeof(POINT)) || memcmp(&windowRect, &oldRect, sizeof(RECT))) {
			oldRect = windowRect;
			windowInfo->Width = windowRect.right;
			windowInfo->Height = windowRect.bottom;
			SetWindowPos(overlayWindow, (HWND)0, windowPoint.x, windowPoint.y, windowInfo->Width, windowInfo->Height, SWP_NOREDRAW);
			pDevice->Reset(&gD3DPresentParams);
		}

		renderImGui();
	}
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT WINAPI WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	if (isInitialized && isMenuVisible) {
		ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);
		return TRUE;
	}
	switch (uMsg) {
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	case WM_CLOSE:
		TerminateProcess(GetCurrentProcess(), 0);
		break;
	}
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

void inputHandler() {
	ImGuiIO& io = ImGui::GetIO();
	io.MouseDown[0] = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
	for (int i = 1; i < 5; i++) {
		io.MouseDown[i] = false;
	}
}

bool createOverlay() {
	windowClass = { sizeof(WNDCLASSEX), NULL, WndProc, NULL, NULL, NULL, NULL, NULL, NULL, NULL, ovarlayName.c_str(), NULL };
	RegisterClassEx(&windowClass);

	RECT windowRect;
	POINT windowPoint;
	ZeroMemory(&windowRect, sizeof(RECT));
	ZeroMemory(&windowPoint, sizeof(POINT));

	GetClientRect(targetWindow, &windowRect);
	ClientToScreen(targetWindow, &windowPoint);

	MARGINS margins = { -1 };
	windowInfo->Width = windowRect.right;
	windowInfo->Height = windowRect.bottom;

	overlayWindow = CreateWindowEx(NULL, ovarlayName.c_str(), ovarlayName.c_str(), WS_POPUP | WS_VISIBLE, windowInfo->Left, windowInfo->Top, windowInfo->Width, windowInfo->Height, NULL, NULL, windowClass.hInstance, NULL);
	DwmExtendFrameIntoClientArea(overlayWindow, &margins);
	SetWindowLong(overlayWindow, GWL_EXSTYLE, WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW);
	ShowWindow(overlayWindow, SW_SHOW);
	UpdateWindow(overlayWindow);
	return TRUE;
}

bool createDirectX() {
	if (FAILED(Direct3DCreate9Ex(D3D_SDK_VERSION, &pDirect))) {
		return FALSE;
	}

	ZeroMemory(&gD3DPresentParams, sizeof(gD3DPresentParams));
	gD3DPresentParams.Windowed = TRUE;
	gD3DPresentParams.SwapEffect = D3DSWAPEFFECT_DISCARD;
	gD3DPresentParams.BackBufferFormat = D3DFMT_UNKNOWN;
	gD3DPresentParams.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
	gD3DPresentParams.BackBufferFormat = D3DFMT_A8R8G8B8;
	gD3DPresentParams.PresentationInterval = D3DPRESENT_INTERVAL_ONE; //VSync (Vertical Synchronization)

	if (pDirect->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, overlayWindow, D3DCREATE_HARDWARE_VERTEXPROCESSING, &gD3DPresentParams, 0, &pDevice) != D3D_OK) {
		return FALSE;
	}
	return TRUE;
}

DWORD WINAPI MainThread(HMODULE hMod) {

	windowInfo = new WindowInfo();
	bool WindowFocus = false;
	while (WindowFocus == false) {
		DWORD ForegroundWindowProcessID;
		GetWindowThreadProcessId(GetForegroundWindow(), &ForegroundWindowProcessID);
		if (getProcessID(targetProcessName) == ForegroundWindowProcessID) {
			targetWindow = GetForegroundWindow();
			RECT windowRect;
			GetWindowRect(targetWindow, &windowRect);
			windowInfo->Width = windowRect.right - windowRect.left;
			windowInfo->Height = windowRect.bottom - windowRect.top;
			windowInfo->Left = windowRect.left;
			windowInfo->Right = windowRect.right;
			windowInfo->Top = windowRect.top;
			windowInfo->Bottom = windowRect.bottom;
			WindowFocus = true;
		}
	}

	createOverlay();
	createDirectX();
	mainLoop();

	while (!GetAsyncKeyState(VK_DELETE)) {
		Sleep(500);
	}

	ImGui_ImplDX9_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	clearVariable(pDevice);
	clearVariable(pDirect);
	if (overlayWindow) {
		DestroyWindow(overlayWindow);
		UnregisterClass(windowClass.lpszClassName, windowClass.hInstance);
		overlayWindow = nullptr;
	}

	FreeLibraryAndExitThread(hMod, 0);
}

BOOL __stdcall StartThread(LPTHREAD_START_ROUTINE startaddr, HMODULE hMod) {
	return CloseHandle(CreateThread(0, 0, (LPTHREAD_START_ROUTINE)startaddr, hMod, 0, 0));
}

BOOL APIENTRY DllMain(HMODULE hmodule, DWORD dwreason, LPVOID lpreserved) {
	switch (dwreason)
	{
	case DLL_PROCESS_ATTACH:
		DisableThreadLibraryCalls(hmodule);
		hooks->GetAddreses();
		widthscreen = GetSystemMetrics(SM_CXSCREEN);
		heightscreen = GetSystemMetrics(SM_CYSCREEN);
		StartThread((LPTHREAD_START_ROUTINE)MainThread, hmodule);
	default:
		break;
	}
	return true;
}