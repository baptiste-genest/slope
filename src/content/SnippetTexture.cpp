#include "SnippetTexture.h"

namespace slope {

void SnippetTexture::configure(const Spec &spec)
{
    const bool same = sp.fn == spec.fn && sp.res_u == spec.res_u && sp.res_v == spec.res_v
        && sp.u == spec.u && sp.v == spec.v && sp.components == spec.components
        && sp.when == spec.when;
    sp = spec;
    sp.components = std::clamp(sp.components,1,4);
    sp.res_u = std::max(1,sp.res_u);
    sp.res_v = std::max(1,sp.res_v);
    if (!same)
        sampled = false;
}

void SnippetTexture::sample()
{
    const int w = sp.res_u, h = std::max(1,sp.res_v), c = std::clamp(sp.components,1,4);
    const bool flat = h == 1;
    samples.assign(std::size_t(w)*h*c,0.f);

    auto call = Snippet::resolve(sp.fn);
    const int sizes[1] = {flat ? 1 : 2};
    scalar out[4];

    // the read set of this pass decides whether it is ever run again
    Snippet::beginRecord();
    for (int j = 0; j < h; j++){
        // texel centres, so the domain ends sit half a texel inside the edges
        const scalar y = h > 1 ? sp.v(0) + (sp.v(1)-sp.v(0))*(j+0.5)/h : 0;
        for (int i = 0; i < w; i++){
            const scalar x = sp.u(0) + (sp.u(1)-sp.u(0))*(i+0.5)/w;
            const scalar in[2] = {x,y};
            if (!Snippet::invoke(call,in,sizes,1,out,c))
                continue;   // a failing section leaves zeros rather than nothing
            float* q = samples.data() + (std::size_t(j)*w + i)*c;
            for (int k = 0; k < c; k++)
                q[k] = float(out[k]);
        }
    }
    deps = Snippet::endRecord();
    state = Snippet::stateOf(deps);
    sampled = true;
}

bool SnippetTexture::update()
{
    if (!Snippet::ready())
        return false;
    if (!sampled){
        sample();
        return true;
    }
    if (sp.when == Spec::When::Once)
        return false;
    if (sp.when == Spec::When::Always || deps.time){
        sample();
        return true;
    }
    // nothing it was built from has moved, so the samples still stand
    if (Snippet::stateOf(deps) == state)
        return false;
    sample();
    return true;
}

}
