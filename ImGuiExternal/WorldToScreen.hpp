#pragma once
#include "include.h"
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cmath>

static char g_boneDebug[256] = "waiting...";

struct CameraView {
	fvector  Location;
	fvector  Rotation;
	float    FOV = 90.0f;
	float    AspectRatio = 0.0f;
	bool     ConstrainAspect = false;
	uint8_t  AxisConstraint = 0;
	bool     valid = false;
};
inline CameraView g_View;

inline int g_AxisOverride = AspectAxis_MaintainXFOV;

inline float g_FallbackFOV = 90.0f;

inline float g_FovScale = 1.150f;

inline constexpr float kFovMin = 20.0f;
inline constexpr float kFovMax = 170.0f;

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
		if (!(fov == fov) || fov < kFovMin || fov > kFovMax) continue;
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

inline void ProjectionMultipliers(const CameraView& v, float W, float H,
                                  float& xMult, float& yMult) {
	xMult = 1.0f; yMult = 1.0f;
	if (W <= 0.0f || H <= 0.0f) return;

	const bool forced = (g_AxisOverride >= 0 && g_AxisOverride <= 2);

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
	if (depth <= 1.0) return false;

	double tanHalf = tan((double)g_View.FOV * M_PI / 360.0);
	if (tanHalf <= 1e-6) return false;
	if (g_FovScale > 0.05f) tanHalf /= (double)g_FovScale;

	float xMult, yMult;
	ProjectionMultipliers(g_View, W, H, xMult, yMult);

	const double sx = (W * 0.5) + d.dot(right) / depth * (xMult / tanHalf) * (W * 0.5);
	const double sy = (H * 0.5) - d.dot(up) / depth * (yMult / tanHalf) * (H * 0.5);

	if (!(sx == sx) || !(sy == sy)) return false;
	out = fvector2d(sx, sy);
	return true;
}

inline bool OnScreen(const fvector2d& p) {
	return p.x >= 0.0 && p.y >= 0.0 && p.x <= (double)widthscreen && p.y <= (double)heightscreen;
}

inline constexpr int kMaxDrawBones = 256;

struct SkeletonInfo {
	std::vector<int32_t> parents;
	std::vector<uint8_t> keep;
	std::vector<int32_t> drawParent;
	int  head = -1;
	int  coreCount = 0;
	int  nameTries = 0;
	bool named = false;
	bool valid = false;
};
inline constexpr int kMaxNameTries = 3;
inline std::unordered_map<uintptr_t, SkeletonInfo> g_SkelCache;

inline int g_RefSkelOffset = -1;

struct BoneDiag {
	int noMesh = 0, noPose = 0, noAsset = 0, noHierarchy = 0, badMesh = 0, drawn = 0;
	void reset() { noMesh = noPose = noAsset = noHierarchy = badMesh = drawn = 0; }
};
inline BoneDiag g_BoneDiag;

inline constexpr double kMaxMeshOffsetCm = 500.0;
inline constexpr double kMaxWorldCoordCm = 1.0e7;

inline bool TransformLooksSane(const FTransform& t) {
	const double qn = t.rot.x * t.rot.x + t.rot.y * t.rot.y + t.rot.z * t.rot.z + t.rot.w * t.rot.w;
	if (!(qn == qn) || qn < 0.5 || qn > 2.0) return false;

	const double sx = fabs(t.scale.x), sy = fabs(t.scale.y), sz = fabs(t.scale.z);
	if (!(sx == sx) || !(sy == sy) || !(sz == sz)) return false;
	if (sx < 1e-4 || sy < 1e-4 || sz < 1e-4) return false;
	if (sx > 100.0 || sy > 100.0 || sz > 100.0) return false;

	const double tx = t.translation.x, ty = t.translation.y, tz = t.translation.z;
	if (!(tx == tx) || !(ty == ty) || !(tz == tz)) return false;
	if (fabs(tx) > kMaxWorldCoordCm || fabs(ty) > kMaxWorldCoordCm || fabs(tz) > kMaxWorldCoordCm) return false;
	return true;
}

