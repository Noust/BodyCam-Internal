#pragma once
#include "include.h"
#include <vector>
#include <unordered_map>
#include <algorithm>

/* ---------------------------------------------------------------------------
 *  Camara, proyeccion y huesos.
 *
 *  Los tres bugs que arregla este archivo respecto a la version anterior:
 *    1. La proyeccion usaba width/2 en AMBOS ejes. El eje que conserva el FOV
 *       lo decide ULocalPlayer::AspectRatioAxisConstraint@0xB8. Con el eje mal,
 *       las cajas se desplazan hacia AFUERA del centro (factor W/H = 1.78).
 *    2. Lo que estaba detras de la camara se recortaba con "if (z<1) z=1" en
 *       vez de rechazarse, asi que los enemigos de detras se dibujaban delante
 *       y en espejo.
 *    3. Los indices de hueso estaban a fuego (head = 48...). Ahora la jerarquia
 *       se resuelve leyendo el FReferenceSkeleton del propio asset.
 * ------------------------------------------------------------------------- */

static char g_boneDebug[256] = "waiting...";

/* ============================== CAMARA ================================== */

struct CameraView {
	fvector  Location;
	fvector  Rotation;          /* x = Pitch, y = Yaw, z = Roll */
	float    FOV = 90.0f;
	float    AspectRatio = 0.0f;
	bool     ConstrainAspect = false;
	uint8_t  AxisConstraint = 0;   /* EAspectAxis */
	bool     valid = false;
};
inline CameraView g_View;

/* -1 = automatico (leer del juego). 0/1/2 = forzar un eje concreto.
 *
 * Se arranca FORZANDO X-FOV (1) porque es lo que encaja en este juego, medido en
 * partida: el AspectRatioAxisConstraint@0xB8 del juego devuelve 0 (MaintainYFOV)
 * pero la proyeccion real se comporta como MaintainXFOV. Se deja el control en
 * el menu por si en algun modo o resolucion conviniera cambiarlo. */
inline int g_AxisOverride = AspectAxis_MaintainXFOV;

/* FOV de emergencia si los dos caches de camara fallan. */
inline float g_FallbackFOV = 90.0f;

/* Corrección fina de la escala de proyección.
 *
 * El FOV que publica FMinimalViewInfo no siempre coincide con el que el juego
 * usa de verdad para construir la matriz: puede haber un factor propio del
 * título (escalado de FOV por opciones, render a otra proporción, etc). El
 * síntoma es inconfundible: cuanto más cerca del borde de la pantalla está el
 * enemigo, más se desplaza su caja.
 *
 * Valor calibrado en partida para Bodycam: 1.150. (En Hell Let Loose: Vietnam
 * el que cuadraba era 1.330.) Se deja ajustable desde el menu por si cambia con
 * la resolucion o el FOV configurado en el juego. */
inline float g_FovScale = 1.150f;

inline constexpr float kFovMin = 20.0f;
inline constexpr float kFovMax = 170.0f;

/* Lee el POV del cache actual; si no es plausible, prueba el del frame
 * anterior; si tampoco, cae al FOV manual. Tres niveles, porque durante los
 * cambios de camara (muerte, respawn) el cache actual puede venir a cero. */
inline bool ReadCamera(CameraView& out) {
	out = CameraView{};
	const uintptr_t cm = adresses.camera_manager;
	if (!IsValidPtr(cm)) return false;

	const uintptr_t povs[2] = { cm + offset::pov_info, cm + offset::pov_info_last };

	for (int tier = 0; tier < 2; ++tier) {
		fvector loc{}, rot{};
		float fov = 0.0f;
		if (!readRaw<fvector>(povs[tier] + offset::mvi_location, loc)) continue;
		if (!readRaw<fvector>(povs[tier] + offset::mvi_rotation, rot)) continue;
		if (!read<float>(povs[tier] + offset::mvi_fov, fov)) continue;
		if (!(fov == fov) || fov < kFovMin || fov > kFovMax) continue;   /* NaN o absurdo */
		if (loc.x == 0.0 && loc.y == 0.0 && loc.z == 0.0) continue;

		out.Location = loc;
		out.Rotation = rot;
		out.FOV = fov;
		read<float>(povs[tier] + offset::mvi_aspect_ratio, out.AspectRatio);
		uint32_t flags = 0;
		read<uint32_t>(povs[tier] + offset::mvi_flags, flags);
		out.ConstrainAspect = (flags & 0x01) != 0;
		out.AxisConstraint = adresses.aspect_axis;
		out.valid = true;
		return true;
	}

	/* Ultimo recurso: posicion del cache actual + FOV manual. */
	fvector loc{}, rot{};
	if (readRaw<fvector>(povs[0] + offset::mvi_location, loc) &&
		readRaw<fvector>(povs[0] + offset::mvi_rotation, rot)) {
		out.Location = loc;
		out.Rotation = rot;
		out.FOV = g_FallbackFOV;
		out.AxisConstraint = adresses.aspect_axis;
		out.valid = true;
		return true;
	}
	return false;
}

