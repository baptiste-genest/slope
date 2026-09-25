#pragma once
// Curves, grids and frames in data coordinates, at widths that stay put in
// pixels however the axes are scaled.
//
//   #include <plot.glsl>
//
// The data space is the shader's own view (setViewRect), read as iWorld(). The
// two axes scale independently, so every distance here is in pixels.
//
// Needs the built-in prelude (iResolution, iWorld, iPixelXY).

// compositing
// A plot is blitted over the slide, so it accumulates coverage instead of
// painting a background. Premultiplied while stacking, straight alpha out.
// What is put in first stays on top.
struct Ink { vec4 acc; };

// Empty ink.
Ink inkClear() { Ink k; k.acc = vec4(0.0); return k; }

// Adds a color with coverage a under what is already in the ink.
void inkOver(inout Ink k, vec3 col, float a)
{
    a = clamp(a, 0.0, 1.0);
    k.acc += (1.0 - k.acc.a) * vec4(col * a, a);
}

// Final color of the ink, with straight alpha.
vec4 inkResolve(Ink k)
{
    return k.acc.a > 1e-5 ? vec4(k.acc.rgb / k.acc.a, k.acc.a) : vec4(0.0);
}

// distances, in pixels
// Signed distance in pixels from p to the graph y = f(x). The slope term is
// what keeps a steep curve from drawing thicker than a flat one.
float sdGraph(vec2 p, float fx, float dfx, vec2 px)
{
    float gap   = (p.y - fx) / px.y;        // vertical, in pixels
    float slope = dfx * px.x / px.y;        // rise over run, both in pixels
    return gap / sqrt(1.0 + slope * slope);
}

// 1 inside a stroke of `width_px`, antialiased over the pixel it spans
float stroke(float d_px, float width_px)
{
    return smoothstep(0.5 * width_px + 0.5, 0.5 * width_px - 0.5, abs(d_px));
}

// dashes
// A dash pattern along x in pixels. `d` is (mark, gap), a zero mark is solid.
float dashMask(vec2 p, vec2 d, vec2 px)
{
    if (d.x <= 0.0) return 1.0;
    float period = max(d.x + d.y, 1.0);
    float s = fract(p.x / px.x / period) * period;
    return smoothstep(d.x + 0.5, d.x - 0.5, s);
}

// grid, axes, frame
// lines every `step` data units, one axis
float gridAxis(float v, float step, float pxv, float width_px)
{
    if (step <= 0.0) return 0.0;
    return stroke(abs(v - step * round(v / step)) / pxv, width_px);
}

// both axes at once; `step` is the spacing per axis
float gridMask(vec2 p, vec2 step, vec2 px, float width_px)
{
    return max(gridAxis(p.x, step.x, px.x, width_px),
               gridAxis(p.y, step.y, px.y, width_px));
}

// the two lines x = 0 and y = 0
float axesMask(vec2 p, vec2 px, float width_px)
{
    return max(stroke(p.x / px.x, width_px), stroke(p.y / px.y, width_px));
}

// a border just inside the data rectangle, drawn inwards to stay in the image
float frameMask(vec2 p, vec2 lo, vec2 hi, vec2 px, float width_px)
{
    vec2 d = min(p - lo, hi - p) / px;
    return smoothstep(width_px, width_px - 1.0, min(d.x, d.y));
}

// sampled data
// A curve uploaded as a 1-D texture of values over `span`. See inSpan for
// what happens outside it.
float dataAt(sampler2D tex, vec2 span, float x)
{
    return texture(tex, vec2((x - span.x) / (span.y - span.x), 0.5)).r;
}

// its slope, by central differences one texel apart
float dataSlope(sampler2D tex, vec2 span, float x)
{
    float h = (span.y - span.x) / float(textureSize(tex, 0).x);
    return (dataAt(tex, span, x + h) - dataAt(tex, span, x - h)) / (2.0 * h);
}

// 1 inside the interval the data covers, fading over the last pixel so the
// clamped sampler does not smear the end of a curve
float inSpan(float x, vec2 span, float pxx)
{
    return smoothstep(-0.5, 0.5, (x - span.x) / pxx)
         * smoothstep(0.5, -0.5, (x - span.y) / pxx);
}

// revealing
// A curve drawn on from left to right as `u` goes 0 -> 1 over `span`.
float revealMask(float x, vec2 span, float u, float pxx)
{
    float edge = mix(span.x, span.y, clamp(u, 0.0, 1.0));
    return smoothstep(0.5, -0.5, (x - edge) / pxx);
}
