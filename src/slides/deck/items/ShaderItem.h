#ifndef DECK_SHADERITEM_H
#define DECK_SHADERITEM_H

#include "libslope.h"
#include "extern/json.hpp"

namespace slope {

class Shader;
using ShaderPtr = std::shared_ptr<Shader>;

/*
 * What a manifest can say about a shader, beyond building one : "uniforms",
 * "textures" and "view". None of it needs the loader, so it lives next to the
 * "shader" item record rather than in DeckLoader.
 *
 * The "object:" branch declares the same inputs on a shader registered from
 * C++, which is why these are reachable from outside the item record.
 */

// declares each uniform as a persistent Params entry named "<ref>/<name>" and
// returns those names. clear = false leaves the shader's other binds alone,
// for a shader the deck did not create
std::vector<std::string> declareShaderUniforms(const ShaderPtr& shader, const json& item,
                                               const std::string& ref, bool clear = true);
// binds an image file to each named sampler, dropping what is no longer declared
void declareShaderTextures(const ShaderPtr& shader, const json& item);
// "view", the region of the plane a shader draws
void declareShaderView(const ShaderPtr& shader, const json& item);

}

#endif // DECK_SHADERITEM_H