/* Replica de CalculateProjectionMatrixGivenViewRectangle: decide que eje
 * conserva el FOV. Ver REVERSE_ENGINEERING.md §5. */
inline void ProjectionMultipliers(const CameraView& v, float W, float H,
                                  float& xMult, float& yMult) {
	xMult = 1.0f; yMult = 1.0f;
	if (W <= 0.0f || H <= 0.0f) return;

	const bool forced = (g_AxisOverride >= 0 && g_AxisOverride <= 2);

	/* Con aspecto restringido el juego usa el AspectRatio del POV. Pero si el
	 * usuario ha forzado un eje a mano, manda el suyo: "forzar" tiene que
	 * significar forzar, sin excepciones que lo salten por detras. */
	if (!forced && v.ConstrainAspect && v.AspectRatio > 0.01f && v.AspectRatio < 100.0f) {
		xMult = 1.0f; yMult = v.AspectRatio;
		return;
	}
	int axis = v.AxisConstraint;
	if (forced) axis = g_AxisOverride;

	if ((W > H && axis == AspectAxis_MajorAxisFOV) || axis == AspectAxis_MaintainXFOV) {
		xMult = 1.0f;   yMult = W / H;
	}
	else {
		xMult = H / W;  yMult = 1.0f;
	}
}

/* Proyecta un punto del mundo a pantalla.
 * Devuelve false si el punto NO es visible (detras de la camara o pantalla
 * invalida). El llamante DEBE comprobar el retorno: recortar en vez de
 * rechazar es lo que dibujaba a los de detras como si estuvieran delante. */
inline bool W2S(const fvector& world, fvector2d& out) {
	out = fvector2d(0, 0);
	if (!g_View.valid) return false;

	const float W = widthscreen, H = heightscreen;
	if (W <= 0.0f || H <= 0.0f) return false;

	const D3DXMATRIX m = Matrix(g_View.Rotation);
	const fvector fwd(m.m[0][0], m.m[0][1], m.m[0][2]);
	const fvector right(m.m[1][0], m.m[1][1], m.m[1][2]);
	const fvector up(m.m[2][0], m.m[2][1], m.m[2][2]);

	fvector d = world - g_View.Location;

	const double depth = d.dot(fwd);
	if (depth <= 1.0) return false;          /* detras de la camara */

	double tanHalf = tan((double)g_View.FOV * M_PI / 360.0);
	if (tanHalf <= 1e-6) return false;
	/* g_FovScale corrige el desajuste entre el FOV publicado y el que el juego
	 * usa realmente. Divide el tangente, asi que un valor > 1 "abre" la
	 * proyeccion y acerca las cajas al centro. */
	if (g_FovScale > 0.05f) tanHalf /= (double)g_FovScale;

	float xMult, yMult;
	ProjectionMultipliers(g_View, W, H, xMult, yMult);

	const double sx = (W * 0.5) + d.dot(right) / depth * (xMult / tanHalf) * (W * 0.5);
	const double sy = (H * 0.5) - d.dot(up) / depth * (yMult / tanHalf) * (H * 0.5);

	if (!(sx == sx) || !(sy == sy)) return false;   /* NaN */
	out = fvector2d(sx, sy);
	return true;
}

inline bool OnScreen(const fvector2d& p) {
	return p.x >= 0.0 && p.y >= 0.0 && p.x <= (double)widthscreen && p.y <= (double)heightscreen;
}