inline bool MeshBelongsToPawn(uintptr_t pawn, const FTransform& c2w) {
	uintptr_t root = 0;
	if (!readPtr(pawn + offset::root_component, root)) return true;
	fvector rootPos{};
	if (!readRaw<fvector>(root + offset::c2w_translation, rootPos)) return true;
	if (!(rootPos.x == rootPos.x) || !(rootPos.y == rootPos.y) || !(rootPos.z == rootPos.z)) return true;

	const double dx = c2w.translation.x - rootPos.x;
	const double dy = c2w.translation.y - rootPos.y;
	const double dz = c2w.translation.z - rootPos.z;
	const double d2 = dx * dx + dy * dy + dz * dz;
	if (!(d2 == d2)) return false;
	return d2 <= kMaxMeshOffsetCm * kMaxMeshOffsetCm;
}

inline fquat QuatMul(const fquat& a, const fquat& b) {
	fquat r;
	r.x = a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y;
	r.y = a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x;
	r.z = a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w;
	r.w = a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z;
	return r;
}

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

inline bool IsCoreBone(const char* n) {
	if (!n || !*n) return false;

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
		const uintptr_t infoAddr = data + (uintptr_t)i * offset::mesh_bone_info_stride;
		if (!Names::ReadFName(infoAddr, name, sizeof(name))) continue;
		++named;
		if (IsCoreBone(name)) { info.keep[i] = 1; ++info.coreCount; }
		if (headByName < 0 && _stricmp(name, "head") == 0) headByName = i;
	}

	if (named < cnt / 2 || info.coreCount < 4) {
		info.keep.assign(n, 0);
		info.drawParent.assign(n, -1);
		info.coreCount = 0;
		info.named = false;
		return;
	}
	info.named = true;
	if (headByName >= 0) info.head = headByName;

	for (int i = 0; i < n; ++i) {
		if (!info.keep[i]) continue;
		int p = info.parents[i];
		int guard = 0;
		while (p >= 0 && p < n && !info.keep[p] && ++guard < limits::kMaxParents)
			p = info.parents[p];
		info.drawParent[i] = (p >= 0 && p < n && info.keep[p]) ? p : -1;
	}
}

inline const SkeletonInfo* GetSkeleton(uintptr_t meshComp) {
	uintptr_t asset = 0;
	if (!readPtr(meshComp + offset::skinned_asset, asset))
		readPtr(meshComp + offset::skeletal_mesh_asset_old, asset);
	if (!IsValidPtr(asset)) { g_BoneDiag.noAsset++; return nullptr; }

	auto it = g_SkelCache.find(asset);
	if (it != g_SkelCache.end() && it->second.valid) {
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

	if (g_SkelCache.size() > 64) g_SkelCache.clear();
	auto res = g_SkelCache.emplace(asset, std::move(info));
	return &res.first->second;
}

struct PawnPose {
	bool    ok = false;
	int     count = 0;
	fvector world[kMaxDrawBones];
};

inline uintptr_t ResolveBoneMesh(uintptr_t meshComp) {
	uintptr_t leader = 0;
	if (readPtr(meshComp + offset::leader_pose_component, leader)) return leader;
	return meshComp;
}

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

	if (!TransformLooksSane(c2w) || !MeshBelongsToPawn(pawn, c2w)) {
		g_BoneDiag.badMesh++;
		return false;
	}

	for (int i = 0; i < n; ++i)
		out.world[i] = c2w.TransformPosition(s_bones[i].translation);

	out.count = n;
	out.ok = true;
	return true;
}

inline const SkeletonInfo* GetSkeletonForPawn(uintptr_t pawn) {
	uintptr_t meshComp = 0;
	if (!readPtr(pawn + offset::skeletal_mesh_component, meshComp)) return nullptr;
	return GetSkeleton(ResolveBoneMesh(meshComp));
}
