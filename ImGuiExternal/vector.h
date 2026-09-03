#pragma once
#include "include.h"
#include <corecrt_math_defines.h>

struct Vector2 {
	float x, y;

	Vector2 operator+(Vector2 V) {
		return { V.x + x, V.y + y };
	}

	Vector2 operator-(Vector2 V) {
		return { V.x - x, V.y - y };
	}

	Vector2 operator*(Vector2 V) {
		return { V.x * x, V.y * y };
	}

	Vector2 operator/(Vector2 V) {
		return { V.x / x, V.y / y };
	}

	float distance(Vector2 V) {
				return (float)(sqrt(pow(V.x - x, 2) + pow(V.y - y, 2)));
	}
};

struct Vector3 {
	float x, y, z;

	Vector3() : x(0.0f), y(0.0f), z(0.0f) {}

	Vector3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}

	Vector3 operator+(Vector3 V) {
		return { V.x + x, V.y + y, V.z + z };
	}

	Vector3 operator-(Vector3 V) {
		return { V.x - x, V.y - y, V.z - z };
	}

	Vector3 operator*(Vector3 V) {
		return { V.x * x, V.y * y, V.z * z };
	}

	Vector3 operator/(Vector3 V) {
		return { V.x / x, V.y / y, V.z / z };
	}

	float dot(Vector3 V) {
		return x * V.x + y * V.y + z * V.z;
	}

	float distance(Vector3 V) {
				return (float)(sqrt(pow(V.x - x, 2) + pow(V.y - y, 2) + pow(V.z - z, 2)));
	}
};

struct Vector4 {
	float x, y, z, w;

	Vector4 operator+(Vector4 V) {
		return { V.x + x, V.y + y, V.z + z, V.w + w };
	}

	Vector4 operator-(Vector4 V) {
		return { V.x - x, V.y - y, V.z - z, V.w - w };
	}

	Vector4 operator*(Vector4 V) {
		return { V.x * x, V.y * y, V.z * z, V.w * w };
	}

	Vector4 operator/(Vector4 V) {
		return { V.x / x, V.y / y, V.z / z, V.w / w };
	}

	float distance(Vector4 V) {
				return (float)(sqrt(pow(V.x - x, 2) + pow(V.y - y, 2) + pow(V.z - z, 2) + pow(V.w - w, 2)));
	}
};

class fvector2d
{
public:
	fvector2d() : x(0.f), y(0.f)
	{

	}

	fvector2d(double _x, double _y) : x(_x), y(_y)
	{

	}
	~fvector2d()
	{

	}

	fvector2d operator-(fvector2d v)
	{
		return fvector2d(x - v.x, y - v.y);
	}

	double x;
	double y;

};

class fvector
{
public:
	fvector() : x(0.f), y(0.f), z(0.f)
	{

	}

	fvector(double _x, double _y, double _z) : x(_x), y(_y), z(_z)
	{

	}
	~fvector()
	{

	}

	double x,y,z;

	inline double dot(const fvector& v) const
	{
		return x * v.x + y * v.y + z * v.z;
	}

	inline double distance(fvector v) const
	{
		const double dx = v.x - x, dy = v.y - y, dz = v.z - z;
		return sqrt(dx * dx + dy * dy + dz * dz);
	}

	inline double length() const {
		return sqrt(x * x + y * y + z * z);
	}

	fvector operator+(const fvector& v) const
	{
		return fvector(x + v.x, y + v.y, z + v.z);
	}

	fvector operator-(const fvector& v) const
	{
		return fvector(x - v.x, y - v.y, z - v.z);
	}

	fvector operator/(double flNum) const
	{
		return fvector(x / flNum, y / flNum, z / flNum);
	}

	fvector operator*(double flNum) const { return fvector(x * flNum, y * flNum, z * flNum); }

};
struct fquat
{
	double x;
	double y;
	double z;
	double w;
};

struct FMinimalViewInfo
{
	fvector Location;
	fvector Rotation;
	float FOV;
};

struct FTransform
{
	fquat rot;
	fvector translation;
	char pad[4];
	fvector scale;
	char pad1[4];