/* ============================== HUESOS ================================== */

inline constexpr int kMaxDrawBones = 256;

/* Jerarquia del esqueleto, cacheada por asset (es inmutable). */
/* Un modelo de UE trae del orden de 150 huesos: dedos, twists, huesos IK,
 * correctivos y puntos de anclaje de armas. Dibujarlos todos produce una maraña
 * de líneas (y las de los huesos de arma salen disparadas lejos del cuerpo).
 *
 * Con GNames resuelto se puede filtrar por NOMBRE y quedarse solo con el
 * esqueleto del cuerpo: ~20 líneas limpias. `drawParent` guarda, para cada
 * hueso que se dibuja, el ancestro dibujable más cercano, de forma que la
 * cadena no se rompe aunque por medio haya huesos descartados. */
struct SkeletonInfo {
	std::vector<int32_t> parents;
	std::vector<uint8_t> keep;        /* 1 = forma parte del cuerpo        */
	std::vector<int32_t> drawParent;  /* ancestro dibujable, -1 si ninguno */
	int  head = -1;
	int  coreCount = 0;               /* huesos que se dibujan             */
	int  nameTries = 0;               /* intentos de resolver los nombres  */
	bool named = false;               /* se pudieron leer los nombres      */
	bool valid = false;
};
/* Reconstruir el filtro cuesta leer ~100 nombres. Si un modelo simplemente no
 * da nombres utiles, hay que dejar de intentarlo o se repetiria cada frame y
 * para cada jugador. */
inline constexpr int kMaxNameTries = 3;
inline std::unordered_map<uintptr_t, SkeletonInfo> g_SkelCache;

/* Offset del TArray<FMeshBoneInfo> dentro del USkinnedAsset. No esta
 * reflexionado, asi que se localiza una vez por escaneo validado y se recuerda. */
inline int g_RefSkelOffset = -1;

/* Contadores de diagnostico: sin desglosar las causas, un "el esqueleto no se
 * dibuja" es imposible de arreglar. */
struct BoneDiag {
	int noMesh = 0, noPose = 0, noAsset = 0, noHierarchy = 0, drawn = 0;
	void reset() { noMesh = noPose = noAsset = noHierarchy = drawn = 0; }
};
inline BoneDiag g_BoneDiag;

/* Multiplicacion de cuaterniones, para componer la pose de referencia. */
inline fquat QuatMul(const fquat& a, const fquat& b) {
	fquat r;
	r.x = a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y;
	r.y = a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x;
	r.z = a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w;
	r.w = a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z;
	return r;
}

/* Valida que arrAddr sea un TArray<FMeshBoneInfo> de verdad.
 * Los invariantes de un FReferenceSkeleton no los cumple ningun dato basura:
 *   - ParentIndex[0] == -1 (la raiz no tiene padre)
 *   - 0 <= ParentIndex[i] < i  para todo i > 0 (el padre siempre va antes)
 * Con num > 8 la probabilidad de un falso positivo es despreciable. */
inline bool ValidateBoneInfo(uintptr_t arrAddr, std::vector<int32_t>& parents) {
	parents.clear();
	uintptr_t data = 0;
	int num = 0;
	if (!readPtr(arrAddr, data)) return false;
	if (!read<int>(arrAddr + 0x08, num)) return false;
	if (num <= 8 || num > limits::kMaxBones) return false;

	std::vector<uint8_t> buf(static_cast<size_t>(num) * offset::mesh_bone_info_stride);
	if (!readBytes(data, buf.data(), buf.size())) return false;

	parents.resize(num);
	for (int i = 0; i < num; ++i) {
		int32_t p = 0;
		memcpy(&p, buf.data() + static_cast<size_t>(i) * offset::mesh_bone_info_stride
		                      + offset::mesh_bone_info_parent, sizeof(int32_t));
		if (i == 0) { if (p != -1) return false; }
		else if (p < 0 || p >= i) return false;
		parents[i] = p;
	}
	return true;
}

