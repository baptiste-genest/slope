#ifndef SNIPPETTEXTURE_H
#define SNIPPETTEXTURE_H

#include "Snippet.h"

namespace slope {

/*
 * A callable snippet sampled onto a grid, ready to hand to a shader as a
 * texture. Lua cannot be called from GLSL, so this is how a snippet function
 * reaches the GPU : it is evaluated on the CPU once per grid point and uploaded.
 *
 *   --- prior_mean                     -- in snippets.lua
 *   return function(x) return 0.55*math.sin(1.15*x) end
 *
 *   SnippetTexture::Spec sp;
 *   sp.fn = "prior_mean";
 *   sp.u  = vec2(-6, 6);               -- what the width covers
 *   fx->setTexture("prior", sp);       -- uniform sampler2D prior;
 *
 * A 1D function (res_v == 1) is called with one number and gives a texture one
 * texel high; a 2D one is called with a vec2. The section may return 1 to 4
 * numbers, or a vec2/vec3, and `components` says how many of them to keep.
 *
 * ── Cost ───────────────────────────────────────────────────────────────────
 * Sampling is res_u * res_v Lua calls, so the question that matters is how
 * often it happens. It is answered by watching what the section reads :
 *
 *   the section reads t          resampled every frame
 *   it does not                  sampled once, and again only when a snippet
 *                                file is saved, or a value or parameter it
 *                                read has moved
 *
 * So a fixed function costs nothing per frame however fine the grid, and a
 * time dependent one wants a resolution you would be happy to pay for at 60Hz.
 * `when` overrides the verdict when you know better than the recording does.
 */
struct SnippetTexture {
    struct Spec {
        std::string fn;                 // the callable section to sample
        int res_u = 256, res_v = 1;     // res_v == 1 means a function of one number
        vec2 u = vec2(0,1);             // the parameter range the width covers
        vec2 v = vec2(0,1);             // and the height, when 2D
        int components = 1;             // 1..4, how many returned numbers to keep
        // Auto reads the verdict off what the section touched
        enum class When { Auto, Once, Always };
        When when = When::Auto;
    };

    explicit SnippetTexture(const Spec& spec) : sp(spec) {}

    void configure(const Spec& spec);
    const Spec& spec() const {return sp;}

    // resamples when it has to. True when the samples changed and want uploading
    bool update();

    const std::vector<float>& data() const {return samples;}
    int width() const {return sp.res_u;}
    int height() const {return std::max(1,sp.res_v);}
    int components() const {return sp.components;}
    // whether the last verdict was to resample every frame
    bool animated() const {return deps.time;}

private:
    Spec sp;
    std::vector<float> samples;
    Snippet::Deps deps;
    long state = 0;
    bool sampled = false;

    void sample();
};

}

#endif // SNIPPETTEXTURE_H
