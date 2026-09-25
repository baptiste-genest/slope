#ifndef DECK_SHADERITEM_H
#define DECK_SHADERITEM_H

#include "libslope.h"
#include "extern/json.hpp"

namespace slope {

class Shader;
using ShaderPtr = std::shared_ptr<Shader>;

/*
 * What a deck can say about a shader besides building it, which is "uniforms", "textures" and "view".
 * None of it needs the loader, so it is next to the "shader" item and not in DeckLoader.
 *
 * The "object:" branch declares the same inputs on a shader registered from C++,
 * so these functions can be called from outside the item.
 */

// Declares each uniform as a Params entry named "<ref>/<name>" and returns those names.
// With clear false, the other binds of the shader are kept. Use it for a shader that the deck did not create.
std::vector<std::string> declareShaderUniforms(const ShaderPtr& shader, const json& item,
                                               const std::string& ref, bool clear = true);
// Binds an image file to each named sampler, and removes what is no longer declared.
void declareShaderTextures(const ShaderPtr& shader, const json& item);
// Sets the "view", which is the region of the plane that the shader draws.
void declareShaderView(const ShaderPtr& shader, const json& item);

}

#endif // DECK_SHADERITEM_H