/* Localiza el array de FMeshBoneInfo dentro del asset.
 * Primero prueba el offset ya conocido (caso normal, coste casi cero) y solo
 * escanea si falla. El escaneo cubre todo el USkeletalMesh (sizeof 0x528).
 *
 * El escaneo completo son ~165 validaciones, cada una con sus lecturas. Si el
 * offset no se encuentra nunca (asset aun sin cargar, o layout distinto), se
 * repetiria por cada jugador y cada frame, y eso se nota en los FPS. Por eso el
 * escaneo va limitado a uno cada 500 ms mientras no haya exito. En cuanto
 * acierta una vez, g_RefSkelOffset queda fijado y no se vuelve a escanear. */
inline bool FindBoneInfoArray(uintptr_t asset, std::vector<int32_t>& parents, int& foundOff) {
	if (g_RefSkelOffset >= 0 &&
		ValidateBoneInfo(asset + g_RefSkelOffset, parents)) {
		foundOff = g_RefSkelOffset;
		return true;
	}

	static ULONGLONG s_lastScan = 0;
	const ULONGLONG now = GetTickCount64();
	if (g_RefSkelOffset < 0 && s_lastScan != 0 && now - s_lastScan < 500) return false;
	s_lastScan = now;

	for (int off = 0; off <= 0x520; off += 8) {
		if (ValidateBoneInfo(asset + off, parents)) {
			g_RefSkelOffset = off;
			foundOff = off;
			return true;
		}
	}
	return false;
}

/* Identifica la cabeza sin necesitar GNames.
 * En la pose de REFERENCIA (T-pose) la cabeza es siempre el hueso mas alto.
 * Se compone la jerarquia completa una vez por asset y se toma el de mayor Z.
 * Mirar la pose actual no serviria: cambia al agacharse o tumbarse.
 * boneInfoArr + 0x10 es la pose, tanto si el array hallado es RawRefBoneInfo
 * (0x00 -> RawRefBonePose 0x10) como FinalRefBoneInfo (0x20 -> FinalRefBonePose 0x30). */
inline int FindHeadBone(uintptr_t boneInfoArr, const std::vector<int32_t>& parents) {
	const uintptr_t poseArr = boneInfoArr + 0x10;
	uintptr_t data = 0;
	int num = 0;
	if (!readPtr(poseArr, data)) return -1;
	if (!read<int>(poseArr + 0x08, num)) return -1;
	if (num <= 0 || num > limits::kMaxBones) return -1;

	const int n = std::min<int>(num, (int)parents.size());
	if (n <= 1) return -1;

	std::vector<FTransform> local(n);
	if (!readBytes(data, local.data(), static_cast<size_t>(n) * sizeof(FTransform))) return -1;

	std::vector<FTransform> world(n);
	for (int i = 0; i < n; ++i) {
		const int p = parents[i];
		if (i == 0 || p < 0 || p >= i) {
			world[i] = local[i];
		}
		else {
			world[i].translation = world[p].TransformPosition(local[i].translation);
			world[i].rot = QuatMul(world[p].rot, local[i].rot);
			world[i].scale = fvector(world[p].scale.x * local[i].scale.x,
			                         world[p].scale.y * local[i].scale.y,
			                         world[p].scale.z * local[i].scale.z);
		}
	}

	int best = -1;
	double bestZ = -1e300;
	for (int i = 0; i < n; ++i) {
		if (world[i].translation.z > bestZ) { bestZ = world[i].translation.z; best = i; }
	}
	return best;
}

/* Decide si un hueso forma parte del esqueleto que queremos dibujar.
 *
 * Primero se descarta lo que ensucia (dedos, twists, IK, anclajes de arma) y
 * luego se exige que el nombre contenga alguno de los tokens del cuerpo. Se
 * cubren las dos nomenclaturas habituales: la del Mannequin de UE
 * (upperarm_l, calf_r, clavicle_l...) y la genérica (shoulder, elbow, knee...). */
