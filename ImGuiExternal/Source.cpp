#include "include.h"
bool esp = false;
bool team = false;
bool showDebug = false;

bool aimbot = false;
bool aimTeam = false;
float aimFov = 150.0f;
float aimSmooth = 5.0f;
int aimKey = VK_RBUTTON; // Right mouse button

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

			if (ImGui::BeginTabItem("Aimbot")) {
				ImGui::BeginChild("AimbotChild", ImVec2(windowSize.x - 16, windowSize.y - 40), true);

				ImGui::Text("Aimbot Options");
				ImGui::Separator();

				ImGui::Checkbox("Enable Aimbot", &aimbot);
				if (aimbot) {
					ImGui::Indent(20);
					ImGui::Checkbox("Ignore Team Members##aim", &aimTeam);
					ImGui::SliderFloat("Aim FOV", &aimFov, 10.0f, 500.0f, "%.0f px");
					ImGui::SliderFloat("Smoothing", &aimSmooth, 1.0f, 20.0f, "%.1f");
					ImGui::Text("Aim Key: Right Mouse Button");
					ImGui::Unindent(20);
				}

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

						// Obtener posiciones de huesos (head + root)
						fvector headPos = get_bone_3d(currentPlayerActor, bone::head);
						fvector basePos = get_bone_3d(currentPlayerActor, bone::Root);
						bool usedBones = (headPos.x != 0 || headPos.y != 0 || headPos.z != 0);

						// Fallback a rootComponent si los huesos fallan
						if (!usedBones) {
							DWORD64 rootComponent;
							if (!read<DWORD64>(currentPlayerActor + offset::root_component, rootComponent)) {
								failedBonePosition++;
								sprintf_s(playerInfo, sizeof(playerInfo), "Player %d: FAILED - No Root Component", i);
								playerDebugInfo.push_back(std::string(playerInfo));
								continue;
							}
							if (!readRaw<fvector>(rootComponent + offset::relative_location, basePos)) {
								failedBonePosition++;
								sprintf_s(playerInfo, sizeof(playerInfo), "Player %d: FAILED - Invalid Base Position", i);
								playerDebugInfo.push_back(std::string(playerInfo));
								continue;
							}
							if (basePos.x == 0 && basePos.y == 0 && basePos.z == 0) {
								failedBonePosition++;
								sprintf_s(playerInfo, sizeof(playerInfo), "Player %d: FAILED - Zero Base Position", i);
								playerDebugInfo.push_back(std::string(playerInfo));
								continue;
							}
							basePos.z -= 90;
							headPos = basePos;
							headPos.z += 180;
						}

						fvector2d root = w2s(basePos);
						fvector2d head = w2s(headPos);

						float distance = POV.Location.distance(basePos) / 100;

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
							sprintf_s(playerInfo, sizeof(playerInfo), "Player %d: SHOWN - HP:%.1f Dist:%.1fm Team:%d %s", i, health, distance, teamIndex, usedBones ? "(bones)" : "(fallback)");
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

			// Bone debug info
			DrawT(debugPos, "=== BONE DEBUG ===", 1, ImColor(255, 255, 0));
			debugPos.y += lineSpacing;
			DrawT(debugPos, g_boneDebug, 1, ImColor(255, 128, 0));
			debugPos.y += lineSpacing;
			sprintf_s(debugLine, sizeof(debugLine), "sizeof(FTransform)=%d bone_stride=0x%X", (int)sizeof(FTransform), offset::bone_stride);
			DrawT(debugPos, debugLine, 1, ImColor(255, 128, 0));
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

	// === AIMBOT LOGIC ===
	// Log: buffer con el estado del ultimo tick del aimbot (visible en pantalla si showDebug)
	static char aimLog[256] = "Aimbot: idle";

	if (aimbot && GetAsyncKeyState(aimKey) & 0x8000) {
		if (!ReadValues()) {
			sprintf_s(aimLog, sizeof(aimLog), "Aimbot ERROR: ReadValues() failed (UWorld/chain roto)");
		} else {
			double healthL = 0.0;
			if (!read<double>(adresses.survivor_status + offset::health, healthL)) {
				sprintf_s(aimLog, sizeof(aimLog), "Aimbot ERROR: read LocalPlayer health failed (survivor_status=0x%llX)", adresses.survivor_status);
			} else if (healthL <= 1) {
				sprintf_s(aimLog, sizeof(aimLog), "Aimbot SKIP: jugador local muerto (health=%.1f)", healthL);
			} else {
				int numP = 0;
				if (!read<int>(adresses.game_state + (offset::player_array + sizeof(uintptr_t)), numP)) {
					sprintf_s(aimLog, sizeof(aimLog), "Aimbot ERROR: read numPlayers failed (game_state=0x%llX)", adresses.game_state);
				} else {
					int ackteamid = *(int*)(adresses.acknowledged_pawn + offset::team_index_pawn);
					readRaw<FMinimalViewInfo>(adresses.camera_manager + offset::pov_info, POV);

					double bestDist = aimFov;
					fvector bestTarget(0, 0, 0);
					bool foundTarget = false;

					int skipTeam = 0, skipDead = 0, skipOffscreen = 0, skipReadFail = 0;

					for (int i = 0; i < numP; ++i) {
						DWORD64 currentPlayerActor;
						if (!read<DWORD64>((adresses.player_array + offset::player_array_data) + (i * offset::player_array_stride), currentPlayerActor)) {
							skipReadFail++;
							continue;
						}

						if (!adresses.acknowledged_pawn || currentPlayerActor == adresses.acknowledged_pawn)
							continue;

						DWORD64 survivorStatus;
						if (!read<DWORD64>(currentPlayerActor + offset::SurvivorStatus, survivorStatus)) {
							skipReadFail++;
							continue;
						}

						double health;
						if (!read<double>(survivorStatus + offset::health, health)) {
							skipReadFail++;
							continue;
						}
						if (health <= 1) {
							skipDead++;
							continue;
						}

						int teamIndex = *(int*)(currentPlayerActor + offset::team_index_pawn);
						if (aimTeam && teamIndex == ackteamid) {
							skipTeam++;
							continue;
						}

						DWORD64 rootComponent;
						if (!read<DWORD64>(currentPlayerActor + offset::root_component, rootComponent)) {
							skipReadFail++;
							continue;
						}

						// Intentar obtener posicion del head bone para apuntar
						fvector targetPos = get_bone_3d(currentPlayerActor, bone::head);

						// Fallback a rootComponent si el hueso falla
						if (targetPos.x == 0 && targetPos.y == 0 && targetPos.z == 0) {
							if (!readRaw<fvector>(rootComponent + offset::relative_location, targetPos)) {
								skipReadFail++;
								continue;
							}
							if (targetPos.x == 0 && targetPos.y == 0 && targetPos.z == 0) {
								skipReadFail++;
								continue;
							}
							// Apuntar a la altura de la cabeza (aprox -30 desde centro)
							targetPos.z -= 30.0;
						}

						fvector2d screenPos = w2s(targetPos);
						if (screenPos.x <= 0 || screenPos.y <= 0 || screenPos.x >= widthscreen || screenPos.y >= heightscreen) {
							skipOffscreen++;
							continue;
						}

						double dist = GetCrosshairDistance(screenPos, widthscreen, heightscreen);
						if (dist < bestDist) {
							bestDist = dist;
							bestTarget = targetPos;
							foundTarget = true;
						}
					}

					if (!foundTarget) {
						sprintf_s(aimLog, sizeof(aimLog),
							"Aimbot: sin objetivo | total=%d muertos=%d equipo=%d pantalla=%d lecturaFail=%d fov=%.0f",
							numP, skipDead, skipTeam, skipOffscreen, skipReadFail, aimFov);
					} else {
						FRotator targetAngle = CalcAngle(POV.Location, bestTarget);

						// Leer rotacion actual
						FRotator currentRot{};
						if (!readRaw<FRotator>(adresses.player_controller + offset::control_rotation, currentRot)) {
							sprintf_s(aimLog, sizeof(aimLog),
								"Aimbot ERROR: readRaw ControlRotation fallo (PC=0x%llX off=0x%X)",
								adresses.player_controller, (unsigned)offset::control_rotation);
						} else {
							FRotator smoothed = SmoothRotation(currentRot, targetAngle, aimSmooth);

							bool wP = write<double>(adresses.player_controller + offset::control_rotation + 0x00, smoothed.Pitch);
							bool wY = write<double>(adresses.player_controller + offset::control_rotation + 0x08, smoothed.Yaw);
							bool wR = write<double>(adresses.player_controller + offset::control_rotation + 0x10, smoothed.Roll);

							if (!wP || !wY || !wR) {
								sprintf_s(aimLog, sizeof(aimLog),
									"Aimbot ERROR: write ControlRotation fallo (P=%d Y=%d R=%d) addr=0x%llX",
									(int)wP, (int)wY, (int)wR,
									adresses.player_controller + offset::control_rotation);
							} else {
								sprintf_s(aimLog, sizeof(aimLog),
									"Aimbot OK: P=%.2f Y=%.2f dist=%.0fpx target=(%.0f,%.0f,%.0f)",
									smoothed.Pitch, smoothed.Yaw, bestDist,
									bestTarget.x, bestTarget.y, bestTarget.z);
							}
						}
					}
				}
			}
		}
	} else if (aimbot) {
		sprintf_s(aimLog, sizeof(aimLog), "Aimbot: listo (mantener RMB para activar)");
	}

	// Mostrar log del aimbot en pantalla si showDebug esta activo
	if (showDebug && aimbot) {
		fvector2d logPos = { 160.0, (double)heightscreen - 30.0 };
		ImColor logColor = (strstr(aimLog, "ERROR") != nullptr) ? ImColor(255, 60, 60)
		                 : (strstr(aimLog, "OK")    != nullptr) ? ImColor(60, 255, 60)
		                 :                                        ImColor(255, 200, 0);
		DrawT(logPos, aimLog, 1, logColor);
	}

	// Dibujar circulo de FOV del aimbot
	if (aimbot) {
		fvector2d center(widthscreen / 2.0, heightscreen / 2.0);
		DrawCircle(center, (int)aimFov, 1, ImColor(255, 255, 255, 100));
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