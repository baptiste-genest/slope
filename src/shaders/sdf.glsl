#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// Signed distance fields, negative inside, zero on the surface, positive
// outside, and (for these) equal to the true Euclidean distance, which is what
// lets sphere tracing and constant-width outlines work.
//
//   #include <sdf.glsl>
//   float d = opSmoothUnion(sdSphere(p - a, 0.5), sdSphere(p - b, 0.5), 0.2);
// ─────────────────────────────────────────────────────────────────────────────

// ── 2D ───────────────────────────────────────────────────────────────────────
float sdCircle(vec2 p, float r) { return length(p) - r; }

float sdBox2(vec2 p, vec2 half_size) {
    vec2 d = abs(p) - half_size;
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
}

float sdRoundBox2(vec2 p, vec2 half_size, float r) {
    return sdBox2(p, half_size - r) - r;
}

float sdSegment2(vec2 p, vec2 a, vec2 b) {
    vec2 pa = p - a, ba = b - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - ba * h);
}

// regular n-gon of circumradius r
float sdNgon(vec2 p, float r, int n) {
    float a = atan(p.x, p.y) + 3.14159265;
    float step = 6.28318531 / float(n);
    a = floor(0.5 + a / step) * step - a;
    return cos(a) * length(p) - r * cos(step * 0.5);
}

// exact triangle, any winding, from its three corners
float sdTriangle(vec2 p, vec2 p0, vec2 p1, vec2 p2) {
    vec2 e0 = p1 - p0, e1 = p2 - p1, e2 = p0 - p2;
    vec2 v0 = p - p0,  v1 = p - p1,  v2 = p - p2;
    vec2 pq0 = v0 - e0 * clamp(dot(v0, e0) / dot(e0, e0), 0.0, 1.0);
    vec2 pq1 = v1 - e1 * clamp(dot(v1, e1) / dot(e1, e1), 0.0, 1.0);
    vec2 pq2 = v2 - e2 * clamp(dot(v2, e2) / dot(e2, e2), 0.0, 1.0);
    float s = sign(e0.x * e2.y - e0.y * e2.x);
    vec2 d = min(min(vec2(dot(pq0, pq0), s * (v0.x * e0.y - v0.y * e0.x)),
                      vec2(dot(pq1, pq1), s * (v1.x * e1.y - v1.y * e1.x))),
                      vec2(dot(pq2, pq2), s * (v2.x * e2.y - v2.y * e2.x)));
    return -sqrt(d.x) * sign(d.y);
}

// a ring wedge. `sc` is (sin, cos) of the half-aperture, `ra`/`rb` the ring's
// mid-radius and thickness. sc = vec2(sin(a), cos(a)) for a half-angle `a`.
float sdArc(vec2 p, vec2 sc, float ra, float rb) {
    p.x = abs(p.x);
    return ((sc.y * p.x > sc.x * p.y) ? length(p - sc * ra)
                                       : abs(length(p) - ra)) - rb;
}

// a filled pie slice of radius r ; `c` is (sin, cos) of the half-aperture
float sdPie(vec2 p, vec2 c, float r) {
    p.x = abs(p.x);
    float l = length(p) - r;
    float m = length(p - c * clamp(dot(p, c), 0.0, r));
    return max(l, m * sign(c.y * p.x - c.x * p.y));
}

// ── 3D ───────────────────────────────────────────────────────────────────────
float sdSphere(vec3 p, float r) { return length(p) - r; }

float sdPlane(vec3 p, vec3 n, float h) { return dot(p, normalize(n)) + h; }

float sdBox(vec3 p, vec3 half_size) {
    vec3 d = abs(p) - half_size;
    return length(max(d, 0.0)) + min(max(d.x, max(d.y, d.z)), 0.0);
}

float sdRoundBox(vec3 p, vec3 half_size, float r) {
    return sdBox(p, half_size - r) - r;
}

// t = (major radius, minor radius)
float sdTorus(vec3 p, vec2 t) {
    return length(vec2(length(p.xz) - t.x, p.y)) - t.y;
}

float sdCapsule(vec3 p, vec3 a, vec3 b, float r) {
    vec3 pa = p - a, ba = b - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - ba * h) - r;
}

float sdCylinder(vec3 p, float h, float r) {
    vec2 d = abs(vec2(length(p.xz), p.y)) - vec2(r, h);
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0));
}

