#include "include.h"

bool esp = false;
bool team = false;

bool isInitialized = false;
bool isMenuVisible = true;

bool there = false;

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

	ImGui_ImplDX9_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	SetWindowLong(overlayWindow, GWL_EXSTYLE, isMenuVisible ? WS_EX_TOOLWINDOW : (WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW));
	UpdateWindow(overlayWindow);
	if (isMenuVisible) {
		inputHandler();
		drawItem();
		ImGui::Begin("NOVA");
		ImGui::Checkbox("esp", &esp);
		if (esp)
			ImGui::Checkbox("Ignore Team", &team);
		ImGui::Text("%d", there);
		ImGui::End();
		SetFocus(overlayWindow);
	}
	if (esp) {
		if (ReadValues()) {
			int numPlayers;
			if (read<int>(adresses.game_state, (0x2A8 + sizeof(uintptr_t)), numPlayers)) {
				int ackteamid;
				if (read<int>(adresses.acknowledged_pawn, 0x1000, ackteamid)) {
					for (int i = 0; i < numPlayers; ++i) {
						DWORD64 playerState;
						if (!read<DWORD64>(adresses.player_array, (i * sizeof(uintptr_t)), playerState)) continue;

						DWORD64 currentPlayerActor;
						if (!read<DWORD64>(playerState, offset::pawn_private, currentPlayerActor)) continue;

						DWORD64 skeletalMesh;
						if (!read<DWORD64>(currentPlayerActor, offset::skeletal_mesh, skeletalMesh)) continue;

						if (!adresses.acknowledged_pawn || currentPlayerActor == adresses.acknowledged_pawn) continue;

						name = reinterpret_cast<Name*>(playerState);
						if (!name) continue;

						DWORD64 survivorStatus;
						if (!read<DWORD64>(currentPlayerActor, 0x0638, survivorStatus)) continue;

						double health;
						if (!read<double>(survivorStatus, 0x00B0, health)) continue;

						if (health < 1) continue;

						there = true;

						int teamIndex;
						if (!read<int>(currentPlayerActor, 0x1000, teamIndex)) continue;

						if (team && teamIndex == ackteamid) continue;

						if (!skeletalMesh) continue;
						fvector base = get_bone_3d(skeletalMesh, bone::Root);
						if (base.x == 0 && base.y == 0 && base.z == 0) continue;
						fvector2d root = w2s(base);

						if (root.x > 0 && root.y > 0 && root.x < 1920 && root.y < 1080) {
							DrawBones(skeletalMesh);
						}
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