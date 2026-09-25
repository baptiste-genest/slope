#pragma once
// Complex arithmetic on vec2 (x = real, y = imaginary), and domain colouring,
// the standard way to show a whole complex function on one picture.
//
//   #include <complex.glsl>
//
//   vec2 z = plotPoint(-2.0, 2.0);            // from <plot2d.glsl>
//   vec2 w = cdiv(csub(cpow(z, 3.0), ONE), cadd(cmul(z, z), ONE));
//   fragColor = vec4(domainColor(w), 1.0);
//
// NB this header includes <colormap.glsl> for hsv2rgb, so everything in that
// one (viridis, turbo, remap, ...) is in scope here too, defining your own
// function by one of those names is a redefinition even if you never asked
// for the colour maps.
#include <colormap.glsl>          // hsv2rgb

#define I   vec2(0.0, 1.0)
#define ONE vec2(1.0, 0.0)

// Sum of two complex numbers.
vec2 cadd(vec2 a, vec2 b) { return a + b; }
// Difference of two complex numbers.
vec2 csub(vec2 a, vec2 b) { return a - b; }
// Product of two complex numbers.
vec2 cmul(vec2 a, vec2 b) { return vec2(a.x*b.x - a.y*b.y, a.x*b.y + a.y*b.x); }
// Complex conjugate.
vec2 cconj(vec2 a)        { return vec2(a.x, -a.y); }

// Quotient of two complex numbers.
vec2 cdiv(vec2 a, vec2 b) {
    float d = dot(b, b);
    return vec2(a.x*b.x + a.y*b.y, a.y*b.x - a.x*b.y) / max(d, 1e-20);
}
// Inverse of a complex number.
vec2 cinv(vec2 a) { return cconj(a) / max(dot(a, a), 1e-20); }

// Argument of a complex number, in (-pi,pi].
float carg(vec2 a) { return atan(a.y, a.x); }
// Modulus of a complex number.
float cabs(vec2 a) { return length(a); }

// Complex exponential.
vec2 cexp(vec2 a) { return exp(a.x) * vec2(cos(a.y), sin(a.y)); }
// principal branch, the imaginary part jumps by 2*pi across the negative real
// axis, and that discontinuity is real, not an artefact
// Principal value of the complex logarithm.
vec2 clog(vec2 a) { return vec2(log(max(cabs(a), 1e-20)), carg(a)); }

// Complex number a to a real power k.
vec2 cpow(vec2 a, float k) {
    float r = pow(max(cabs(a), 1e-20), k), t = carg(a) * k;
    return r * vec2(cos(t), sin(t));
}
// Complex number a to a complex power b.
vec2 cpow(vec2 a, vec2 b) { return cexp(cmul(b, clog(a))); }
// Principal square root.
vec2 csqrt(vec2 a) { return cpow(a, 0.5); }

// Complex sine.
vec2 csin(vec2 a) { return vec2(sin(a.x) * cosh(a.y),  cos(a.x) * sinh(a.y)); }
// Complex cosine.
vec2 ccos(vec2 a) { return vec2(cos(a.x) * cosh(a.y), -sin(a.x) * sinh(a.y)); }
// Complex tangent.
vec2 ctan(vec2 a) { return cdiv(csin(a), ccos(a)); }

// Möbius transformation (az + b) / (cz + d)
vec2 mobius(vec2 z, vec2 a, vec2 b, vec2 c, vec2 d) {
    return cdiv(cadd(cmul(a, z), b), cadd(cmul(c, z), d));
}

// domain colouring
// hue is the argument, so zeros and poles are the points every colour meets
// (and you can read the order off how many times the wheel wraps). Brightness
// steps at each doubling of |w|, giving contour bands that thin towards a zero
// and crowd towards a pole.
vec3 domainColor(vec2 w) {
    float hue = carg(w) / 6.28318531 + 0.5;
    float m   = log2(max(cabs(w), 1e-20));
    float band = fract(m);                       // 0..1 within an octave of |w|
    float val = 0.55 + 0.45 * band;
    float sat = 0.95 - 0.25 * band;
    return hsv2rgb(vec3(hue, sat, val));
}

// with the argument contours drawn too, the grid you actually measure angles
// on, `spokes` lines per turn
vec3 domainColorGrid(vec2 w, int spokes) {
    vec3 col = domainColor(w);
    float a = carg(w) / 6.28318531 * float(spokes);
    float line = 1.0 - smoothstep(0.0, 0.06, abs(fract(a + 0.5) - 0.5));
    return mix(col, col * 0.65, line);
}