inline bool IsCoreBone(const char* n) {
	if (!n || !*n) return false;

	/* Lo que sobra. `root` se descarta del dibujo (esta en el suelo) pero sigue
	 * sirviendo de ancestro en la cadena. */
	static const char* kSkip[] = {
		"twist", "index", "middle", "ring", "pinky", "thumb", "finger",
		"ik_", "weapon", "attach", "socket", "prop", "camera", "root", "armor",
		"corrective", "adjust", "helper", "aim", "offset", "clothing",
		"cloth", "physics", "vb ", "interaction", "center_of_mass"
	};
	for (const char* s : kSkip) if (Names::ContainsCI(n, s)) return false;

	static const char* kCore[] = {
		"pelvis", "spine", "neck", "head", "clavicle",
		"upperarm", "lowerarm", "hand", "thigh", "calf", "foot", "ball", "toe",
		"shoulder", "elbow", "wrist", "hip", "knee", "ankle", "chest", "leg", "arm"
	};
	for (const char* s : kCore) if (Names::ContainsCI(n, s)) return true;
	return false;
}

/* Lee los nombres del RefSkeleton y construye el filtro + la cadena de dibujo.
 * Si GNames no esta disponible o ningun nombre encaja, se marca `named=false`
 * y el dibujado cae a un modo de respaldo (ver DrawSkeleton). */
inline void BuildBoneFilter(uintptr_t boneInfoArr, SkeletonInfo& info) {
	const int n = (int)info.parents.size();
	info.keep.assign(n, 0);
	info.drawParent.assign(n, -1);
	info.coreCount = 0;
	info.named = false;
	if (n <= 0) return;

	uintptr_t data = 0;
	int num = 0;
	if (!readPtr(boneInfoArr, data)) return;
	if (!read<int>(boneInfoArr + 0x08, num)) return;
	if (num <= 0) return;
	const int cnt = (num < n) ? num : n;

	int named = 0, headByName = -1;
	char name[128];
	for (int i = 0; i < cnt; ++i) {
		/* FMeshBoneInfo: FName Name @0x00 (stride 0x0C) — verificado por ASM. */
		const uintptr_t infoAddr = data + (uintptr_t)i * offset::mesh_bone_info_stride;
		if (!Names::ReadFName(infoAddr, name, sizeof(name))) continue;
		++named;
		if (IsCoreBone(name)) { info.keep[i] = 1; ++info.coreCount; }
		/* El hueso llamado exactamente "head" es la cabeza, sin heuristicas. */
		if (headByName < 0 && _stricmp(name, "head") == 0) headByName = i;
	}

	/* Si no se pudo leer practicamente ningun nombre, el filtro no vale. */
	if (named < cnt / 2 || info.coreCount < 4) {
		info.keep.assign(n, 0);
		info.drawParent.assign(n, -1);
		info.coreCount = 0;
		info.named = false;
		return;
	}
	info.named = true;
	if (headByName >= 0) info.head = headByName;

	/* Ancestro dibujable mas cercano, para no dejar huecos en la cadena. */
	for (int i = 0; i < n; ++i) {
		if (!info.keep[i]) continue;
		int p = info.parents[i];
		int guard = 0;
		while (p >= 0 && p < n && !info.keep[p] && ++guard < limits::kMaxParents)
			p = info.parents[p];
		info.drawParent[i] = (p >= 0 && p < n && info.keep[p]) ? p : -1;
	}
}

/* Devuelve la jerarquia del esqueleto de un mesh, cacheada por asset.
 * NO se cachea el fallo: si un frame no resuelve (asset aun sin cargar), hay
 * que poder reintentar al siguiente. */
inline const SkeletonInfo* GetSkeleton(uintptr_t meshComp) {
	uintptr_t asset = 0;
	if (!readPtr(meshComp + offset::skinned_asset, asset))
		readPtr(meshComp + offset::skeletal_mesh_asset_old, asset);
	if (!IsValidPtr(asset)) { g_BoneDiag.noAsset++; return nullptr; }

	auto it = g_SkelCache.find(asset);
	if (it != g_SkelCache.end() && it->second.valid) {
		/* Si el filtro por nombre no se pudo construir porque GNames aun no
		 * estaba listo, se reintenta ahora que si lo esta. Sin esto, el primer
		 * pawn de la partida condenaba el esqueleto al modo de respaldo para
		 * el resto de la sesion. */
		if (!it->second.named && Names::g_Ready && it->second.nameTries < kMaxNameTries) {
			++it->second.nameTries;
			int off2 = -1;
			std::vector<int32_t> tmp;
			if (FindBoneInfoArray(asset, tmp, off2))
				BuildBoneFilter(asset + off2, it->second);
		}
		return &it->second;
	}

	SkeletonInfo info;
	int off = -1;
	if (!FindBoneInfoArray(asset, info.parents, off)) { g_BoneDiag.noHierarchy++; return nullptr; }
	info.head = FindHeadBone(asset + off, info.parents);
	BuildBoneFilter(asset + off, info);
	info.valid = true;

	/* Evita que el mapa crezca sin limite en sesiones largas. */
	if (g_SkelCache.size() > 64) g_SkelCache.clear();
	auto res = g_SkelCache.emplace(asset, std::move(info));
	return &res.first->second;
}

