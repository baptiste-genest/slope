#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// Hash-based noise. Everything here is a pure function of its input : no
// textures, no seeds, and the same point always gives the same value, which is
// what makes it safe under the ping-pong feedback passes.
//
//   #include <noise.glsl>
//   float h = fbm(p * 3.0, 5);
// ─────────────────────────────────────────────────────────────────────────────

// ── hashes ───────────────────────────────────────────────────────────────────
float hash11(float p) {
    p = fract(p * 0.1031);
    p *= p + 33.33;
    return fract(p * (p + p));
}
float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}
float hash13(vec3 p) {
    p = fract(p * 0.1031);
    p += dot(p, p.zyx + 31.32);
    return fract((p.x + p.y) * p.z);
}
vec2 hash22(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * vec3(0.1031, 0.1030, 0.0973));
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.xx + p3.yz) * p3.zy);
}
vec3 hash33(vec3 p) {
    p = fract(p * vec3(0.1031, 0.1030, 0.0973));
    p += dot(p, p.yxz + 33.33);
    return fract((p.xxy + p.yxx) * p.zyx);
}

// ── value noise ──────────────────────────────────────────────────────────────
// hash the lattice corners and interpolate. The quintic fade has zero first
// *and* second derivative at the ends, so the result has no visible grid.
float valueNoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    vec2 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
    return mix(mix(hash12(i + vec2(0, 0)), hash12(i + vec2(1, 0)), u.x),
               mix(hash12(i + vec2(0, 1)), hash12(i + vec2(1, 1)), u.x), u.y);
}

float valueNoise(vec3 p) {
    vec3 i = floor(p), f = fract(p);
    vec3 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
    return mix(mix(mix(hash13(i + vec3(0, 0, 0)), hash13(i + vec3(1, 0, 0)), u.x),
                   mix(hash13(i + vec3(0, 1, 0)), hash13(i + vec3(1, 1, 0)), u.x), u.y),
               mix(mix(hash13(i + vec3(0, 0, 1)), hash13(i + vec3(1, 0, 1)), u.x),
                   mix(hash13(i + vec3(0, 1, 1)), hash13(i + vec3(1, 1, 1)), u.x), u.y), u.z);
}

// ── gradient (Perlin-style) noise, in -1..1 ──────────────────────────────────
// random *gradients* rather than random values : zero at every lattice point,
// which removes the blobbiness of value noise
float gradientNoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    vec2 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
    vec2 g00 = normalize(hash22(i + vec2(0, 0)) * 2.0 - 1.0);
    vec2 g10 = normalize(hash22(i + vec2(1, 0)) * 2.0 - 1.0);
    vec2 g01 = normalize(hash22(i + vec2(0, 1)) * 2.0 - 1.0);
    vec2 g11 = normalize(hash22(i + vec2(1, 1)) * 2.0 - 1.0);
    return mix(mix(dot(g00, f - vec2(0, 0)), dot(g10, f - vec2(1, 0)), u.x),
               mix(dot(g01, f - vec2(0, 1)), dot(g11, f - vec2(1, 1)), u.x), u.y) * 1.4;
}

// ── fractal sums ─────────────────────────────────────────────────────────────
// octaves at doubling frequency and halving amplitude : the 1/f spectrum that
// natural detail tends to have
float fbm(vec2 p, int octaves) {
    float sum = 0.0, amp = 0.5, norm = 0.0;
    for (int i = 0; i < octaves; ++i) {
        sum += amp * valueNoise(p);
        norm += amp;
        p *= 2.02;          // slightly off 2 so octaves do not line up
        amp *= 0.5;
    }
    return sum / max(norm, 1e-8);
}

float fbm(vec3 p, int octaves) {
    float sum = 0.0, amp = 0.5, norm = 0.0;
    for (int i = 0; i < octaves; ++i) {
        sum += amp * valueNoise(p);
        norm += amp;
        p *= 2.02;
        amp *= 0.5;
    }
    return sum / max(norm, 1e-8);
}

// absolute value of signed noise per octave : creases instead of blobs
float ridgedFbm(vec2 p, int octaves) {
    float sum = 0.0, amp = 0.5, norm = 0.0;
    for (int i = 0; i < octaves; ++i) {
        sum += amp * (1.0 - abs(gradientNoise(p)));
        norm += amp;
        p *= 2.02;
        amp *= 0.5;
    }
    return sum / max(norm, 1e-8);
}

// warp the domain by more noise : turns bland fbm into something that looks
// like it flows
float domainWarp(vec2 p, int octaves, float strength) {
    vec2 q = vec2(fbm(p, octaves), fbm(p + vec2(5.2, 1.3), octaves));
    return fbm(p + strength * q, octaves);
}

// ── cellular (Worley) ────────────────────────────────────────────────────────
// distance to the nearest of one random point per cell. Returns
// (nearest, second nearest) : their difference outlines the cell borders.
vec2 worley(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    float d1 = 8.0, d2 = 8.0;
    for (int y = -1; y <= 1; ++y)
        for (int x = -1; x <= 1; ++x) {
            vec2 o = vec2(float(x), float(y));
            float d = length(o + hash22(i + o) - f);
            if (d < d1) { d2 = d1; d1 = d; }
            else if (d < d2) { d2 = d; }
        }
    return vec2(d1, d2);
}

// ── divergence-free 2D flow ──────────────────────────────────────────────────
// the perpendicular gradient of a scalar field is divergence-free by
// construction, so this never pools or sources : what you want to advect with
vec2 curlNoise(vec2 p, float eps) {
    float n1 = fbm(p + vec2(0.0, eps), 4);
    float n2 = fbm(p - vec2(0.0, eps), 4);
    float n3 = fbm(p + vec2(eps, 0.0), 4);
    float n4 = fbm(p - vec2(eps, 0.0), 4);
    return vec2(n1 - n2, -(n3 - n4)) / (2.0 * eps);
}
