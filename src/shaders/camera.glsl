#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// Building a primary ray for the current pixel. Independent of what you then
// trace against, an SDF, a height field, an implicit surface, so it does not
// drag in <raymarch.glsl> and its sceneSDF requirement.
//
//   #include <camera.glsl>
//   vec3 ro, rd;
//   orbitRay(ro, rd);
//
// Needs the built-in prelude (iResolution / iMouseNorm / iHovered), so it does
// not apply to a shader that brings its own #version.
// ─────────────────────────────────────────────────────────────────────────────

// ray through this pixel for a camera at `ro` looking at `target`.
// `lens` is the distance to the image plane, larger is a longer lens, i.e. a
// narrower field of view and less perspective distortion.
void lookAtRay(vec3 ro, vec3 target, float lens, out vec3 rd) {
    vec3 fw = normalize(target - ro);
    vec3 rt = normalize(cross(vec3(0.0, 1.0, 0.0), fw));
    vec3 up = cross(fw, rt);
    // divide by y so the vertical field of view is fixed and the horizontal one
    // widens with the rectangle, the picture does not squash when it is resized
    vec2 uv = (gl_FragCoord.xy - 0.5 * iResolution) / iResolution.y;
    rd = normalize(uv.x * rt + uv.y * up + lens * fw);
}

// camera on a sphere around `target`. `orbit` is (yaw, pitch) in 0..1.
void orbitRayAt(vec2 orbit, float radius, vec3 target, out vec3 ro, out vec3 rd) {
    float yaw   = (orbit.x - 0.5) * 6.2831853;
    float pitch = clamp((orbit.y - 0.5) * 2.6, -1.2, 1.3);
    ro = target + vec3(radius * cos(pitch) * sin(yaw),
                       radius * sin(pitch),
                       radius * cos(pitch) * cos(yaw));
    lookAtRay(ro, target, 1.6, rd);
}

// driven by the cursor while it is over the rectangle, and by a default 3/4
// view otherwise, so an export still looks composed
void orbitRayTarget(float radius, vec3 target, out vec3 ro, out vec3 rd) {
    vec2 orbit = (iHovered > 0.5) ? iMouseNorm : vec2(0.55, 0.62);
    orbitRayAt(orbit, radius, target, ro, rd);
}

void orbitRay(out vec3 ro, out vec3 rd) {
    orbitRayTarget(4.8, vec3(0.0, 0.2, 0.0), ro, rd);
}

// ── polyscope's camera ───────────────────────────────────────────────────────
// The ray polyscope itself would trace through this fragment, so a shader scene
// lands in the same world as the 3D scene behind it, an object the shader
// draws at world position X covers the polyscope geometry at X, and it stays
// registered while the user orbits.
//
//   #include <camera.glsl>
//   void main() {
//       vec3 ro, rd;
//       polyscopeRay(ro, rd);
//       ... trace, and write alpha 0 where nothing was hit so the 3D scene
//           shows through ...
//   }
//
// The shader draws into its own offscreen target and is blitted into a
// rectangle, so the fragment is first mapped back to the window pixel it will
// end up on (iScreenRect), then unprojected with polyscope's matrices.

// ── the screen, shared with slope ────────────────────────────────────────────
// What polyscopeRay is to the 3D scene, screenPoint is to the window, the
// referential a shader shares with everything outside it, whatever rectangle
// it is drawn into. slope's 2D parameters live in exactly this space, so a
// handle dragged in the panel is at screenPoint() == the parameter's value.

// this fragment in screen coordinates. 0..1 across the window, y up
vec2 screenPoint() {
    vec2 f  = gl_FragCoord.xy / iResolution;                 // 0..1 in the target, y up
    vec2 px = iScreenRect.xy + vec2(f.x, 1.0 - f.y) * iScreenRect.zw;  // window px, y down
    return vec2(px.x / iWindowSize.x, 1.0 - px.y / iWindowSize.y);
}

// the way back, a screen coordinate as this shader's own uv (0..1, y up),
// outside 0..1 when it falls off the rectangle
vec2 screenToLocal(vec2 s) {
    vec2 px = vec2(s.x * iWindowSize.x, (1.0 - s.y) * iWindowSize.y);
    vec2 q  = (px - iScreenRect.xy) / iScreenRect.zw;        // 0..1 in the rect, y down
    return vec2(q.x, 1.0 - q.y);
}

// the window's aspect, to keep a picture square on screen rather than in the
// rectangle, iAspect is the rectangle's own
float screenAspect() { return iWindowSize.x / iWindowSize.y; }

