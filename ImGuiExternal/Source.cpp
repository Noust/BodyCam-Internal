#include "include.h"
#include <cmath>
#include <cctype>
#include <cfloat>
#include <cstring>
#include <new>

/* ===========================================================================
 *  NOVA - Bodycam
 *
 *  DLL inyectada: overlay D3D9 + ImGui propio, lectura in-process.
 *  Offsets en HookFunc.h; evidencia de cada uno en context/REVERSE_ENGINEERING.md.
 * ======================================================================== */

/* ------------------------------- CONFIG -------------------------------- */

enum BoxMode : int { BOX_NONE = 0, BOX_FULL = 1, BOX_CORNERS = 2 };

struct EspConfig {
	bool enabled = false;

	struct Players {
		int   boxMode = BOX_FULL;
		bool  boxFromBones = true;   /* caja ajustada a la pose real       */
		float boxScale = 1.0f;
		bool  name = true;
		bool  health = true;
		bool  distance = true;
		bool  skeleton = false;
		bool  headDot = false;
		float headDotSize = 4.0f;
		bool  snapline = false;
		bool  showEnemy = true;
		bool  showTeam = false;
		bool  showDrones = true;      /* al morir se pasa a controlar un dron */
		bool  hideDead = true;
		float maxDistance = 300.0f;   /* metros */
	} players;

	struct Aim {
		bool  enabled = false;
		bool  ignoreTeam = true;
		float fov = 150.0f;
		float smooth = 5.0f;
		int   method = 0;          /* 0 = AddYaw/PitchInput (funcion del juego) */
		int   boneMode = 0;        /* 0 = cabeza, 1 = cuerpo                    */
		float maxStep = 25.0f;     /* tope de grados por frame                  */
		bool  drawFov = true;
		bool  drawTarget = false;

		/* Soft aim: en vez de mantener un boton, corrige en el instante del
		 * disparo. Ver la nota en RunAimbot sobre lo que esto SI y NO hace. */
		bool  softAim = false;
		float softFov = 120.0f;
		float softSmooth = 1.0f;   /* 1 = corregir todo el error de golpe */
		bool  softHeadOnly = true;
	} aim;
} g_Cfg;

enum AimMethod : int {
	AIM_GAME_FUNCTION = 0,   /* AddYawInput / AddPitchInput  <- recomendado */
	AIM_ROTATION_INPUT = 1,  /* escritura directa en RotationInput          */
	AIM_CONTROL_ROTATION = 2 /* escritura directa en ControlRotation (viejo)*/
};

/* Diagnostico del ESP: cada motivo de descarte por separado. Un contador
 * global de "no se ve nada" no permite arreglar nada. */
struct EspDiag {
	int total = 0, drawn = 0;
	int noPawn = 0, self = 0, noHealth = 0, dead = 0, teamFiltered = 0;
	int tooFar = 0, offScreen = 0, noPosition = 0;
	int drones = 0, droneFiltered = 0;
	void reset() {
		total = drawn = noPawn = self = noHealth = dead = teamFiltered = 0;
		tooFar = offScreen = noPosition = drones = droneFiltered = 0;
	}
} g_Diag;

static bool  isInitialized = false;
static bool  isMenuVisible = true;
static int   g_MenuSection = 0;
static char  g_UiFilter[64] = "";
static int   g_UiShown = 0;
static char  g_AimLog[256] = "Aimbot: idle";
static int   g_AimKey = VK_RBUTTON;

/* ------------------------- VENTANA / DIRECTX --------------------------- */

struct WindowInfo { int Width, Height, Left, Right, Top, Bottom; };

static WindowInfo* windowInfo = nullptr;
static WNDCLASSEX  windowClass{};
static HWND        targetWindow = nullptr;
static HWND        overlayWindow = nullptr;

static std::string targetProcessName = "Bodycam-Win64-Shipping.exe";
static std::string ovarlayName;

static IDirect3DDevice9Ex* pDevice = nullptr;
static IDirect3D9Ex* pDirect = nullptr;
static D3DPRESENT_PARAMETERS gD3DPresentParams = { NULL };

/* ------------------------------ UI HELPERS ----------------------------- */

static constexpr float UI_LABEL_W = 170.0f;

/* Filtro del buscador. Devuelve true si la opcion debe mostrarse. */
static bool UiPass(const char* label) {
	if (g_UiFilter[0] == '\0') { g_UiShown++; return true; }
	char a[128], b[128];
	int i = 0;
	for (; label[i] && i < 127; ++i) a[i] = (char)tolower((unsigned char)label[i]);
	a[i] = '\0';
	for (i = 0; g_UiFilter[i] && i < 127; ++i) b[i] = (char)tolower((unsigned char)g_UiFilter[i]);
	b[i] = '\0';
	if (strstr(a, b)) { g_UiShown++; return true; }
	return false;
}

/* El "(?)" va pegado a la etiqueta, nunca despues del control: los controles
 * usan ancho -1 y cualquier cosa a su derecha se sale del panel. */
static void UiHelp(const char* help) {
	if (!help || !*help) return;
	ImGui::SameLine(0.0f, 4.0f);
	ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered()) {
		ImGui::BeginTooltip();
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 26.0f);
		ImGui::TextUnformatted(help);
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
}

static void UiLabel(const char* label, const char* help) {
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(label);
	UiHelp(help);
	ImGui::SameLine(UI_LABEL_W);
}

static bool UiToggle(const char* label, bool* v, const char* help = nullptr) {
	if (!UiPass(label)) return false;
	ImGui::PushID(label);
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(label);
	UiHelp(help);
	ImGui::SameLine(UI_LABEL_W);
	const bool r = ImGui::Checkbox("##v", v);
	ImGui::PopID();
	return r;
}

static bool UiSlider(const char* label, float* v, float lo, float hi,
                     const char* fmt, const char* help = nullptr) {
	if (!UiPass(label)) return false;
	ImGui::PushID(label);
	UiLabel(label, help);
	ImGui::SetNextItemWidth(-1.0f);
	const bool r = ImGui::SliderFloat("##v", v, lo, hi, fmt);
	ImGui::PopID();
	return r;
}

static bool UiCombo(const char* label, int* v, const char* items, const char* help = nullptr) {
	if (!UiPass(label)) return false;
	ImGui::PushID(label);
	UiLabel(label, help);
	ImGui::SetNextItemWidth(-1.0f);
	const bool r = ImGui::Combo("##v", v, items);
	ImGui::PopID();
	return r;
}

static void UiGroup(const char* title) {
	ImGui::Dummy(ImVec2(0, 4));
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.33f, 0.67f, 0.86f, 1.0f));
	ImGui::TextUnformatted(title);
	ImGui::PopStyleColor();
	ImGui::Separator();
	ImGui::Dummy(ImVec2(0, 2));
}

/* ------------------------------- ESP ----------------------------------- */

/* Caja a partir de la capsula de colision. Es el camino de respaldo y el que
 * se usa cuando la pose no esta disponible.
 * La capsula ENCOGE al agacharse o tumbarse, asi que hay que releerla cada
 * frame: es lo que hace que la caja siga la postura. */
