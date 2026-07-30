#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// Drawing graphs and grids in plot coordinates, with line widths that stay put
// in pixels however the plot is scaled.
//
//   #include <plot2d.glsl>
//
//   void main() {
//       float upp = unitsPerPixel(-4.0, 4.0);
//       vec2  p   = plotPoint(-4.0, 4.0);
//       vec3 col = BG;
//       col = mix(col, GRID_COL, 0.6 * gridMask(p, 1.0, upp));
//       col = mix(col, AXIS_COL, axesMask(p, upp));
//       col = mix(col, CURVE,    curveMask(p, sin(p.x), cos(p.x), 3.0, upp));
//       fragColor = vec4(col, 1.0);
//   }
//
// Needs the built-in prelude (iResolution), so it does not apply to a shader
// that brings its own #version.
// ─────────────────────────────────────────────────────────────────────────────

// plot units covered by one pixel, given the visible x range. The y range
// follows from the aspect ratio, which keeps grid cells square.
float unitsPerPixel(float xmin, float xmax) {
    return (xmax - xmin) / iResolution.x;
}

// this fragment's position in plot coordinates, centred on the x range with
// y = 0 through the middle of the rectangle
vec2 plotPoint(float xmin, float xmax) {
    float upp = unitsPerPixel(xmin, xmax);
    return vec2(0.5 * (xmin + xmax), 0.0) + (gl_FragCoord.xy - 0.5 * iResolution) * upp;
}
// ... with an explicit vertical centre
vec2 plotPointAt(float xmin, float xmax, float ycenter) {
    float upp = unitsPerPixel(xmin, xmax);
    return vec2(0.5 * (xmin + xmax), ycenter) + (gl_FragCoord.xy - 0.5 * iResolution) * upp;
}

// distance from p to the graph y = f(x), to first order. Dividing by the slope
// term is what keeps a steep curve from drawing thicker than a flat one : the
// vertical gap |y - f(x)| overestimates the true distance by exactly that.
float graphDist(float y, float fx, float dfx) {
    return abs(y - fx) / sqrt(1.0 + dfx * dfx);
}

// 1 inside a stroke of half-width `hw`, antialiased over `aa`
float stroke(float d, float hw, float aa) {
    return smoothstep(hw + aa, hw - aa, d);
}

// a curve of constant pixel width through (x, fx) with slope dfx
float curveMask(vec2 p, float fx, float dfx, float width_px, float upp) {
    return stroke(graphDist(p.y, fx, dfx), 0.5 * width_px * upp, upp);
}

// a filled disc marker of screen radius `r_px` pixels, antialiased over one
// pixel : scatter data, since curveMask only draws lines.
float pointMask(vec2 p, vec2 center, float r_px, float upp) {
    return 1.0 - smoothstep(-upp, upp, length(p - center) - r_px * upp);
}

// slope of f at p.x by central differences, for when you have no derivative.
// One pixel is the right step : smaller is noise, larger is a visible corner.
#define PLOT_SLOPE(f, x, upp) (((f)((x) + (upp)) - (f)((x) - (upp))) / (2.0 * (upp)))

// ── grid & axes ──────────────────────────────────────────────────────────────
// 1 on the grid lines of the given spacing
float gridMask(vec2 p, float spacing, float upp) {
    vec2 g = abs(p - spacing * round(p / spacing));
    return stroke(min(g.x, g.y), 0.0, 1.2 * upp);
}

// a coarse grid over a fine one, the fine one fainter : reads as a real plot
// rather than graph paper
float gridMaskMinor(vec2 p, float spacing, int subdivisions, float upp) {
    return gridMask(p, spacing / float(max(subdivisions, 1)), upp);
}

// 1 on the two axes
float axesMask(vec2 p, float upp) {
    return stroke(min(abs(p.x), abs(p.y)), 0.0, 1.4 * upp);
}

// 1 on the tick marks along the x axis : short strokes of `len_px` pixels
float xTickMask(vec2 p, float spacing, float len_px, float upp) {
    float x = abs(p.x - spacing * round(p.x / spacing));
    float on_tick = stroke(x, 0.0, 1.2 * upp);
    float in_span = 1.0 - smoothstep(0.0, len_px * upp, abs(p.y));
    return on_tick * in_span;
}
float yTickMask(vec2 p, float spacing, float len_px, float upp) {
    float y = abs(p.y - spacing * round(p.y / spacing));
    float on_tick = stroke(y, 0.0, 1.2 * upp);
    float in_span = 1.0 - smoothstep(0.0, len_px * upp, abs(p.x));
    return on_tick * in_span;
}

// ── regions ──────────────────────────────────────────────────────────────────
// 1 below the graph : for shading the area under a curve
float underCurve(vec2 p, float fx, float upp) {
    return smoothstep(upp, -upp, p.y - fx);
}
// 1 between two graphs
float betweenCurves(vec2 p, float lo, float hi, float upp) {
    return smoothstep(-upp, upp, p.y - lo) * smoothstep(upp, -upp, p.y - hi);
}
