#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// Following the talk.
//
// The prelude already hands the shader its TimeObject (from_action, from_begin,
// absolute_frame_number, transition_parameter, ...) and turns every keyframe the
// deck declared into a #define. This is the sugar on top : the staging patterns
// that otherwise get rewritten in every shader.
//
//   #include <slide.glsl>
//
//   vec3 col = mix(before, after, fadeInAt(reveal, 0.5));
//   int  s   = stageAfter(build, 3);       // 0,1,2,3 over the slides after it
//   col *= slideAlpha();                   // honour the deck's own transition
//
// Needs the built-in prelude, so it does not apply to a shader that brings its
// own #version.
// ─────────────────────────────────────────────────────────────────────────────

// ── within the current slide ─────────────────────────────────────────────────
// 0 -> 1 over the first `seconds` of the slide, then held
float fadeIn(float seconds) {
    return clamp(from_action / max(seconds, 1e-4), 0.0, 1.0);
}
// ... eased, for something a viewer watches rather than measures
float fadeInSmooth(float seconds) {
    float t = fadeIn(seconds);
    return t * t * (3.0 - 2.0 * t);
}
// 1 -> 0 over the first `seconds`
float fadeOut(float seconds) { return 1.0 - fadeIn(seconds); }

// a pulse that rises over `attack` and falls over `release`, for drawing the
// eye to something the moment a slide lands
float pulse(float attack, float release) {
    float up   = clamp(from_action / max(attack, 1e-4), 0.0, 1.0);
    float down = 1.0 - clamp((from_action - attack) / max(release, 1e-4), 0.0, 1.0);
    return up * down;
}

// ── relative to a keyframe ───────────────────────────────────────────────────
// 0 before the keyframe, then 0 -> 1 over `seconds` once it is reached. The
// ramp restarts on every later step, so use it on the keyframe's own slide.
float fadeInAt(int kf, float seconds) {
    return afterKeyframe(kf) ? fadeIn(seconds) : 0.0;
}
float fadeInAtSmooth(int kf, float seconds) {
    return afterKeyframe(kf) ? fadeInSmooth(seconds) : 0.0;
}

// 0 before the keyframe, 1 from it on : a hard switch
float onceAt(int kf) { return afterKeyframe(kf) ? 1.0 : 0.0; }

// 1 only while the deck sits between two keyframes
float betweenKeyframes(int from_kf, int to_kf) {
    return (afterKeyframe(from_kf) && beforeKeyframe(to_kf)) ? 1.0 : 0.0;
}

// which of `count` stages we are in, counting from a keyframe : 0 before it and
// on it, then 1, 2, ... clamped. The staged-reveal idiom in one call.
int stageAfter(int kf, int count) {
    return clamp(slidesSinceKeyframe(kf), 0, count);
}

// a continuous version : stage index blended by how far into the slide we are,
// so a staged quantity moves rather than jumps
float stageAfterSmooth(int kf, int count, float seconds) {
    float s = float(clamp(slidesSinceKeyframe(kf), 0, count));
    float prev = max(s - 1.0, 0.0);
    return mix(prev, s, fadeInSmooth(seconds));
}

// ── across the whole talk ────────────────────────────────────────────────────
// the deck's own intro/outro, eased. Multiply your colour by it and the shader
// joins slide transitions instead of popping in and out.
float slideAlpha() {
    float t = clamp(transition_parameter, 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

// seconds since this shader itself appeared, which unlike from_action does not
// reset on every step
float shaderTime() { return inner_time; }