// a cone / frustum between `a` (radius ra) and `b` (radius rb) ; ra or rb may
// be 0 for a plain cone. Follows sdCapsule's a/b parameterization, so the
// axis is not fixed to y like sdCylinder's.
float sdCappedCone(vec3 p, vec3 a, vec3 b, float ra, float rb) {
    float rba = rb - ra;
    float baba = dot(b - a, b - a);
    float papa = dot(p - a, p - a);
    float paba = dot(p - a, b - a) / baba;
    float x = sqrt(papa - paba * paba * baba);
    float cax = max(0.0, x - ((paba < 0.5) ? ra : rb));
    float cay = abs(paba - 0.5) - 0.5;
    float k = rba * rba + baba;
    float f = clamp((rba * (x - ra) + paba * baba) / k, 0.0, 1.0);
    float cbx = x - ra - f * rba;
    float cby = paba - f;
    float s = (cbx < 0.0 && cay < 0.0) ? -1.0 : 1.0;
    return s * sqrt(min(cax * cax + cay * cay * baba,
                        cbx * cbx + cby * cby * baba));
}

// like sdCapsule but tapered between two different radii, the organic,
// smoothly-tipped look that a capped cone's flat ends lack
float sdRoundCone(vec3 p, vec3 a, vec3 b, float r1, float r2) {
    vec3 ba = b - a;
    float l2 = dot(ba, ba);
    float rr = r1 - r2;
    float a2 = l2 - rr * rr;
    float il2 = 1.0 / l2;

    vec3 pa = p - a;
    float y = dot(pa, ba);
    float z = y - l2;
    vec3 pe = pa * l2 - ba * y;
    float x2 = dot(pe, pe);
    float y2 = y * y * l2;
    float z2 = z * z * l2;

    float k = sign(rr) * rr * rr * x2;
    if (sign(z) * a2 * z2 > k) return sqrt(x2 + z2) * il2 - r2;
    if (sign(y) * a2 * y2 < k) return sqrt(x2 + y2) * il2 - r1;
    return (sqrt(x2 * a2 * il2) + y * rr) * il2 - r1;
}

// bound, not exact (fine for marching, tight near the surface). `r` is the
// three semi-axes, so a sphere is r = vec3(radius)
float sdEllipsoid(vec3 p, vec3 r) {
    float k0 = length(p / r);
    float k1 = length(p / (r * r));
    return k0 * (k0 - 1.0) / k1;
}

// ── combining ────────────────────────────────────────────────────────────────
float opUnion    (float a, float b) { return min(a, b); }
float opIntersect(float a, float b) { return max(a, b); }
float opSubtract (float a, float b) { return max(-a, b); }   // b minus a

// union with a fillet of radius ~k. The workhorse, this is what makes two
// spheres read as one organic blob instead of two spheres.
float opSmoothUnion(float a, float b, float k) {
    k *= 4.0;
    float h = max(k - abs(a - b), 0.0);
    return min(a, b) - h * h * 0.25 / k;
}
float opSmoothIntersect(float a, float b, float k) {
    float h = clamp(0.5 - 0.5 * (b - a) / k, 0.0, 1.0);
    return mix(b, a, h) + k * h * (1.0 - h);
}
float opSmoothSubtract(float a, float b, float k) {
    float h = clamp(0.5 - 0.5 * (a + b) / k, 0.0, 1.0);
    return mix(a, -b, h) + k * h * (1.0 - h);
}

// hollow shell of thickness 2*t around the surface
float opShell(float d, float t) { return abs(d) - t; }

// exact offset of any signed distance field by r, rounding sharp edges and
// corners (apply before combining, as sdRoundBox does internally).
float opRound(float d, float r) { return d - r; }

// ── domain ───────────────────────────────────────────────────────────────────
// tile space with period `c`, so one primitive becomes infinitely many. The
// result is only a bound on the true distance, which is fine for marching as
// long as the primitive fits inside its cell.
vec3 opRepeat(vec3 p, vec3 c) { return mod(p + 0.5 * c, c) - 0.5 * c; }
vec2 opRepeat2(vec2 p, vec2 c) { return mod(p + 0.5 * c, c) - 0.5 * c; }

// mirror across x = 0, model half a symmetric object, get both halves
vec3 opMirrorX(vec3 p) { return vec3(abs(p.x), p.yz); }

vec3 opRotateY(vec3 p, float a) {
    float c = cos(a), s = sin(a);
    return vec3(c * p.x - s * p.z, p.y, s * p.x + c * p.z);
}
vec2 opRotate2(vec2 p, float a) {
    float c = cos(a), s = sin(a);
    return vec2(c * p.x - s * p.y, s * p.x + c * p.y);
}
