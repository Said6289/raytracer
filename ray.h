#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <float.h>

#define u8  uint8_t
#define u16 uint16_t
#define u32 uint32_t
#define u64 uint64_t

#define f32 float

#define ArrayCount(arr) (sizeof(arr)/sizeof(arr[0]))

typedef struct {
  f32 x;
  f32 y;
} v2;

typedef struct {
  f32 x;
  f32 y;
  f32 z;
} v3;

typedef struct {
    f32 r;
    f32 g;
    f32 b;
} color;

typedef struct {
    v3 Origin;
    v3 Direction;
} ray;

typedef struct {
    v3 Origin;
    v3 Direction;
} raycaster;

typedef struct {
} camera;

typedef struct {
} mat3;

typedef struct {
    v3 vertex0;
    v3 vertex1;
    v3 vertex2;
} triangle;

typedef struct {
    v3 Color;
    bool Mirror;
    f32 Shininess;
    f32 Reflectivity;
} material;

typedef struct {
  v3 Center;
  f32 Radius;
  material *Material;
} sphere;

typedef struct {
  f32 D;
  v3 Normal;
  material *Material;
} plane;

typedef struct {
    uint32_t SphereCount;
    sphere *Spheres;

    uint32_t PlaneCount;
    plane *Planes;
} scene;

typedef struct {
} mesh;

typedef struct {
} face;

typedef struct {
    material *Material;
    mesh *Mesh;
    face Face;
    v3 Point;
    v3 Normal;
} hit;

typedef struct {
    v3 V0;
    v3 V1;
    v3 V2;

    triangle Triangle;
    v3 BaryP;

    v3 n0;
    v3 n1;
    v3 n2;

    mat3 normalMatrix;
} regs;

typedef struct {
    uint32_t *Pixels;
    uint32_t Width;
    uint32_t Height;
} image_data;

typedef struct {
    v3 Position;
    v3 Color;
    float Intensity;
} light;

static float
Clamp(float A, float V, float B) {
    if (V < A) return A;
    if (V > B) return B;
    return V;
}

static v3 V3(f32 x, f32 y, f32 z) {
  v3 result = {x, y, z};
  return result;
}

static v2 V2(f32 x, f32 y) {
  v2 Result = {x, y};
  return Result;
}

static f32 dot(v3 a, v3 b) {
  f32 result = a.x * b.x + a.y * b.y + a.z * b.z;
  return result;
}

static v3 cross(v3 a, v3 b) {
  v3 result;
  result.x = a.y*b.z - a.z*b.y;
  result.y = a.z*b.x - a.x*b.z;
  result.z = a.x*b.y - a.y*b.x;
  return result;
}

static v3 hadamard(v3 a, v3 b) {
  v3 result;
  result.x = a.x * b.x;
  result.y = a.y * b.y;
  result.z = a.z * b.z;
  return result;
}

static v3 scale(v3 v, f32 s) {
  v3 result;
  result.x = v.x * s;
  result.y = v.y * s;
  result.z = v.z * s;
  return result;
}

static v3 add(v3 a, v3 b) {
  v3 result;
  result.x = a.x + b.x;
  result.y = a.y + b.y;
  result.z = a.z + b.z;
  return result;
}

static v3 subtract(v3 a, v3 b) {
  v3 result;
  result.x = a.x - b.x;
  result.y = a.y - b.y;
  result.z = a.z - b.z;
  return result;
}

static f32 length(v3 v) {
  f32 result = dot(v, v);
  result = sqrt(result);
  return result;
}

static v3 normalize(v3 v) {
  f32 length_v = length(v);
  v3 result = v;
  result.x /= length_v;
  result.y /= length_v;
  result.z /= length_v;
  return result;
}

static u32 color_to_u32(v3 color) {
  f32 r = color.x > 1.0f ? 1.0f : color.x;
  f32 g = color.y > 1.0f ? 1.0f : color.y;
  f32 b = color.z > 1.0f ? 1.0f : color.z;

  u32 result =
      (((u32) (r * 255.0f) & 0xFF) << 16)
    | (((u32) (g * 255.0f) & 0xFF) << 8)
    | ((u32)  (b * 255.0f) & 0xFF);
  return result;
}