static bool BoxFromCapsule(uintptr_t pawn, ImVec2& tl, ImVec2& br, fvector& rootOut,
                           bool hasCapsule = true) {
	uintptr_t root = 0;
	if (!readPtr(pawn + offset::root_component, root)) return false;

	fvector pos{};
	if (!readRaw<fvector>(root + offset::c2w_translation, pos)) return false;
	rootOut = pos;

	/* HalfHeight y Radius son adyacentes: una sola lectura de 8 bytes.
	 *
	 * Solo se leen si el pawn es un ACharacter. Un dron deriva de APawn, donde
	 * el offset 0x338 NO es la capsula sino otra cosa: leerlo ahi daria valores
	 * sin sentido. Para esos se usa un tamano fijo pequeno. */
	float cap[2] = { 88.0f, 34.0f };
	if (hasCapsule) {
		uintptr_t capsule = 0;
		if (readPtr(pawn + offset::capsule_component, capsule))
			readBytes(capsule + offset::capsule_half_height, cap, sizeof(cap));
	}
	else {
		cap[0] = 30.0f; cap[1] = 30.0f;   /* dron: caja compacta y cuadrada */
	}

	float halfH = cap[0], radius = cap[1];
	if (!(halfH == halfH) || halfH < 10.0f || halfH > 500.0f) halfH = 88.0f;
	if (!(radius == radius) || radius < 5.0f || radius > 300.0f) radius = 34.0f;

	fvector top = pos, bottom = pos;
	top.z += halfH;
	bottom.z -= halfH;

	fvector2d sTop, sBot;
	if (!W2S(top, sTop) || !W2S(bottom, sBot)) return false;

	const float h = (float)(sBot.y - sTop.y);
	if (h <= 1.0f) return false;
	const float w = h * (radius / halfH);

	const float cx = (float)((sTop.x + sBot.x) * 0.5);
	tl = ImVec2(cx - w * 0.5f, (float)sTop.y);
	br = ImVec2(cx + w * 0.5f, (float)sBot.y);
	return true;
}

/* Caja ajustada a la pose real: bounding box de todos los huesos proyectados.
 * Se exige un minimo de huesos visibles; si el jugador esta medio detras de la
 * camara la caja saldria deformada y es mejor caer a la capsula. */
static bool BoxFromPose(const PawnPose& pose, ImVec2& tl, ImVec2& br) {
	if (!pose.ok || pose.count < 4) return false;

	/* Solo se toma el rango VERTICAL de la pose, y el centro horizontal.
	 *
	 * Antes se usaba el bounding box completo, y el ancho bailaba cada vez que
	 * el enemigo movía los brazos al correr. La altura sí interesa (es lo que
	 * hace que la caja siga al agacharse), pero el ancho se deriva de ella con
	 * una proporción humana fija, así que se queda quieto. */
	float minY = FLT_MAX, maxY = -FLT_MAX;
	double sumX = 0.0;
	int visible = 0;
	for (int i = 0; i < pose.count; ++i) {
		fvector2d s;
		if (!W2S(pose.world[i], s)) continue;
		++visible;
		sumX += s.x;
		minY = (std::min)(minY, (float)s.y);
		maxY = (std::max)(maxY, (float)s.y);
	}
	if (visible < pose.count * 3 / 5 || visible < 4) return false;

	const float h = maxY - minY;
	if (h < 2.0f) return false;

	const float cx = (float)(sumX / visible);
	const float padY = h * 0.05f;
	const float top = minY - padY;
	const float bot = maxY + padY;
	/* Proporción humana en pantalla: alto / ancho ≈ 2.6 */
	const float w = (bot - top) / 2.6f;

	tl = ImVec2(cx - w * 0.5f, top);
	br = ImVec2(cx + w * 0.5f, bot);
	return true;
}

static void ScaleBox(ImVec2& tl, ImVec2& br, float scale) {
	if (scale == 1.0f) return;
	const float cx = (tl.x + br.x) * 0.5f, cy = (tl.y + br.y) * 0.5f;
	const float hw = (br.x - tl.x) * 0.5f * scale, hh = (br.y - tl.y) * 0.5f * scale;
	tl = ImVec2(cx - hw, cy - hh);
	br = ImVec2(cx + hw, cy + hh);
}

/* ---------------------------------------------------------------------------
 *  Tipo de pawn
 *
 *  Al morir, el jugador pasa a controlar un dron (y tambien puede sacar uno
 *  estando vivo), asi que el pawn del PlayerState deja de ser un personaje.
 *  Por eso NO desaparecen al morir: siguen siendo entidades validas.
 *
 *  Se distingue por el nombre de la clase, resuelto con GNames. El resultado se
 *  cachea POR UClass, nunca por objeto: la clase es la misma para todos los
 *  drones y no cambia, asi que se resuelve una vez por tipo.
 * ------------------------------------------------------------------------- */
enum PawnKind : int { PK_UNKNOWN = 0, PK_PLAYER = 1, PK_DRONE = 2 };

static std::unordered_map<uintptr_t, int> g_ClassKind;

static int ClassifyPawn(uintptr_t pawn) {
	uintptr_t cls = 0;
	if (!readPtr(pawn + Names::kUObject_Class, cls)) return PK_UNKNOWN;

	auto it = g_ClassKind.find(cls);
	if (it != g_ClassKind.end()) return it->second;

	char name[128] = "";
	int kind = PK_UNKNOWN;
	if (Names::ReadFName(cls + Names::kUObject_Name, name, sizeof(name)) && name[0]) {
		if (Names::ContainsCI(name, "drone") || Names::ContainsCI(name, "perk"))
			kind = PK_DRONE;
		else if (Names::ContainsCI(name, "character") || Names::ContainsCI(name, "pawn"))
			kind = PK_PLAYER;
		/* Solo se cachea cuando se pudo leer el nombre: si GNames aun no estaba
		 * listo, hay que poder reintentar en el siguiente frame. */
		if (g_ClassKind.size() > 256) g_ClassKind.clear();
		g_ClassKind[cls] = kind;
	}
	return kind;
}

/* Esqueleto.
 *
 * Se dibujan SOLO los huesos del cuerpo, identificados por su nombre real en el
 * FReferenceSkeleton. Dibujar los ~150 huesos del modelo daba una maraña: los
 * dedos amontonan líneas en las manos y los huesos de anclaje del arma salen
 * disparados lejos del personaje.
 *
 * `drawParent` ya salta los huesos descartados, así que la cadena no se rompe
 * aunque entre dos huesos del cuerpo haya twists o correctivos por medio. */
static void DrawSkeleton(const PawnPose& pose, const SkeletonInfo* skel, ImU32 col) {
	if (!skel || !skel->valid) return;
	const int n = (std::min)(pose.count, (int)skel->parents.size());
	if (n <= 1) return;

	int drawn = 0;

	if (skel->named && skel->coreCount >= 4) {
		for (int i = 0; i < n; ++i) {
			if (!skel->keep[i]) continue;
			const int p = skel->drawParent[i];
			if (p < 0 || p >= n) continue;
			fvector2d a, b;
			if (!W2S(pose.world[i], a)) continue;
			if (!W2S(pose.world[p], b)) continue;
			Render::Line(ImVec2((float)a.x, (float)a.y), ImVec2((float)b.x, (float)b.y), col);
			++drawn;
		}
	}
	else {
		/* Respaldo si no se pudieron leer los nombres: se dibuja hueso->padre,
		 * pero descartando los segmentos absurdamente largos, que son los que
		 * producen las líneas disparadas hacia fuera del cuerpo. */
		const double maxLen = 60.0;   /* cm; ningún hueso del cuerpo mide más */
		for (int i = 1; i < n; ++i) {
			const int p = skel->parents[i];
			if (p < 0 || p >= n) continue;
			if (pose.world[i].distance(pose.world[p]) > maxLen) continue;
			fvector2d a, b;
			if (!W2S(pose.world[i], a)) continue;
			if (!W2S(pose.world[p], b)) continue;
			Render::Line(ImVec2((float)a.x, (float)a.y), ImVec2((float)b.x, (float)b.y), col);
			++drawn;
		}
	}
	if (drawn) g_BoneDiag.drawn++;
}