/* Pose actual de un pawn, en coordenadas de mundo. */
struct PawnPose {
	bool    ok = false;
	int     count = 0;
	fvector world[kMaxDrawBones];
};

/* Resuelve el mesh del que hay que leer los huesos. Si el pawn usa
 * LeaderPoseComponent, la pose vive en el componente lider, no en el suyo.
 * Ojo: la pose se lee del lider pero ComponentToWorld sigue siendo el del
 * mesh propio. */
inline uintptr_t ResolveBoneMesh(uintptr_t meshComp) {
	uintptr_t leader = 0;
	if (readPtr(meshComp + offset::leader_pose_component, leader)) return leader;
	return meshComp;
}

/* Lee toda la pose de una vez (una sola lectura para los N huesos) en lugar de
 * una lectura por hueso, que es lo que hacia la version anterior. */
inline bool ReadPawnPose(uintptr_t pawn, PawnPose& out) {
	out.ok = false;
	out.count = 0;

	uintptr_t meshComp = 0;
	if (!readPtr(pawn + offset::skeletal_mesh_component, meshComp)) {
		g_BoneDiag.noMesh++;
		return false;
	}
	const uintptr_t boneMesh = ResolveBoneMesh(meshComp);

	int bufIdx = 0;
	read<int>(boneMesh + offset::bone_buffer_index, bufIdx);
	if (bufIdx < 0 || bufIdx > 1) bufIdx = 0;

	uintptr_t data = 0;
	int num = 0;
	bool got = false;
	/* Doble buffer: si el indice activo no da nada, probar el otro. */
	for (int attempt = 0; attempt < 2 && !got; ++attempt) {
		const int b = (bufIdx + attempt) & 1;
		const uintptr_t arr = boneMesh + offset::active_bone_array + (uintptr_t)b * offset::bone_array_stride;
		uintptr_t d = 0; int n = 0;
		if (readPtr(arr, d) && read<int>(arr + 0x08, n) && n > 0 && n <= limits::kMaxBones) {
			data = d; num = n; got = true;
		}
	}
	if (!got) { g_BoneDiag.noPose++; return false; }

	const int n = std::min(num, kMaxDrawBones);
	static FTransform s_bones[kMaxDrawBones];
	if (!readBytes(data, s_bones, static_cast<size_t>(n) * sizeof(FTransform))) {
		g_BoneDiag.noPose++;
		return false;
	}

	FTransform c2w{};
	if (!readRaw<FTransform>(meshComp + offset::component_to_world, c2w)) {
		g_BoneDiag.noPose++;
		return false;
	}

	for (int i = 0; i < n; ++i)
		out.world[i] = c2w.TransformPosition(s_bones[i].translation);

	out.count = n;
	out.ok = true;
	return true;
}

/* Jerarquia del esqueleto a partir del pawn.
 * Importante: se resuelve sobre el mesh del que sale la POSE (el lider si lo
 * hay), no sobre el mesh propio; si no, la jerarquia no corresponderia con los
 * huesos que se estan leyendo. */
inline const SkeletonInfo* GetSkeletonForPawn(uintptr_t pawn) {
	uintptr_t meshComp = 0;
	if (!readPtr(pawn + offset::skeletal_mesh_component, meshComp)) return nullptr;
	return GetSkeleton(ResolveBoneMesh(meshComp));
}

/* Nota: no hay un getter "de un solo hueso" a proposito. Leer un hueso suelto
 * obligaria a releer la pose entera del pawn, asi que tanto el ESP como el
 * aimbot llaman a ReadPawnPose una vez por jugador y usan el indice que
 * necesiten sobre el resultado. */
