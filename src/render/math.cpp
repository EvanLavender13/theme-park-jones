#include "render/math.h"

#include <math.h>

namespace tpj {

float dot(Vec3 a, Vec3 b) { return a.X * b.X + a.Y * b.Y + a.Z * b.Z; }

Vec3 cross(Vec3 a, Vec3 b) {
  return {a.Y * b.Z - a.Z * b.Y, a.Z * b.X - a.X * b.Z, a.X * b.Y - a.Y * b.X};
}

Vec3 normalize(Vec3 v) {
  const float length = sqrtf(dot(v, v));
  return length > 0.0f ? v * (1.0f / length) : v;
}

Mat4 multiply(const Mat4 &a, const Mat4 &b) {
  Mat4 result;
  for (int col = 0; col < 4; ++col) {
    for (int row = 0; row < 4; ++row) {
      float sum = 0.0f;
      for (int k = 0; k < 4; ++k) {
        sum += a.M[k * 4 + row] * b.M[col * 4 + k];
      }
      result.M[col * 4 + row] = sum;
    }
  }
  return result;
}

Mat4 perspective(float fovY, float aspect, float nearZ, float farZ) {
  const float f = 1.0f / tanf(fovY * 0.5f);
  Mat4 result;
  result.M[0] = f / aspect;
  result.M[5] = f;
  result.M[10] = nearZ / (farZ - nearZ);
  result.M[11] = -1.0f;
  result.M[14] = nearZ * farZ / (farZ - nearZ);
  return result;
}

Mat4 lookAt(Vec3 eye, Vec3 target, Vec3 up) {
  const Vec3 forward = normalize(target - eye);
  const Vec3 side = normalize(cross(forward, up));
  const Vec3 camUp = cross(side, forward);
  Mat4 result;
  result.M[0] = side.X;
  result.M[4] = side.Y;
  result.M[8] = side.Z;
  result.M[1] = camUp.X;
  result.M[5] = camUp.Y;
  result.M[9] = camUp.Z;
  result.M[2] = -forward.X;
  result.M[6] = -forward.Y;
  result.M[10] = -forward.Z;
  result.M[12] = -dot(side, eye);
  result.M[13] = -dot(camUp, eye);
  result.M[14] = dot(forward, eye);
  result.M[15] = 1.0f;
  return result;
}

} // namespace tpj