	D3DMATRIX ToMatrixWithScale() const
	{
		D3DMATRIX m;
		m._41 = (float)translation.x;
		m._42 = (float)translation.y;
		m._43 = (float)translation.z;

		const double x2 = rot.x + rot.x;
		const double y2 = rot.y + rot.y;
		const double z2 = rot.z + rot.z;

		const double xx2 = rot.x * x2;
		const double yy2 = rot.y * y2;
		const double zz2 = rot.z * z2;
		m._11 = (float)((1.0 - (yy2 + zz2)) * scale.x);
		m._22 = (float)((1.0 - (xx2 + zz2)) * scale.y);
		m._33 = (float)((1.0 - (xx2 + yy2)) * scale.z);

		const double yz2 = rot.y * z2;
		const double wx2 = rot.w * x2;
		m._32 = (float)((yz2 - wx2) * scale.z);
		m._23 = (float)((yz2 + wx2) * scale.y);

		const double xy2 = rot.x * y2;
		const double wz2 = rot.w * z2;
		m._21 = (float)((xy2 - wz2) * scale.y);
		m._12 = (float)((xy2 + wz2) * scale.x);

		const double xz2 = rot.x * z2;
		const double wy2 = rot.w * y2;
		m._31 = (float)((xz2 + wy2) * scale.z);
		m._13 = (float)((xz2 - wy2) * scale.x);

		m._14 = 0.0f;
		m._24 = 0.0f;
		m._34 = 0.0f;
		m._44 = 1.0f;

		return m;
	}

	fvector TransformPosition(const fvector& p) const
	{
		const double x2 = rot.x + rot.x, y2 = rot.y + rot.y, z2 = rot.z + rot.z;
		const double xx2 = rot.x * x2, yy2 = rot.y * y2, zz2 = rot.z * z2;
		const double xy2 = rot.x * y2, xz2 = rot.x * z2, yz2 = rot.y * z2;
		const double wx2 = rot.w * x2, wy2 = rot.w * y2, wz2 = rot.w * z2;

		const double sx = p.x * scale.x, sy = p.y * scale.y, sz = p.z * scale.z;

		return fvector(
			sx * (1.0 - (yy2 + zz2)) + sy * (xy2 - wz2) + sz * (xz2 + wy2) + translation.x,
			sx * (xy2 + wz2) + sy * (1.0 - (xx2 + zz2)) + sz * (yz2 - wx2) + translation.y,
			sx * (xz2 - wy2) + sy * (yz2 + wx2) + sz * (1.0 - (xx2 + yy2)) + translation.z);
	}
};

static_assert(sizeof(FTransform) == 0x60, "FTransform must be 0x60 bytes (UE5 LWC)");
static_assert(offsetof(FTransform, translation) == 0x20, "Translation must be at 0x20");
static_assert(offsetof(FTransform, scale) == 0x40, "Scale3D must be at 0x40");
static_assert(sizeof(fvector) == 24, "UE5 FVector is three doubles");
inline D3DMATRIX MatrixMultiplication(D3DMATRIX pM1, D3DMATRIX pM2)
{

		D3DMATRIX pOut;
	pOut._11 = pM1._11 * pM2._11 + pM1._12 * pM2._21 + pM1._13 * pM2._31 + pM1._14 * pM2._41;
	pOut._12 = pM1._11 * pM2._12 + pM1._12 * pM2._22 + pM1._13 * pM2._32 + pM1._14 * pM2._42;
	pOut._13 = pM1._11 * pM2._13 + pM1._12 * pM2._23 + pM1._13 * pM2._33 + pM1._14 * pM2._43;
	pOut._14 = pM1._11 * pM2._14 + pM1._12 * pM2._24 + pM1._13 * pM2._34 + pM1._14 * pM2._44;
	pOut._21 = pM1._21 * pM2._11 + pM1._22 * pM2._21 + pM1._23 * pM2._31 + pM1._24 * pM2._41;
	pOut._22 = pM1._21 * pM2._12 + pM1._22 * pM2._22 + pM1._23 * pM2._32 + pM1._24 * pM2._42;
	pOut._23 = pM1._21 * pM2._13 + pM1._22 * pM2._23 + pM1._23 * pM2._33 + pM1._24 * pM2._43;
	pOut._24 = pM1._21 * pM2._14 + pM1._22 * pM2._24 + pM1._23 * pM2._34 + pM1._24 * pM2._44;
	pOut._31 = pM1._31 * pM2._11 + pM1._32 * pM2._21 + pM1._33 * pM2._31 + pM1._34 * pM2._41;
	pOut._32 = pM1._31 * pM2._12 + pM1._32 * pM2._22 + pM1._33 * pM2._32 + pM1._34 * pM2._42;
	pOut._33 = pM1._31 * pM2._13 + pM1._32 * pM2._23 + pM1._33 * pM2._33 + pM1._34 * pM2._43;
	pOut._34 = pM1._31 * pM2._14 + pM1._32 * pM2._24 + pM1._33 * pM2._34 + pM1._34 * pM2._44;
	pOut._41 = pM1._41 * pM2._11 + pM1._42 * pM2._21 + pM1._43 * pM2._31 + pM1._44 * pM2._41;
	pOut._42 = pM1._41 * pM2._12 + pM1._42 * pM2._22 + pM1._43 * pM2._32 + pM1._44 * pM2._42;
	pOut._43 = pM1._41 * pM2._13 + pM1._42 * pM2._23 + pM1._43 * pM2._33 + pM1._44 * pM2._43;
	pOut._44 = pM1._41 * pM2._14 + pM1._42 * pM2._24 + pM1._43 * pM2._34 + pM1._44 * pM2._44;

	return pOut;
}
#define PI 3.14159265358979323846f

