#pragma once
// common/quat.h — Quaternion / Vector3 math (same semantics as baseline).
#include <cmath>

struct Vec3 { float x, y, z; };

struct Quat { float x, y, z, w; };

static inline Quat QuatIdentity() { return {0, 0, 0, 1}; }

static inline Quat QuatMul(Quat a, Quat b) {
  return {
      a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
      a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
      a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
      a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
}

static inline Quat QuatInv(Quat q) { return {-q.x, -q.y, -q.z, q.w}; }

static inline float QuatDot(Quat a, Quat b) {
  return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

static inline Quat QuatSlerp(Quat a, Quat b, float t) {
  float dot = QuatDot(a, b);
  if (dot < 0.0f) {
    b.x = -b.x; b.y = -b.y; b.z = -b.z; b.w = -b.w;
    dot = -dot;
  }
  if (dot > 0.9995f) {
    Quat r = {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t,
              a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t};
    float len = sqrtf(r.x * r.x + r.y * r.y + r.z * r.z + r.w * r.w);
    if (len > 0.0001f) { r.x /= len; r.y /= len; r.z /= len; r.w /= len; }
    return r;
  }
  float theta = acosf(dot);
  float sinTheta = sinf(theta);
  float wa = sinf((1.0f - t) * theta) / sinTheta;
  float wb = sinf(t * theta) / sinTheta;
  return {wa * a.x + wb * b.x, wa * a.y + wb * b.y,
          wa * a.z + wb * b.z, wa * a.w + wb * b.w};
}

// Rotate k times the half-angle about the same axis (k=1 identity, k=2 double).
static inline Quat QuatPowK(Quat q, float k) {
  float n = sqrtf(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
  if (n < 1e-6f) return QuatIdentity();
  q.x /= n; q.y /= n; q.z /= n; q.w /= n;
  float vn = sqrtf(q.x * q.x + q.y * q.y + q.z * q.z);
  if (vn < 1e-6f) return QuatIdentity();
  float ang = atan2f(vn, q.w) * k;
  float s = sinf(ang) / vn;
  return {q.x * s, q.y * s, q.z * s, cosf(ang)};
}

// Axis-angle delta quaternion (angle in radians, axis 0/1/2 = X/Y/Z).
static inline Quat QuatAxisAngle(int axis, float angleRad) {
  float s = sinf(angleRad * 0.5f), c = cosf(angleRad * 0.5f);
  Quat q = {0, 0, 0, c};
  if (axis == 0) q.x = s;
  else if (axis == 1) q.y = s;
  else q.z = s;
  return q;
}

static inline float DegToRad(float deg) { return deg * 0.0174532925f; }
static inline float RadToDeg(float rad) { return rad * 57.2957795f; }
