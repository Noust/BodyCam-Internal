#include "include.h"

bool esp = false;
bool team = false;
static bool showHealth = true;

bool isInitialized = false;
bool isMenuVisible = true;
char cdistance[25];
char chealth[25];
float distancelimit = 50;
int numPlayers;

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
				
				ImGui::Text("Debug Info");
				ImGui::Separator();
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
		if (ReadValues()) {
			if (read<int>(adresses.game_state + (0x2A8 + sizeof(uintptr_t)), numPlayers)) {
				int ackteamid;
				read<int>(adresses.acknowledged_pawn + 0xFA0, ackteamid);
				for (int i = 0; i < numPlayers; ++i) {
					DWORD64 playerState;
					if (!read<DWORD64>(adresses.player_array + (i * sizeof(uintptr_t)), playerState)) continue;

					DWORD64 currentPlayerActor;
					if (!read<DWORD64>(playerState + offset::pawn_private, currentPlayerActor)) continue;

					DWORD64 skeletalMesh;
					if (!read<DWORD64>(currentPlayerActor + offset::skeletal_mesh, skeletalMesh)) continue;

					if (!adresses.acknowledged_pawn || currentPlayerActor == adresses.acknowledged_pawn) continue;

					//name = reinterpret_cast<Name*>(playerState);
					//if (!name) continue;

					DWORD64 survivorStatus;
					if (!read<DWORD64>(currentPlayerActor + 0x0638, survivorStatus)) continue;

					double health;
					if (!read<double>(survivorStatus + 0x00B0, health)) continue;

					if (health <= 0) {
						showHealth = false;
					} else if (health >= 100) {
						showHealth = true;
					}

					if (!showHealth) continue;

					int teamIndex;
					if (!read<int>(currentPlayerActor + 0xFA0, teamIndex) && team) continue;

					if (team && teamIndex == ackteamid) continue;

					camera_postion = get_camera();

					if (!skeletalMesh) continue;
					fvector base = get_bone_3d(skeletalMesh, bone::Root);
					if (base.x == 0 && base.y == 0 && base.z == 0) continue;
					fvector2d root = w2s(base);

					float distance = camera_postion.location.distance(base) / 100;

					there = true;

					if (distance > distancelimit)
						continue;

					sprintf_s(cdistance, sizeof(cdistance), "[%0.fm]", distance);
					sprintf_s(chealth, sizeof(chealth), "HP:%0.f", health);

					if (root.x > 0 && root.y > 0 && root.x < 1920 && root.y < 1080) {
						DrawBones(skeletalMesh, is_visible(skeletalMesh));
						DrawT(root, cdistance, 4, ImColor(255, 0, 0));
						DrawT(root, chealth, 1, ImColor(255, 0, 0));
					}
				}
			}
		}
	}
	ImGui::EndFrame();

	pDevice->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0);
	if (pDevice->BeginScene() >= 0) {
		ImGui::Render();
		ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
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