static void RenderESP() {
	g_Diag.reset();
	g_BoneDiag.reset();

	if (!g_Cfg.enabled) return;
	if (!ReadValues()) return;
	if (!ReadCamera(g_View)) return;

	const uintptr_t data = adresses.player_array;
	const int count = adresses.player_count;
	if (!IsValidPtr(data) || count <= 0) return;

	g_Diag.total = count;

	const ImU32 colEnemy = IM_COL32(255, 60, 60, 255);
	const ImU32 colTeam = IM_COL32(80, 150, 255, 255);
	const float maxDistCm = g_Cfg.players.maxDistance * 100.0f;

	for (int i = 0; i < count; ++i) {
		/* PlayerArray es TArray<APlayerState*>: stride 8, Data en +0. */
		uintptr_t ps = 0;
		if (!readPtr(data + (uintptr_t)i * offset::player_array_stride, ps)) { g_Diag.noPawn++; continue; }

		uintptr_t pawn = 0;
		if (!readPtr(ps + offset::ps_pawn, pawn)) { g_Diag.noPawn++; continue; }

		if (adresses.acknowledged_pawn && pawn == adresses.acknowledged_pawn) { g_Diag.self++; continue; }

		/* Ojo: read<T> pone el destino a CERO si falla, no deja el valor previo.
		 * Sin comprobar el retorno, un fallo de lectura daria teamId = 0, que es
		 * un equipo valido: un enemigo podria quedar marcado como aliado y
		 * ocultarse. Ante la duda, equipo desconocido (-1) = tratar como enemigo. */
		int teamId = -1;
		if (!read<int>(ps + offset::ps_team_id, teamId)) teamId = -1;
		const bool sameTeam = (adresses.local_team >= 0 && teamId >= 0 && teamId == adresses.local_team);

		if (sameTeam && !g_Cfg.players.showTeam) { g_Diag.teamFiltered++; continue; }
		if (!sameTeam && !g_Cfg.players.showEnemy) { g_Diag.teamFiltered++; continue; }

		const int kind = ClassifyPawn(pawn);
		const bool isDrone = (kind == PK_DRONE);

		/* La vida vive en ABodycamCharacter+0x668. Un dron no es un Character,
		 * asi que ese offset cae fuera de su clase: no se lee. */
		float health = 0.0f, maxHealth = 100.0f;
		const bool gotHealth = !isDrone && ReadHealth(pawn, health, maxHealth);
		if (!gotHealth && !isDrone) g_Diag.noHealth++;
		if (g_Cfg.players.hideDead && gotHealth && health <= 0.0f) { g_Diag.dead++; continue; }

		if (isDrone) g_Diag.drones++;
		if (isDrone && !g_Cfg.players.showDrones) { g_Diag.droneFiltered++; continue; }

		/* Pose (para esqueleto, caja ajustada y punto de cabeza).
		 * En un dron no hay esqueleto humano, asi que la caja sale de la
		 * capsula: ajustarla a "la pose" de un dron da formas raras. */
		PawnPose pose;
		const bool hasPose = !isDrone && ReadPawnPose(pawn, pose);

		ImVec2 tl, br;
		fvector rootPos{};
		bool haveBox = false;

		if (g_Cfg.players.boxFromBones && hasPose)
			haveBox = BoxFromPose(pose, tl, br);
		if (!haveBox)
			haveBox = BoxFromCapsule(pawn, tl, br, rootPos, !isDrone);

		if (!haveBox) { g_Diag.noPosition++; continue; }

		/* Referencia para la distancia: el hueso raiz si hay pose (siempre esta
		 * en los pies del modelo), y si no el centro de la capsula. */
		const fvector refPos = (hasPose && pose.count > 0) ? pose.world[0] : rootPos;
		double distCm = g_View.Location.distance(refPos);
		if (distCm <= 0.0 || distCm > 1e7) distCm = 0.0;

		if (maxDistCm > 0.0f && distCm > maxDistCm) { g_Diag.tooFar++; continue; }

		ScaleBox(tl, br, g_Cfg.players.boxScale);

		/* Si la caja entera queda fuera de pantalla, no hay nada que pintar. */
		if (br.x < 0 || br.y < 0 || tl.x > widthscreen || tl.y > heightscreen) {
			g_Diag.offScreen++;
			continue;
		}

		const ImU32 col = sameTeam ? colTeam : colEnemy;
		g_Diag.drawn++;

		if (g_Cfg.players.boxMode == BOX_FULL)         Render::Box(tl, br, col);
		else if (g_Cfg.players.boxMode == BOX_CORNERS) Render::CornerBox(tl, br, col);

		if (g_Cfg.players.health && gotHealth && maxHealth > 0.0f)
			Render::HealthBar(tl, br, health / maxHealth * 100.0f);

		if (g_Cfg.players.snapline)
			Render::Snapline(ImVec2(widthscreen * 0.5f, heightscreen), ImVec2((tl.x + br.x) * 0.5f, br.y), col);

		/* Un solo lookup del esqueleto por jugador; lo usan el esqueleto y el
		 * punto de cabeza. */
		const SkeletonInfo* skel = nullptr;
		if (hasPose && (g_Cfg.players.skeleton || g_Cfg.players.headDot))
			skel = GetSkeletonForPawn(pawn);

		if (g_Cfg.players.skeleton && hasPose)
			DrawSkeleton(pose, skel, col);

		/* Textos: nombre encima, vida y distancia debajo. */
		float ty = tl.y - Render::FontSize() - 2.0f;
		const float cx = (tl.x + br.x) * 0.5f;

		if (g_Cfg.players.name) {
			char nameBuf[64] = "";
			if (ReadPlayerName(ps, nameBuf, sizeof(nameBuf)) && nameBuf[0]) {
				Render::Text(ImVec2(cx, ty), nameBuf, col);
				ty -= Render::FontSize() + 1.0f;
			}
		}
		/* Etiqueta de dron encima del nombre, en su propio color. */
		if (isDrone) {
			Render::Text(ImVec2(cx, ty), "[DRONE]", IM_COL32(0, 220, 255, 255));
			ty -= Render::FontSize() + 1.0f;
		}

		float by = br.y + 2.0f;
		if (g_Cfg.players.health && gotHealth) {
			char hp[32];
			sprintf_s(hp, sizeof(hp), "%.0f HP", health);
			Render::Text(ImVec2(cx, by), hp, Render::HealthColor(health / maxHealth * 100.0f));
			by += Render::FontSize() + 1.0f;
		}
		if (g_Cfg.players.distance && distCm > 0.0) {
			char d[32];
			sprintf_s(d, sizeof(d), "%.0f m", distCm / 100.0);
			Render::Text(ImVec2(cx, by), d, IM_COL32(220, 220, 220, 255));
		}

		if (g_Cfg.players.headDot && hasPose && skel && skel->head >= 0 && skel->head < pose.count) {
			fvector2d h;
			if (W2S(pose.world[skel->head], h))
				Render::Circle(ImVec2((float)h.x, (float)h.y), g_Cfg.players.headDotSize, col);
		}
	}
}

/* ------------------------------ AIMBOT --------------------------------- */

/* Objetivo del ultimo tick, para poder dibujarlo. */
static bool    g_AimHasTarget = false;
static fvector g_AimTargetWorld;

/* ---------------------------------------------------------------------------
 *  SOFT AIM — que hace exactamente, y que NO hace
 *
 *  Lo que hace: cuando disparas, y solo en ese instante, corrige la punteria
 *  hacia la cabeza del enemigo mas cercano al centro dentro de su FOV. La
 *  correccion se aplica de golpe (softSmooth = 1) por la misma via verificada
 *  que el aimbot: RotationInput, a traves de AddYawInput/AddPitchInput.
 *
 *  Lo que NO hace: redirigir la bala sin mover la vista. Eso exigiria
 *  interceptar el calculo de direccion del disparo dentro del juego, o sea un
 *  hook con trampolin sobre la funcion de trace. Esa funcion no esta
 *  identificada en este binario, y el `hookclass::Hook` que trae el proyecto
 *  sobrescribe la funcion original sin conservarla, asi que no serviria.
 *  Montarlo requiere analisis nuevo y un motor de hooks de verdad.
 *
 *  Consecuencia practica: aqui la mira SI se mueve, aunque sea durante un solo
 *  frame en el momento del disparo.
 * ------------------------------------------------------------------------- */
