#ifndef TPJ_RENDER_MATH_H
#define TPJ_RENDER_MATH_H

// Minimal vector and matrix math for the camera and GPU uniforms. Render-side only; the sim
// chooses its own math when it needs some, with determinism in mind (principle 10).

namespace tpj {

struct Vec3 {
  float X = 0.0f;
  float Y = 0.0f;
  float Z = 0.0f;
};

inline Vec3 operator+(Vec3 a, Vec3 b) { return {a.X + b.X, a.Y + b.Y, a.Z + b.Z}; }
inline Vec3 operator-(Vec3 a, Vec3 b) { return {a.X - b.X, a.Y - b.Y, a.Z - b.Z}; }
inline Vec3 operator*(Vec3 v, float s) { return {v.X * s, v.Y * s, v.Z * s}; }

float dot(Vec3 a, Vec3 b);
Vec3 cross(Vec3 a, Vec3 b);
Vec3 normalize(Vec3 v);

// Column-major, matching GLSL: element (row, col) is M[col * 4 + row].
struct Mat4 {
  float M[16] = {};
};

Mat4 multiply(const Mat4 &a, const Mat4 &b);

// Right-handed view space looking down -Z; clip depth in [0, 1] as SDL_GPU expects, reversed so
// the near plane maps to 1 and the far plane to 0, which spreads a float depth buffer's precision
// evenly across distance.
Mat4 perspective(float fovY, float aspect, float nearZ, float farZ);
Mat4 lookAt(Vec3 eye, Vec3 target, Vec3 up);

} // namespace tpj

#endif