inline D3DXMATRIX Matrix(fvector rot, fvector origin = fvector(0, 0, 0))
{
	float radPitch = (float)(rot.x * M_PI / 180.0);
	float radYaw = (float)(rot.y * M_PI / 180.0);
	float radRoll = (float)(rot.z * M_PI / 180.0);

	float SP = sinf(radPitch);
	float CP = cosf(radPitch);
	float SY = sinf(radYaw);
	float CY = cosf(radYaw);
	float SR = sinf(radRoll);
	float CR = cosf(radRoll);

	D3DMATRIX matrix;
	matrix.m[0][0] = CP * CY;
	matrix.m[0][1] = CP * SY;
	matrix.m[0][2] = SP;
	matrix.m[0][3] = 0.f;

	matrix.m[1][0] = SR * SP * CY - CR * SY;
	matrix.m[1][1] = SR * SP * SY + CR * CY;
	matrix.m[1][2] = -SR * CP;
	matrix.m[1][3] = 0.f;

	matrix.m[2][0] = -(CR * SP * CY + SR * SY);
	matrix.m[2][1] = CY * SR - CR * SP * SY;
	matrix.m[2][2] = CR * CP;
	matrix.m[2][3] = 0.f;

	matrix.m[3][0] = (float)origin.x;
	matrix.m[3][1] = (float)origin.y;
	matrix.m[3][2] = (float)origin.z;
	matrix.m[3][3] = 1.f;

	return matrix;
}

struct FRotator {
	double Pitch;
	double Yaw;
	double Roll;
};

inline FRotator CalcAngle(fvector src, fvector dst) {
	fvector delta = dst - src;
	double hyp = sqrt(delta.x * delta.x + delta.y * delta.y);

	FRotator angle;
	angle.Pitch = atan2(delta.z, hyp) * (180.0 / M_PI);
	angle.Yaw = atan2(delta.y, delta.x) * (180.0 / M_PI);
	angle.Roll = 0.0;
	return angle;
}

inline double NormalizeAngle(double angle) {
	while (angle > 180.0) angle -= 360.0;
	while (angle < -180.0) angle += 360.0;
	return angle;
}

inline FRotator SmoothRotation(FRotator current, FRotator target, float smoothing) {
	FRotator result;
	result.Pitch = current.Pitch + NormalizeAngle(target.Pitch - current.Pitch) / smoothing;
	result.Yaw = current.Yaw + NormalizeAngle(target.Yaw - current.Yaw) / smoothing;
	result.Roll = 0.0;
	return result;
}

inline double GetCrosshairDistance(fvector2d screenPos, float screenW, float screenH) {
	double cx = screenW / 2.0;
	double cy = screenH / 2.0;
	double dx = screenPos.x - cx;
	double dy = screenPos.y - cy;
	return sqrt(dx * dx + dy * dy);
}