static void RunAimbot() {
	g_AimHasTarget = false;

	const bool holdingAim = (GetAsyncKeyState(g_AimKey) & 0x8000) != 0;
	const bool firing = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;

	const bool aimbotActive = g_Cfg.aim.enabled && holdingAim;
	const bool softActive = g_Cfg.aim.softAim && firing;

	if (!g_Cfg.aim.enabled && !g_Cfg.aim.softAim) { strcpy_s(g_AimLog, "Aimbot: off"); return; }
	if (!aimbotActive && !softActive) {
		strcpy_s(g_AimLog, g_Cfg.aim.softAim ? "Ready (soft aim on fire / hold RMB)"
		                                     : "Ready (hold RMB)");
		return;
	}

	/* El soft aim manda cuando esta disparando: corrige de golpe y con su
	 * propio FOV, que suele ser mas cerrado que el del aimbot. */
	const bool useSoft = softActive;
	const double activeFov = useSoft ? (double)g_Cfg.aim.softFov : (double)g_Cfg.aim.fov;
	const double activeSmooth = useSoft
		? ((g_Cfg.aim.softSmooth < 1.0f) ? 1.0 : (double)g_Cfg.aim.softSmooth)
		: ((g_Cfg.aim.smooth < 1.0f) ? 1.0 : (double)g_Cfg.aim.smooth);
	const int activeBone = useSoft ? (g_Cfg.aim.softHeadOnly ? 0 : g_Cfg.aim.boneMode)
	                               : g_Cfg.aim.boneMode;

	if (!ReadValues()) { sprintf_s(g_AimLog, "Aimbot: %s", ChainStageName(adresses.stage)); return; }
	if (!ReadCamera(g_View)) { strcpy_s(g_AimLog, "Aimbot: no camera"); return; }
	if (!adresses.acknowledged_pawn) { strcpy_s(g_AimLog, "Aimbot: no local pawn"); return; }

	const uintptr_t data = adresses.player_array;
	const int count = adresses.player_count;
	if (!IsValidPtr(data) || count <= 0) { strcpy_s(g_AimLog, "Aimbot: empty roster"); return; }

	double bestDist = activeFov;
	fvector bestTarget(0, 0, 0);
	bool found = false;
	int skipTeam = 0, skipDead = 0, skipOff = 0, skipRead = 0, skipDrone = 0;

	for (int i = 0; i < count; ++i) {
		uintptr_t ps = 0, pawn = 0;
		if (!readPtr(data + (uintptr_t)i * offset::player_array_stride, ps)) { skipRead++; continue; }
		if (!readPtr(ps + offset::ps_pawn, pawn)) { skipRead++; continue; }
		if (pawn == adresses.acknowledged_pawn) continue;

		/* Mismo cuidado que en el ESP: si la lectura falla, teamId queda a 0 y un
		 * enemigo podria pasar por aliado (y el aimbot lo ignoraria). */
		int teamId = -1;
		if (!read<int>(ps + offset::ps_team_id, teamId)) teamId = -1;
		if (g_Cfg.aim.ignoreTeam && adresses.local_team >= 0 && teamId >= 0 &&
		    teamId == adresses.local_team) { skipTeam++; continue; }

		/* Un dron no es un ACharacter: ni su vida ni su malla estan donde las de
		 * un jugador. Leerle la pose daria posiciones inventadas y el aimbot
		 * apuntaria a la nada, asi que se descarta como objetivo. */
		if (ClassifyPawn(pawn) == PK_DRONE) { skipDrone++; continue; }

		float hp = 0.0f, maxHp = 100.0f;
		if (ReadHealth(pawn, hp, maxHp) && hp <= 0.0f) { skipDead++; continue; }

		PawnPose pose;
		if (!ReadPawnPose(pawn, pose)) { skipRead++; continue; }

		/* Cabeza segun el esqueleto de referencia; si no se resuelve, el hueso
		 * raiz. En modo "cuerpo" se apunta a la mitad de la altura del modelo,
		 * que es mas estable que la raiz (que esta en los pies). */
		const SkeletonInfo* sk = GetSkeletonForPawn(pawn);
		fvector target;
		if (activeBone == 0 && sk && sk->head >= 0 && sk->head < pose.count) {
			target = pose.world[sk->head];
		}
		else {
			target = pose.world[0];
			if (sk && sk->head >= 0 && sk->head < pose.count)
				target.z = (pose.world[0].z + pose.world[sk->head].z) * 0.5;
		}

		fvector2d s;
		if (!W2S(target, s)) { skipOff++; continue; }
		if (!OnScreen(s)) { skipOff++; continue; }

		const double d = GetCrosshairDistance(s, widthscreen, heightscreen);
		if (d < bestDist) { bestDist = d; bestTarget = target; found = true; }
	}

	if (!found) {
		sprintf_s(g_AimLog, "Aimbot: no target | total=%d dead=%d team=%d drone=%d offscreen=%d readfail=%d",
		          count, skipDead, skipTeam, skipDrone, skipOff, skipRead);
		return;
	}

	g_AimHasTarget = true;
	g_AimTargetWorld = bestTarget;

	/* Angulo que queremos, y el que tenemos ahora mismo. ControlRotation es la
	 * rotacion REAL de la mira, asi que sirve como referencia aunque no la
	 * escribamos. */
	const FRotator want = CalcAngle(g_View.Location, bestTarget);
	FRotator cur{};
	if (!readRaw<FRotator>(adresses.player_controller + offset::control_rotation, cur)) {
		strcpy_s(g_AimLog, "Aimbot: read ControlRotation failed");
		return;
	}

	const double smooth = activeSmooth;
	const double errYaw = NormalizeAngle(want.Yaw - cur.Yaw);
	const double errPitch = NormalizeAngle(want.Pitch - cur.Pitch);
	const double stepYaw = errYaw / smooth;
	const double stepPitch = errPitch / smooth;

	/* El soft aim tiene que poder cerrar todo el error en un frame, o la bala
	 * sale antes de llegar. Se le levanta el tope, pero sin quitarlo: sigue
	 * acotado al FOV en el que puede haber un objetivo. */
	const double stepCap = useSoft ? 180.0 : (double)g_Cfg.aim.maxStep;

	const uintptr_t pc = adresses.player_controller;
	bool applied = false;
	const char* how = "?";

	switch (g_Cfg.aim.method) {
	case AIM_GAME_FUNCTION:
		/* Camino preferido: alimentar RotationInput con las propias funciones
		 * del juego. Si la firma no valido (juego parcheado), se cae solo al
		 * metodo directo en vez de quedarse sin hacer nada. */
		applied = GameCalls::AddLookInput(pc, stepYaw, stepPitch, stepCap);
		how = "AddYawInput/AddPitchInput";
		if (!applied) {
			applied = GameCalls::AddLookInputDirect(pc, stepYaw, stepPitch, stepCap);
			how = "RotationInput (fallback)";
		}
		break;

	case AIM_ROTATION_INPUT:
		applied = GameCalls::AddLookInputDirect(pc, stepYaw, stepPitch, stepCap);
		how = "RotationInput";
		break;

	case AIM_CONTROL_ROTATION:
	default: {
		/* Metodo antiguo. Se conserva por si acaso, pero el juego recalcula
		 * ControlRotation cada tick a partir de RotationInput, asi que este
		 * camino pelea con el motor. */
		const FRotator sm = SmoothRotation(cur, want, (float)smooth);
		const bool okP = write<double>(pc + offset::control_rotation + 0x00, sm.Pitch);
		const bool okY = write<double>(pc + offset::control_rotation + 0x08, sm.Yaw);
		applied = okP && okY;
		how = "ControlRotation (legacy)";
		break;
	}
	}

	if (!applied)
		sprintf_s(g_AimLog, "Aimbot: apply failed via %s", how);
	else
		sprintf_s(g_AimLog, "Aimbot: on target | %.0f px | dYaw %.2f dPitch %.2f | %s",
		          bestDist, stepYaw, stepPitch, how);
}

