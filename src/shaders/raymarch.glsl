#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// Sphere tracing over an SDF, plus the shading terms that make the result look
// like a rendering rather than a depth buffer.
//
// You provide the scene :
//
//   #include <sdf.glsl>
//   #include <raymarch.glsl>
//
//   float sceneSDF(vec3 p) {          // the prototype below is already declared
//       return opSmoothUnion(sdSphere(p, 1.0), sdPlane(p, vec3(0,1,0), 1.0), 0.3);
//   }
//
//   void main() {
//       vec3 ro, rd;
//       orbitRay(ro, rd);             // camera from the cursor, or a default view
//       vec3 pos;
//       if (marchScene(ro, rd, pos)) { ... }
//   }
//
// Every shader that includes this one MUST define sceneSDF : the functions here
// call it, so leaving it out is a link error even if you never call them.
// ─────────────────────────────────────────────────────────────────────────────
#include <camera.glsl>

// you define this; GLSL lets us call it from here as long as it is declared
float sceneSDF(vec3 p);

// ── marching ─────────────────────────────────────────────────────────────────
#ifndef MARCH_STEPS
#define MARCH_STEPS 120
#endif
#ifndef MARCH_MAX_DIST
#define MARCH_MAX_DIST 40.0
#endif
#ifndef MARCH_EPS
#define MARCH_EPS 0.001
#endif

// walks the ray until it is within EPS of the surface. Returns whether it hit,
// and writes the hit point. Sphere tracing is exact for a true distance field :
// each step is as long as the guaranteed-empty ball around the current point.
bool marchScene(vec3 ro, vec3 rd, out vec3 pos) {
    float t = 0.0;
    for (int i = 0; i < MARCH_STEPS; ++i) {
        pos = ro + rd * t;
        float d = sceneSDF(pos);
        if (d < MARCH_EPS) return true;
        t += d;
        if (t > MARCH_MAX_DIST) break;
    }
    pos = ro + rd * t;
    return false;
}

// distance along the ray, or MARCH_MAX_DIST if it escaped (for fog / depth)
float marchDistance(vec3 ro, vec3 rd) {
    float t = 0.0;
    for (int i = 0; i < MARCH_STEPS; ++i) {
        float d = sceneSDF(ro + rd * t);
        if (d < MARCH_EPS) return t;
        t += d;
        if (t > MARCH_MAX_DIST) break;
    }
    return MARCH_MAX_DIST;
}

// the SDF's gradient is its normal, by central differences
vec3 sceneNormal(vec3 p) {
    vec2 e = vec2(0.0008, 0.0);
    return normalize(vec3(sceneSDF(p + e.xyy) - sceneSDF(p - e.xyy),
                          sceneSDF(p + e.yxy) - sceneSDF(p - e.yxy),
                          sceneSDF(p + e.yyx) - sceneSDF(p - e.yyx)));
}

// ── shading terms ────────────────────────────────────────────────────────────
// penumbra comes free : how close the ray passed to the geometry is already the
// distance field, so no extra sampling is needed for a soft edge
float softShadow(vec3 ro, vec3 rd, float mint, float maxt, float sharpness) {
    float res = 1.0, t = mint;
    for (int i = 0; i < 40; ++i) {
        float h = sceneSDF(ro + rd * t);
        if (h < 0.001) return 0.0;
        res = min(res, sharpness * h / t);
        t += clamp(h, 0.02, 0.4);
        if (t > maxt) break;
    }
    return clamp(res, 0.0, 1.0);
}

// how enclosed a point is : march a little way along the normal and see how
// much less distance we gained than we travelled
float ambientOcclusion(vec3 p, vec3 n) {
    float occ = 0.0, sca = 1.0;
    for (int i = 0; i < 5; ++i) {
        float hr = 0.01 + 0.12 * float(i) / 4.0;
        occ += (hr - sceneSDF(p + n * hr)) * sca;
        sca *= 0.85;
    }
    return clamp(1.0 - 1.5 * occ, 0.0, 1.0);
}

// grazing angles reflect more : the cheap rim light that reads as "solid"
float fresnel(vec3 rd, vec3 n, float power) {
    return pow(clamp(1.0 + dot(rd, n), 0.0, 1.0), power);
}

// classic checkerboard in world-space plane coordinates : the reference
// texture for judging scale and grounding a raymarched scene.
float checker(vec2 p, float scale) {
    vec2 c = floor(p / scale);
    return mod(c.x + c.y, 2.0);
}

// a serviceable default : one key light with a soft shadow, ambient tinted by
// the up direction and gated by occlusion, plus a rim
vec3 shadeDefault(vec3 p, vec3 n, vec3 rd, vec3 albedo, vec3 light_dir) {
    vec3 l = normalize(light_dir);
    float dif = clamp(dot(n, l), 0.0, 1.0);
    float sh  = softShadow(p + n * 0.02, l, 0.02, 20.0, 10.0);
    float amb = 0.5 + 0.5 * n.y;
    float ao  = ambientOcclusion(p, n);
    vec3 col = albedo * (dif * sh * vec3(1.05, 0.98, 0.88) * 1.5
                       + amb * vec3(0.22, 0.30, 0.45) * ao);
    return col + fresnel(rd, n, 3.0) * 0.25;
}

// the camera lives in <camera.glsl> : it has nothing to do with the SDF, and a
// shader tracing something else (a height field, say) wants it without being
// obliged to define sceneSDF