// this fragment's position in normalised device coordinates, as polyscope sees it
vec2 polyscopeNDC() { return 2.0 * screenPoint() - 1.0; }

void polyscopeRay(out vec3 ro, out vec3 rd) {
    vec2 ndc = polyscopeNDC();
    mat4 inv = iViewInv * iProjInv;          // clip -> camera -> world
    vec4 pn = inv * vec4(ndc, -1.0, 1.0);    // on the near plane
    vec4 pf = inv * vec4(ndc,  1.0, 1.0);    // on the far plane
    ro = pn.xyz / pn.w;
    rd = normalize(pf.xyz / pf.w - ro);
}

// depth of a world point the way polyscope's depth buffer stores it (0 at the
// near plane, 1 at the far one), so a shader hit can be compared against the
// 3D scene
float polyscopeDepth(vec3 world_pos) {
    vec4 clip = iProj * iView * vec4(world_pos, 1.0);
    return 0.5 * (clip.z / clip.w) + 0.5;
}

// ── compositing against the 3D scene ─────────────────────────────────────────
// Requires the primitive to have opted in :
//
//   fx->useSceneDepth();
//
// Without that, iSceneDepthValid is 0 and everything here reports "nothing in
// front", so a shader written against these degrades to drawing over the 3D
// scene rather than to hiding itself.
//
// Depth is current-frame, the shader defers past polyscope's scene pass to
// read it. useSceneDepth() also pins the renderer to one peel pass, since
// depth peeling otherwise clears the scene buffer every pass and leaves nothing.

// polyscope's depth at this fragment. 1.0 means nothing was drawn there (also
// true when no scene has rendered), so a shader degrades to "draw everywhere".
float sceneDepthHere() {
    if (iSceneDepthValid < 0.5) return 1.0;
    vec2 f  = gl_FragCoord.xy / iResolution;
    vec2 px = iScreenRect.xy + vec2(f.x, 1.0 - f.y) * iScreenRect.zw;   // window px, y down
    vec2 uv = vec2(px.x / iWindowSize.x, 1.0 - px.y / iWindowSize.y);   // texture uv, y up
    // outside the window the sampler would clamp and report whatever is on the
    // edge; say "empty" instead
    if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0))))
        return 1.0;
    // textureLod, not texture, implicit LOD is chosen from derivatives, which
    // are undefined in non-uniform control flow, and this is called from inside
    // raymarching loops, where neighbouring lanes have already exited. That
    // produced garbage in whole 8x8 blocks (an AMD wavefront). A screen-space
    // buffer has exactly one meaningful level anyway.
    return textureLod(iSceneDepth, uv, 0.0).r;
}

// true when a world point would be visible in front of polyscope's geometry.
// This is the whole point of the depth buffer, without it a shader always
// draws over the 3D scene, however far behind it actually is.
bool visibleOverScene(vec3 world_pos) {
    return polyscopeDepth(world_pos) <= sceneDepthHere();
}

// eye-space distance to polyscope's geometry at this fragment, positive and
// measured along the view axis; huge where nothing was drawn. Depth buffers
// are nonlinear, so this is what you want when comparing distances rather
// than just ordering them.
float sceneEyeDistance() {
    float d = sceneDepthHere();
    if (d >= 1.0) return 1e30;
    vec4 eye = iProjInv * vec4(polyscopeNDC(), 2.0 * d - 1.0, 1.0);
    return -eye.z / eye.w;
}

// how far in front of (negative) or behind (positive) the 3D scene a world
// point is, in world units along the view axis. The distance form of
// visibleOverScene(), for effects that need a margin rather than a yes/no.
float sceneClearance(vec3 world_pos) {
    // measured along the view axis, matching what the depth buffer stores;
    // the radial distance to the camera would bow the seam off-centre
    return -(iView * vec4(world_pos, 1.0)).z - sceneEyeDistance();
}

// soft occlusion. 0 fully visible, 1 fully hidden, easing over `fade` world
// units so the seam where a shader surface enters a mesh is not a hard jaggy
// edge.  col = mix(col, behind, sceneOcclusion(p, 0.02));
float sceneOcclusion(vec3 world_pos, float fade) {
    return smoothstep(-fade, fade, sceneClearance(world_pos));
}

// world-space position of the 3D scene's surface at this fragment, for shaders
// that decorate the real geometry rather than adding their own. Undefined
// where sceneDepthHere() is 1.0.
vec3 sceneWorldPos() {
    vec4 p = iViewInv * iProjInv * vec4(polyscopeNDC(), 2.0 * sceneDepthHere() - 1.0, 1.0);
    return p.xyz / p.w;
}