/* -------------------------------- MENU --------------------------------- */

static const char* kSections[] = { "Players", "Aimbot", "Visuals", "Overlay", "Debug" };
static constexpr int kSectionCount = (int)(sizeof(kSections) / sizeof(kSections[0]));

static void RenderMenu() {
	ImGui::SetNextWindowSize(ImVec2(700, 460), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("NOVA", nullptr, ImGuiWindowFlags_NoCollapse)) { ImGui::End(); return; }

	/* Cabecera: buscador a la izquierda, interruptor maestro a la derecha. */
	ImGui::SetNextItemWidth(240.0f);
	ImGui::InputTextWithHint("##search", "Search options...", g_UiFilter, sizeof(g_UiFilter));
	{
		const bool on = g_Cfg.enabled;
		const float bw = 130.0f;
		/* Pegado al borde derecho de la ventana, siempre visible. */
		ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - bw);
		ImGui::PushStyleColor(ImGuiCol_Button, on ? ImVec4(0.10f, 0.55f, 0.25f, 1.0f) : ImVec4(0.45f, 0.12f, 0.12f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, on ? ImVec4(0.14f, 0.65f, 0.30f, 1.0f) : ImVec4(0.55f, 0.16f, 0.16f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, on ? ImVec4(0.08f, 0.45f, 0.20f, 1.0f) : ImVec4(0.35f, 0.10f, 0.10f, 1.0f));
		if (ImGui::Button(on ? "ESP: ON" : "ESP: OFF", ImVec2(bw, 0))) g_Cfg.enabled = !g_Cfg.enabled;
		ImGui::PopStyleColor(3);
	}
	ImGui::Separator();

	/* Barra lateral. */
	ImGui::BeginChild("##nav", ImVec2(140, 0), true);
	for (int i = 0; i < kSectionCount; ++i) {
		if (ImGui::Selectable(kSections[i], g_MenuSection == i, 0, ImVec2(0, 24)))
			g_MenuSection = i;
	}
	ImGui::EndChild();

	ImGui::SameLine();
	ImGui::BeginChild("##content", ImVec2(0, 0), true);

	g_UiShown = 0;

	if (g_MenuSection == 0) {          /* ---------------- Players -------- */
		UiGroup("Box");
		UiCombo("Bounding Box", &g_Cfg.players.boxMode, "None\0Full\0Corners\0",
		        "Shape drawn around each player.");
		UiToggle("Fit box to pose", &g_Cfg.players.boxFromBones,
		         "Builds the box from the actual bone positions, so it follows crouching\n"
		         "and prone. Falls back to the collision capsule when the pose is not\n"
		         "available or the player is half behind the camera.");
		UiSlider("Box Size", &g_Cfg.players.boxScale, 0.5f, 2.0f, "%.2fx",
		         "Multiplier applied to the box after it is computed.");

		UiGroup("Info");
		UiToggle("Name", &g_Cfg.players.name, "Player name, read from PlayerNamePrivate.");
		UiToggle("Health", &g_Cfg.players.health, "Health bar on the left plus the numeric value below.");
		UiToggle("Distance", &g_Cfg.players.distance, "Distance in meters from your camera.");
		UiToggle("Snapline", &g_Cfg.players.snapline, "Line from the bottom of the screen to the player.");

		UiGroup("Skeleton");
		UiToggle("Skeleton", &g_Cfg.players.skeleton,
		         "Draws one line per bone to its parent. The hierarchy comes from the\n"
		         "model's own reference skeleton, not from hardcoded indices.");
		UiToggle("Head Dot", &g_Cfg.players.headDot,
		         "Circle on the head bone. The head is identified from the reference\n"
		         "pose (the highest bone in T-pose), so it works on any model.");
		ImGui::BeginDisabled(!g_Cfg.players.headDot);
		UiSlider("Head Dot Size", &g_Cfg.players.headDotSize, 1.0f, 15.0f, "%.0f px");
		ImGui::EndDisabled();

		UiGroup("Filters");
		UiToggle("Show Enemies", &g_Cfg.players.showEnemy, nullptr);
		UiToggle("Show Teammates", &g_Cfg.players.showTeam,
		         "Teams come from PlayerState::TeamId. In free-for-all modes every\n"
		         "player may report the same team.");
		UiToggle("Show Drones", &g_Cfg.players.showDrones,
		         "When a player dies -- or deploys one -- they control a drone, so the\n"
		         "entity is still valid and does not disappear. Drones are detected by\n"
		         "class name and labelled [DRONE] on screen.");
		UiToggle("Hide Dead", &g_Cfg.players.hideDead,
		         "Skips players whose health is 0. Players whose health cannot be read\n"
		         "are still shown, so a broken offset does not hide everyone.");
		UiSlider("Max Distance", &g_Cfg.players.maxDistance, 10.0f, 1000.0f, "%.0f m");
	}
	else if (g_MenuSection == 1) {     /* ---------------- Aimbot --------- */
		UiGroup("Aim Assist");
		UiToggle("Enable Aimbot", &g_Cfg.aim.enabled, "Hold right mouse button to engage.");
		ImGui::BeginDisabled(!g_Cfg.aim.enabled);
		UiCombo("Target", &g_Cfg.aim.boneMode, "Head\0Body\0",
		        "Which bone to aim at. Head uses the bone identified from the\n"
		        "model's reference pose; Body aims at mid-height.");
		UiSlider("Aim FOV", &g_Cfg.aim.fov, 10.0f, 600.0f, "%.0f px",
		         "Radius around the crosshair where targets are considered.");
		UiSlider("Smoothing", &g_Cfg.aim.smooth, 1.0f, 20.0f, "%.1f",
		         "Higher is slower. 1 closes the whole gap in one frame.");
		ImGui::EndDisabled();

		/* Estos ajustes los comparten el aimbot y el soft aim, asi que basta con
		 * que uno de los dos este activo para poder tocarlos. */
		const bool anyAim = g_Cfg.aim.enabled || g_Cfg.aim.softAim;

		UiGroup("How the view is moved");
		ImGui::BeginDisabled(!anyAim);
		UiCombo("Method", &g_Cfg.aim.method,
		        "Game function (AddYawInput)\0Rotation input (direct)\0Control rotation (legacy)\0",
		        "Game function: calls the engine's own AddYawInput/AddPitchInput.\n"
		        "This feeds RotationInput, which is the input the game actually\n"
		        "expects, so the view moves through the normal path. Recommended.\n\n"
		        "Rotation input: writes the same field without calling anything.\n"
		        "Used automatically as a fallback if the function cannot be verified.\n\n"
		        "Control rotation: the old method. The game recomputes that value\n"
		        "every tick, so this one fights the engine.");
		UiSlider("Max Step", &g_Cfg.aim.maxStep, 1.0f, 90.0f, "%.0f deg",
		         "Hard cap on how many degrees the view may move in a single frame.\n"
		         "Safety net: a bad reading can never produce a wild spin.");
		UiToggle("Ignore Teammates", &g_Cfg.aim.ignoreTeam,
		         "Applies to both the aimbot and soft aim. Teams come from\n"
		         "PlayerState::TeamId, the same source the ESP uses.");
		UiToggle("Draw FOV Circle", &g_Cfg.aim.drawFov,
		         "White circle = aimbot FOV. Yellow circle = soft aim FOV.");
		UiToggle("Draw Target Line", &g_Cfg.aim.drawTarget,
		         "Line from the crosshair to the bone being tracked.");
		ImGui::EndDisabled();

		UiGroup("Soft Aim");
		UiToggle("Soft Aim", &g_Cfg.aim.softAim,
		         "Corrects your aim onto the head the moment you shoot, instead of\n"
		         "while holding a button. Works independently of the aimbot above.\n\n"
		         "IMPORTANT: this MOVES the view, for one frame, when you fire.\n"
		         "It does not redirect the bullet while your view stays put -- that\n"
		         "would require hooking the game's own shot trace, which is not\n"
		         "implemented. See the note in the Debug section.");
		ImGui::BeginDisabled(!g_Cfg.aim.softAim);
		UiSlider("Soft Aim FOV", &g_Cfg.aim.softFov, 10.0f, 600.0f, "%.0f px",
		         "Only corrects if the head is inside this radius from your crosshair.\n"
		         "Keep it tight: this is what makes it look like recoil, not teleporting.");
		UiSlider("Soft Aim Smoothing", &g_Cfg.aim.softSmooth, 1.0f, 10.0f, "%.1f",
		         "1 closes the whole gap on the first shot. Higher values need a few\n"
		         "shots to land, but move the view less each time.");
		UiToggle("Soft Aim Head Only", &g_Cfg.aim.softHeadOnly,
		         "Always aim at the head bone, ignoring the Target setting above.");
		ImGui::EndDisabled();

		ImGui::Dummy(ImVec2(0, 6));
		UiGroup("Engine function");
		ImGui::TextWrapped("%s", GameCalls::g_Calls.status);
		ImGui::Text("Verified: %s", GameCalls::g_Calls.ok ? "yes" : "no");
		ImGui::Text("Measured scale   yaw %.3f %s   pitch %.3f %s",
		            GameCalls::g_YawScale, GameCalls::g_YawCalibrated ? "" : "(default)",
		            GameCalls::g_PitchScale, GameCalls::g_PitchCalibrated ? "" : "(default)");
		ImGui::Dummy(ImVec2(0, 4));
		ImGui::TextWrapped("Status: %s", g_AimLog);
	}
	else if (g_MenuSection == 2) {     /* ---------------- Visuals -------- */
		UiGroup("Style");
		UiToggle("Outline", &Render::g_Style.outline,
		         "Draws everything twice: black and thicker first, then in color.\n"
		         "It is what keeps the ESP readable over bright backgrounds.");
		ImGui::BeginDisabled(!Render::g_Style.outline);
		UiSlider("Outline Width", &Render::g_Style.outlineExtra, 0.5f, 5.0f, "%.1f px");
		ImGui::EndDisabled();
		UiSlider("Line Thickness", &Render::g_Style.lineThickness, 0.5f, 5.0f, "%.1f px");
		UiSlider("Text Size", &Render::g_Style.textScale, 0.6f, 2.5f, "%.2fx",
		         "Scales the font itself, it does not stretch a bitmap.");

		UiGroup("Preview");
		{
			const float fs = Render::FontSize();
			const float boxH = 62.0f;
			/* La altura sale del tamano de fuente elegido: con el texto grande,
			 * un alto fijo dejaba las etiquetas fuera del recuadro. */
			const float previewH = boxH + fs * 4.0f + 18.0f;

			/* Se usa un child en vez de dibujar sobre la ventana.
			 *
			 * Con GetContentRegionAvail() el ancho que se obtenia era mayor que
			 * el area realmente visible, asi que el dibujo se iba a la derecha y
			 * el recorte lo cortaba: de ahi que la mitad clara ocupara casi todo
			 * y la caja quedara pegada al borde. Dentro de un child,
			 * GetWindowPos/GetWindowSize dan las coordenadas exactas del area, y
			 * ademas recorta el solo. */
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
			ImGui::BeginChild("##stylepreview", ImVec2(0, previewH), true,
			                  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

			ImDrawList* dl = ImGui::GetWindowDrawList();
			ImFont* f = ImGui::GetFont();
			const ImVec2 a = ImGui::GetWindowPos();
			const ImVec2 sz = ImGui::GetWindowSize();
			const float cx = a.x + sz.x * 0.5f;

			/* Fondo oscuro y liso, sin mas. */
			dl->AddRectFilled(a, ImVec2(a.x + sz.x, a.y + sz.y), IM_COL32(24, 24, 24, 255));

			const float top = a.y + fs * 2.0f + 6.0f;
			const float bw = boxH / 2.6f;                 /* misma proporcion que el ESP */
			const ImVec2 btl(cx - bw * 0.5f, top);
			const ImVec2 bbr(cx + bw * 0.5f, top + boxH);

			const float t = Render::g_Style.lineThickness;
			const ImU32 demoCol = IM_COL32(255, 60, 60, 255);
			if (Render::g_Style.outline)
				dl->AddRect(btl, bbr, Render::kOutline, 0, 0, t + Render::g_Style.outlineExtra);
			dl->AddRect(btl, bbr, demoCol, 0, 0, t);

			/* Barra de vida, igual que la del ESP. */
			{
				const float barW = 3.0f, bx = btl.x - barW - 3.0f;
				dl->AddRectFilled(ImVec2(bx - 1, btl.y - 1), ImVec2(bx + barW + 1, bbr.y + 1), Render::kOutline);
				const float fill = (bbr.y - btl.y) * 0.75f;
				dl->AddRectFilled(ImVec2(bx, bbr.y - fill), ImVec2(bx + barW, bbr.y),
				                  Render::HealthColor(75.0f));
			}

			auto drawTxt = [&](const char* s, float y, ImU32 c) {
				const float tw = f->CalcTextSizeA(fs, FLT_MAX, 0.0f, s).x;
				const ImVec2 tp(cx - tw * 0.5f, y);
				if (Render::g_Style.outline) {
					dl->AddText(f, fs, ImVec2(tp.x - 1, tp.y), Render::kOutline, s);
					dl->AddText(f, fs, ImVec2(tp.x + 1, tp.y), Render::kOutline, s);
					dl->AddText(f, fs, ImVec2(tp.x, tp.y - 1), Render::kOutline, s);
					dl->AddText(f, fs, ImVec2(tp.x, tp.y + 1), Render::kOutline, s);
				}
				dl->AddText(f, fs, tp, c, s);
			};
			drawTxt("Player", btl.y - fs - 2.0f, demoCol);
			drawTxt("75 HP", bbr.y + 2.0f, Render::HealthColor(75.0f));
			drawTxt("42 m", bbr.y + fs + 3.0f, IM_COL32(220, 220, 220, 255));

			ImGui::EndChild();
			ImGui::PopStyleVar();
		}
	}
	else if (g_MenuSection == 3) {     /* ---------------- Overlay -------- */
		UiGroup("Overlay");
		ImGui::Text("Toggle menu: INSERT");
		ImGui::Text("Unload:      DELETE");
		ImGui::Text("Overlay FPS: %.0f", ImGui::GetIO().Framerate);
		ImGui::Text("Client size: %.0f x %.0f", widthscreen, heightscreen);
		ImGui::Dummy(ImVec2(0, 6));
		ImGui::TextWrapped("The ESP draws on the background list, so this menu always stays on top of it.");
	}
	else if (g_MenuSection == 4) {     /* ---------------- Debug ---------- */
		UiGroup("World scan");
		if (!g_WorldScan.done) {
			ImGui::TextDisabled("Not scanned yet.");
		}
		else if (g_WorldScan.globalAddr) {
			ImGui::Text("Found GWorld global at 0x%llX", (unsigned long long)g_WorldScan.globalAddr);
			ImGui::Text("Module RVA  0x%llX",
			            (unsigned long long)(g_WorldScan.globalAddr -
			                (uintptr_t)GetModuleHandleA("Bodycam-Win64-Shipping.exe")));
			ImGui::Text("UWorld      0x%llX", (unsigned long long)g_WorldScan.worldPtr);
			ImGui::Text("Match       %s", g_WorldScan.strict ? "strict (full chain)" : "loose (no controller)");
		}
		else {
			ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "No world found.");
			ImGui::TextWrapped("Join a match and reopen this page. In the main menu there is "
			                   "no world to find yet.");
		}
		ImGui::Text("Pointers checked %d   scan took %u ms", g_WorldScan.candidates, g_WorldScan.scanMs);

		UiGroup("Chain");
		ImGui::Text("Stage:      %s", ChainStageName(adresses.stage));
		ImGui::Text("GWorld addr 0x%llX  ->  UWorld 0x%llX", (unsigned long long)Uworld, (unsigned long long)adresses.uworld);
		ImGui::Text("GameState   0x%llX", (unsigned long long)adresses.game_state);
		ImGui::Text("PlayerArray 0x%llX  count=%d", (unsigned long long)adresses.player_array, adresses.player_count);
		ImGui::Text("LocalPawn   0x%llX  team=%d", (unsigned long long)adresses.acknowledged_pawn, adresses.local_team);

		UiGroup("Camera / projection");
		if (g_View.valid) {
			ImGui::Text("Loc  %.0f  %.0f  %.0f", g_View.Location.x, g_View.Location.y, g_View.Location.z);
			ImGui::Text("Rot  P=%.1f  Y=%.1f  R=%.1f", g_View.Rotation.x, g_View.Rotation.y, g_View.Rotation.z);
			ImGui::Text("FOV  %.1f    AspectRatio %.3f    Constrain %d",
			            g_View.FOV, g_View.AspectRatio, (int)g_View.ConstrainAspect);
			float xM = 1, yM = 1;
			ProjectionMultipliers(g_View, widthscreen, heightscreen, xM, yM);
			static const char* axisName[3] = { "MaintainYFOV (vertical)", "MaintainXFOV (horizontal)", "MajorAxisFOV" };
			ImGui::Text("Axis %u  %s   ->  xMult %.3f  yMult %.3f",
			            (unsigned)g_View.AxisConstraint,
			            g_View.AxisConstraint < 3 ? axisName[g_View.AxisConstraint] : "?", xM, yM);
		}
		else ImGui::TextDisabled("Camera not resolved.");

		/* El combo usa 0..3; g_AxisOverride usa -1 para "auto". */
		int axisSel = g_AxisOverride + 1;
		if (UiCombo("FOV Axis", &axisSel,
		            "Auto (read from game)\0Force Y-FOV\0Force X-FOV\0Force MajorAxis\0",
		            "Defaults to Force X-FOV, which is what matches this game.\n\n"
		            "Auto reads AspectRatioAxisConstraint from the game, but here it\n"
		            "reports Y-FOV while the real projection behaves as X-FOV, so the\n"
		            "measured value is used instead.\n\n"
		            "Symptom of a wrong axis: boxes drift OUTWARD from the center,\n"
		            "more so the closer they are to the screen edges."))
			g_AxisOverride = axisSel - 1;
		UiSlider("FOV Scale", &g_FovScale, 0.5f, 2.5f, "%.3f",
		         "Fine correction for the projection scale. Default 1.150, the value\n"
		         "calibrated in game for Bodycam.\n\n"
		         "Symptom that it needs adjusting: the closer an enemy is to the edge\n"
		         "of the screen, the more his box drifts away from him. If boxes drift\n"
		         "OUTWARD raise it; if they fall short toward the center, lower it.\n\n"
		         "1.0 = use the FOV exactly as the game reports it.");
		UiSlider("FOV Fallback", &g_FallbackFOV, kFovMin, kFovMax, "%.0f",
		         "Used only if both camera caches fail.");

		UiGroup("Names (GNames)");
		ImGui::Text("Status: %s", Names::g_Status);
		{
			/* Prueba en vivo: si aqui sale el nombre real de tu clase de pawn,
			 * GNames y los offsets de UObject son correctos. */
			char cn[128] = "";
			if (adresses.acknowledged_pawn &&
			    Names::GetClassName(adresses.acknowledged_pawn, cn, sizeof(cn)) && cn[0])
				ImGui::Text("Local pawn class: %s", cn);
			else
				ImGui::TextDisabled("Local pawn class: (unavailable)");
		}

		UiGroup("ESP counters");
		ImGui::Text("roster %d   drawn %d", g_Diag.total, g_Diag.drawn);
		ImGui::Text("noPawn %d  self %d  team %d  dead %d", g_Diag.noPawn, g_Diag.self, g_Diag.teamFiltered, g_Diag.dead);
		ImGui::Text("noHealth %d  noPos %d  tooFar %d  offScreen %d",
		            g_Diag.noHealth, g_Diag.noPosition, g_Diag.tooFar, g_Diag.offScreen);
		ImGui::Text("drones %d  droneFiltered %d", g_Diag.drones, g_Diag.droneFiltered);

		UiGroup("Bone counters");
		ImGui::Text("skeletons drawn %d", g_BoneDiag.drawn);
		ImGui::Text("noMesh %d  noPose %d  noAsset %d  noHierarchy %d",
		            g_BoneDiag.noMesh, g_BoneDiag.noPose, g_BoneDiag.noAsset, g_BoneDiag.noHierarchy);
		ImGui::Text("RefSkeleton offset in asset: %s",
		            g_RefSkelOffset >= 0 ? "found" : "not found yet");
		if (g_RefSkelOffset >= 0) { ImGui::SameLine(); ImGui::Text("(0x%X)", g_RefSkelOffset); }
		ImGui::Text("cached skeletons: %d", (int)g_SkelCache.size());
		{
			/* Cuantos huesos pasan el filtro por nombre. Si `named` es no, el
			 * esqueleto se dibuja en modo de respaldo (por longitud). */
			int named = 0, core = 0, total = 0;
			for (const auto& kv : g_SkelCache) {
				if (!kv.second.valid) continue;
				++total;
				if (kv.second.named) ++named;
				core += kv.second.coreCount;
			}
			ImGui::Text("named %d/%d   body bones (sum) %d", named, total, core);
		}
	}

	/* Aviso si el buscador no encuentra nada, salvo en las secciones que son
	 * solo lectura y no tienen opciones filtrables. */
	if (g_UiFilter[0] && g_UiShown == 0 && g_MenuSection != 3 && g_MenuSection != 4)
		ImGui::TextDisabled("No options match \"%s\".", g_UiFilter);

	ImGui::EndChild();
	ImGui::End();
}

/* ------------------------------ FRAME ---------------------------------- */

void renderImGui() {
	/* Red de seguridad: sin dispositivo o sin ventana no hay nada que dibujar,
	 * y seguir seria desreferenciar un nulo dentro del proceso del juego. */
	if (!pDevice || !overlayWindow) return;

	if (!isInitialized) {
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui_ImplWin32_Init(overlayWindow);
		ImGui_ImplDX9_Init(pDevice);
		ImGui_ImplDX9_CreateDeviceObjects();
		Render::ApplyTheme();
		/* Verifica la firma de AddYawInput/AddPitchInput una sola vez. Si no
		 * cuadra, el aimbot usa la escritura directa y lo dice en su seccion. */
		GameCalls::Resolve();
		Names::Init();          /* FNamePool: nombres de hueso y de clase */
		isInitialized = true;
	}

	if (GetAsyncKeyState(VK_INSERT) & 1) {
		isMenuVisible = !isMenuVisible;
		ImGui::GetIO().MouseDrawCursor = isMenuVisible;
	}

	ImGui_ImplDX9_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	/* Tamano real del cliente: la proyeccion depende de esto. */
	const ImGuiIO& io = ImGui::GetIO();
	if (io.DisplaySize.x > 0 && io.DisplaySize.y > 0) {
		widthscreen = io.DisplaySize.x;
		heightscreen = io.DisplaySize.y;
	}

	SetWindowLong(overlayWindow, GWL_EXSTYLE,
	              isMenuVisible ? WS_EX_TOOLWINDOW : (WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW));

	RenderESP();

	/* Con el menu abierto no se apunta. Si no, cada clic en un control activaria
	 * el soft aim (que se dispara con el boton izquierdo) y movería la vista
	 * mientras el usuario esta tocando ajustes. */
	if (!isMenuVisible) RunAimbot();
	else strcpy_s(g_AimLog, "Paused (menu open)");

	const ImVec2 screenCenter(widthscreen * 0.5f, heightscreen * 0.5f);
	if (g_Cfg.aim.enabled && g_Cfg.aim.drawFov)
		Render::Circle(screenCenter, g_Cfg.aim.fov, IM_COL32(255, 255, 255, 110));
	/* El del soft aim va en otro color para poder distinguirlos de un vistazo. */
	if (g_Cfg.aim.softAim && g_Cfg.aim.drawFov)
		Render::Circle(screenCenter, g_Cfg.aim.softFov, IM_COL32(255, 200, 0, 110));

	if ((g_Cfg.aim.enabled || g_Cfg.aim.softAim) && g_Cfg.aim.drawTarget && g_AimHasTarget) {
		fvector2d t;
		if (W2S(g_AimTargetWorld, t))
			Render::Line(ImVec2(widthscreen * 0.5f, heightscreen * 0.5f),
			             ImVec2((float)t.x, (float)t.y), IM_COL32(255, 220, 0, 200));
	}

	if (isMenuVisible) {
		DrawBackgroundAnimation();
		inputHandler();
		RenderMenu();
		SetFocus(overlayWindow);
	}

	ImGui::EndFrame();

	pDevice->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0);
	if (pDevice->BeginScene() >= 0) {
		ImGui::Render();
		if (ImGui::GetDrawData())
			ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
		pDevice->EndScene();
	}

	if (pDevice->Present(NULL, NULL, NULL, NULL) == D3DERR_DEVICELOST &&
	    pDevice->TestCooperativeLevel() == D3DERR_DEVICENOTRESET) {
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

		if (GetForegroundWindow() == targetWindow)
			SetWindowPos(overlayWindow, GetWindow(GetForegroundWindow(), GW_HWNDPREV), 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

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
			SetWindowPos(overlayWindow, (HWND)0, windowPoint.x, windowPoint.y,
			             windowInfo->Width, windowInfo->Height, SWP_NOREDRAW);
			/* Reset invalida los objetos de ImGui: hay que soltarlos y recrearlos,
			 * o la siguiente pasada dibuja con recursos muertos. */
			if (pDevice) {
				if (isInitialized) ImGui_ImplDX9_InvalidateDeviceObjects();
				pDevice->Reset(&gD3DPresentParams);
				if (isInitialized) ImGui_ImplDX9_CreateDeviceObjects();
			}
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
	case WM_DESTROY: PostQuitMessage(0); break;
	case WM_CLOSE:   TerminateProcess(GetCurrentProcess(), 0); break;
	}
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

void inputHandler() {
	ImGuiIO& io = ImGui::GetIO();
	io.MouseDown[0] = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
	for (int i = 1; i < 5; i++) io.MouseDown[i] = false;
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

	overlayWindow = CreateWindowEx(NULL, ovarlayName.c_str(), ovarlayName.c_str(), WS_POPUP | WS_VISIBLE,
	                               windowInfo->Left, windowInfo->Top, windowInfo->Width, windowInfo->Height,
	                               NULL, NULL, windowClass.hInstance, NULL);
	if (!overlayWindow) return FALSE;
	DwmExtendFrameIntoClientArea(overlayWindow, &margins);
	SetWindowLong(overlayWindow, GWL_EXSTYLE, WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW);
	ShowWindow(overlayWindow, SW_SHOW);
	UpdateWindow(overlayWindow);
	return TRUE;
}

bool createDirectX() {
	if (FAILED(Direct3DCreate9Ex(D3D_SDK_VERSION, &pDirect))) return FALSE;

	ZeroMemory(&gD3DPresentParams, sizeof(gD3DPresentParams));
	gD3DPresentParams.Windowed = TRUE;
	gD3DPresentParams.SwapEffect = D3DSWAPEFFECT_DISCARD;
	gD3DPresentParams.BackBufferFormat = D3DFMT_A8R8G8B8;
	gD3DPresentParams.PresentationInterval = D3DPRESENT_INTERVAL_ONE;

	if (pDirect->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, overlayWindow,
	                            D3DCREATE_HARDWARE_VERTEXPROCESSING, &gD3DPresentParams, 0, &pDevice) != D3D_OK)
		return FALSE;
	return TRUE;
}

static bool InitOnThread();

DWORD WINAPI MainThread(HMODULE hMod) {
	/* Todo lo que antes se hacia en DllMain (asignaciones, user32) se hace aqui,
	 * ya fuera del loader lock. */
	if (!InitOnThread()) FreeLibraryAndExitThread(hMod, 0);

	/* nothrow: una excepcion escapando de un hilo de una DLL inyectada mata el
	 * proceso del juego, no solo al overlay. */
	windowInfo = new (std::nothrow) WindowInfo();
	if (!windowInfo) FreeLibraryAndExitThread(hMod, 0);
	ZeroMemory(windowInfo, sizeof(WindowInfo));

	bool WindowFocus = false;
	while (WindowFocus == false) {
		DWORD ForegroundWindowProcessID = 0;
		GetWindowThreadProcessId(GetForegroundWindow(), &ForegroundWindowProcessID);
		if (getProcessID(targetProcessName) == ForegroundWindowProcessID && ForegroundWindowProcessID != 0) {
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
		Sleep(50);
	}

	/* Sin estas comprobaciones, si createDirectX fallaba (sistema sin D3D9,
	 * driver ocupado, sin recursos) pDevice quedaba nulo y el primer frame hacia
	 * pDevice->Clear(...) sobre un nulo: crash del JUEGO, no solo del overlay.
	 * Ante un fallo se descarga la DLL sin tocar nada mas. */
	if (createOverlay() && createDirectX() && pDevice)
		mainLoop();

	/* Shutdown solo si se llego a inicializar: llamar a los Shutdown de ImGui
	 * sin su Init previo es comportamiento indefinido. */
	if (isInitialized) {
		ImGui_ImplDX9_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
		isInitialized = false;
	}

	clearVariable(pDevice);
	clearVariable(pDirect);
	if (overlayWindow) {
		DestroyWindow(overlayWindow);
		UnregisterClass(windowClass.lpszClassName, windowClass.hInstance);
		overlayWindow = nullptr;
	}
	delete windowInfo;
	windowInfo = nullptr;
	delete hooks;
	hooks = nullptr;

	FreeLibraryAndExitThread(hMod, 0);
}

BOOL __stdcall StartThread(LPTHREAD_START_ROUTINE startaddr, HMODULE hMod) {
	HANDLE h = CreateThread(0, 0, startaddr, hMod, 0, 0);
	if (!h) return FALSE;
	return CloseHandle(h);
}

/* Inicializacion real. Va en el hilo, NO en DllMain.
 *
 * DllMain corre con el loader lock del proceso tomado. Reservar memoria, llamar
 * a user32 o construir std::string ahi puede bloquear el proceso entero del
 * juego si otro hilo esta cargando modulos a la vez. La regla es que DllMain
 * solo cree el hilo y devuelva. */
static bool InitOnThread() {
	std::srand((unsigned)std::time(nullptr));
	ovarlayName = generateRandomString(generateRandomInt(30, 100));
	if (ovarlayName.empty()) return false;

	if (!hooks) {
		hooks = new (std::nothrow) hookclass();
		if (!hooks) return false;
	}
	hooks->GetAddreses();

	widthscreen = (float)GetSystemMetrics(SM_CXSCREEN);
	heightscreen = (float)GetSystemMetrics(SM_CYSCREEN);
	return widthscreen > 0.0f && heightscreen > 0.0f;
}

BOOL APIENTRY DllMain(HMODULE hmodule, DWORD dwreason, LPVOID lpreserved) {
	UNREFERENCED_PARAMETER(lpreserved);
	switch (dwreason) {
	case DLL_PROCESS_ATTACH:
		DisableThreadLibraryCalls(hmodule);
		/* Lo unico que se hace bajo el loader lock. */
		if (!StartThread((LPTHREAD_START_ROUTINE)MainThread, hmodule))
			return FALSE;
		break;
	default:
		break;
	}
	return TRUE;
}
