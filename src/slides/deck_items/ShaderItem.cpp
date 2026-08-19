#include "ShaderItem.h"
#include "DeckItem.h"
#include "JsonRead.h"
#include "../../content/Params.h"
#include "../../content/Snippet.h"
#include "../../content/screen_primitives/Shader.h"
#include "spdlog/spdlog.h"
#include <filesystem>

namespace slope {

// a uniform name has to survive being pasted into GLSL as-is
static bool validGLSLName(const std::string& n)
{
    if (n.empty() || (!std::isalpha((unsigned char)n[0]) && n[0] != '_'))
        return false;
    for (char c : n)
        if (!std::isalnum((unsigned char)c) && c != '_')
            return false;
    return n.compare(0, 3, "gl_") != 0;
}

static const char* uniform_types = "float/int/bool/vec2/vec3/dir/color";

// "glsl" is the name the shader sees, "pname" the parameter it is bound to.
// The two differ only for an array element, "controls[3]".
static void declareUniform(const ShaderPtr& shader, const std::string& glsl,
                           const std::string& pname, const std::string& type,
                           const json& def, scalar mn, scalar mx)
{
    if (type == "float") {
        auto p = Params::Add(pname, def.is_null() ? 0. : def.get<scalar>(), mn, mx);
        shader->bind(glsl, [p] { return scalar(p); });
    } else if (type == "int") {
        auto p = Params::AddInt(pname, def.is_null() ? 0 : def.get<int>(),
                                int(mn), int(mx));
        shader->bindInt(glsl, [p] { return int(p); });
    } else if (type == "bool") {
        auto p = Params::AddBool(pname, def.is_null() ? false : def.get<bool>());
        shader->bindInt(glsl, [p] { return bool(p) ? 1 : 0; });
    } else if (type == "vec2") {
        auto p = Params::AddVec2(pname, def.is_null() ? vec2::Zero() : parseVec2(def), mn, mx);
        shader->bind(glsl, [p] { return vec2(p); });
    } else if (type == "vec3") {
        auto p = Params::AddVec(pname, def.is_null() ? vec::Zero() : parseVec3(def), mn, mx);
        shader->bind(glsl, [p] { return vec(p); });
    } else if (type == "dir") {
        auto p = Params::AddDir(pname, def.is_null() ? vec(0,0,1) : parseVec3(def));
        shader->bind(glsl, [p] { return vec(p); });
    } else if (type == "color") {
        auto p = Params::AddColor(pname, def.is_null() ? RGBA(1.f,1.f,1.f,1.f)
                                                       : parseColor(def));
        shader->bind(glsl, [p] { return RGBA(p); });
    } else {
        throw std::runtime_error("uniform \"" + glsl + "\" : unknown type \"" + type
                                 + "\" (" + uniform_types + ")");
    }
}

// the N of a "vec3[8]", or 0 when the type carries no array suffix
static int arrayCount(const std::string& name, const std::string& type)
{
    auto open = type.find('[');
    if (open == std::string::npos)
        return 0;
    if (type.back() != ']')
        throw std::runtime_error("uniform \"" + name + "\" : malformed array type \""
                                 + type + "\", write \"<type>[N]\"");
    int n = 0;
    try {
        size_t used = 0;
        n = std::stoi(type.substr(open + 1, type.size() - open - 2), &used);
        if (used != type.size() - open - 2)
            n = 0;
    } catch (const std::exception&) {
        n = 0;
    }
    if (n < 1 || n > 64)
        throw std::runtime_error("uniform \"" + name + "\" : an array uniform needs a "
                                 "size between 1 and 64, got \"" + type + "\"");
    return n;
}

// "uniforms:" on a shader item, or on an "object:" naming a shader. Each entry
// becomes a persistent, runtime tunable Params entry (Tuner panel, saved to
// views/params.json) bound to the shader uniform of the same name and re-read
// every frame, so the value a shader reacts to is dragged live and survives
// the session, with no C++.
//
//   uniforms:
//     sun:      dir                                 # a type name, zero valued
//     tint:     {type: color, default: "#ffcc88"}   # long form, with a default
//     speed:    {type: float, default: 1.0, min: 0, max: 5}  # bounds -> slider
//     controls: vec3[8]                             # an array, one parameter
//                                                   # per element, controls[i]
//
// An array's "default" is a list of one value per element. Quote the type in a
// flow mapping, {type: "vec3[8]", ...}, where yaml reads brackets itself.
//
// The parameter is named "<item>/<uniform>", which is also how the Tuner panel
// groups it. A uniform the compiled program does not declare is ignored, like
// every other Shader::bind, so editing the .frag live never breaks the deck.
//
// Returns the names it declared. `clear` drops the shader's whole user set
// first, which only suits a shader the deck created and owns.
std::vector<std::string> declareShaderUniforms(const ShaderPtr& shader,
                                                           const json& item,
                                                           const std::string& ref, bool clear)
{
    std::vector<std::string> declared;
    // the shader is cached across rebuilds, so a dropped uniform needs this
    if (clear)
        shader->clearUniforms();
    if (!item.contains("uniforms"))
        return declared;
    const json& us = item["uniforms"];

    // Two spellings. A list lets a name that needs no type stand on its own,
    // and a map is the older "name: type" form; entries may be mixed.
    std::vector<std::pair<std::string, json>> entries;
    if (us.is_array()) {
        for (const auto& e : us) {
            if (e.is_string())
                entries.emplace_back(e.get<std::string>(), json());
            else if (e.is_object())
                for (const auto& [k, v] : e.items())
                    entries.emplace_back(k, v);
            else if (e.is_boolean() || e.is_number())
                // yaml reads y, n, on, off, yes and no as booleans, and "1e5"
                // and friends as numbers, so such a name arrives already coerced
                throw std::runtime_error("a \"uniforms\" name was read as " + e.dump()
                                         + " : yaml treats y, n, on, off, yes and no as "
                                         "booleans, so quote it, - \"y\"");
            else
                throw std::runtime_error("a \"uniforms\" entry is a bare name, or "
                                         "\"name: <type>\"");
        }
    }
    else if (us.is_object()) {
        for (const auto& [k, v] : us.items())
            entries.emplace_back(k, v);
    }
    else
        throw std::runtime_error("\"uniforms\" must be a list of names, or a map of "
                                 "name: type (or name: {type, default, min, max})");

    for (const auto& [name, spec] : entries) {
        if (!validGLSLName(name)) {
            spdlog::warn("deck: \"{}\" is not a usable GLSL uniform name, ignored", name);
            continue;
        }
        // No type, so the name must already exist, as a parameter declared
        // earlier or as a snippet variable. One namespace, so either works, and
        // a name that resolves to nothing says so once
        if (spec.is_null()) {
            shader->bind(name);
            declared.push_back(name);
            continue;
        }

        json def;
        std::string type;
        scalar mn = 0, mx = 0;
        if (spec.is_string()) {
            type = spec.get<std::string>();
        } else if (spec.is_object()) {
            def = spec.value("default", json());
            type = spec.value("type", "");
            mn = spec.value("min", scalar(0));
            mx = spec.value("max", scalar(0));
            if (type.empty())
                throw std::runtime_error("uniform \"" + name + "\" needs a \"type\" ("
                                         + uniform_types + ")");
        } else {
            throw std::runtime_error("uniform \"" + name + "\" : write its type, "
                                     "\"" + name + ": <" + uniform_types + ">\", or the "
                                     "long form {type: ..., default: ...}");
        }

        int count = arrayCount(name, type);
        if (count == 0) {
            declareUniform(shader, name, ref + "/" + name, type, def, mn, mx);
            declared.push_back(name);
            continue;
        }
        if (!def.is_null() && (!def.is_array() || int(def.size()) != count))
            throw std::runtime_error("uniform \"" + name + "\" : its \"default\" must be "
                                     "a list of " + std::to_string(count) + " values, one "
                                     "per element");
        std::string base = type.substr(0, type.find('['));
        for (int i = 0; i < count; i++) {
            std::string idx = "[" + std::to_string(i) + "]";
            declareUniform(shader, name + idx, ref + "/" + name + idx, base,
                           def.is_null() ? json() : def[i], mn, mx);
            declared.push_back(name + idx);
        }
    }
    return declared;
}

// "textures:" on a shader item. Each entry binds an image file to the sampler
// of the same name, which the shader declares itself :
//
//   uniform sampler2D noise;        // in the .frag
//   uniform vec2      noise_size;   // optional, its size in pixels
//
//   textures:
//     noise: noise.png
//     grad:  {file: gradient.png, filter: nearest, wrap: repeat}
//     prior: {snippet: prior_mean, resolution: 512, domain: [-6, 6]}
//
// An image file or a sampled snippet. A texture fed by another pass needs a
// streaming order the manifest cannot express, and stays on the C++ side.
// {snippet: fn, resolution: N or [w,h], domain: [a,b] or [[a,b],[c,d]],
//  components: 1..4, resample: auto|once|always}
static SnippetTexture::Spec snippetTextureSpec(const std::string& name, const json& spec)
{
    SnippetTexture::Spec sp;
    sp.fn = spec["snippet"].get<std::string>();

    if (spec.contains("resolution")) {
        const json& r = spec["resolution"];
        if (r.is_array()) {
            vec2 n = readVec2(r, "resolution");
            sp.res_u = int(n(0));
            sp.res_v = int(n(1));
        } else
            sp.res_u = r.get<int>();
    }
    if (spec.contains("domain")) {
        const json& d = spec["domain"];
        if (!d.is_array() || d.empty())
            throw std::runtime_error("texture \"" + name + "\" : \"domain\" must be "
                                     "[a, b], or [[a, b], [c, d]] for a 2D one");
        if (d[0].is_array()) {
            if (d.size() != 2)
                throw std::runtime_error("texture \"" + name + "\" : a 2D \"domain\" is "
                                         "[[a, b], [c, d]]");
            sp.u = readVec2(d[0], "domain");
            sp.v = readVec2(d[1], "domain");
        } else
            sp.u = readVec2(d, "domain");
    }
    sp.components = spec.value("components", 1);
    if (sp.components < 1 || sp.components > 4)
        throw std::runtime_error("texture \"" + name + "\" : \"components\" is 1 to 4");

    const std::string w = spec.value("resample", "auto");
    if      (w == "once")   sp.when = SnippetTexture::Spec::When::Once;
    else if (w == "always") sp.when = SnippetTexture::Spec::When::Always;
    else if (w != "auto")
        throw std::runtime_error("texture \"" + name + "\" : \"resample\" must be "
                                 "\"auto\", \"once\" or \"always\"");
    return sp;
}

void declareShaderTextures(const ShaderPtr& shader, const json& item)
{
    std::vector<std::string> declared;
    if (item.contains("textures")) {
        const json& ts = item["textures"];
        if (!ts.is_object())
            throw std::runtime_error("\"textures\" must be a map of "
                                     "name: file (or name: {file, filter, wrap})");
        for (const auto& [name, spec] : ts.items()) {
            if (!validGLSLName(name)) {
                spdlog::warn("deck: \"{}\" is not a usable GLSL sampler name, ignored", name);
                continue;
            }
            std::string file;
            auto filter = Shader::Filter::Linear;
            auto wrap   = Shader::Wrap::Clamp;
            if (spec.is_object()) {
                if (!spec.contains("file") && !spec.contains("snippet"))
                    throw std::runtime_error("texture \"" + name + "\" needs a \"file\" "
                                             "or a \"snippet\"");
                if (spec.contains("file") && spec.contains("snippet"))
                    throw std::runtime_error("texture \"" + name + "\" is either a "
                                             "\"file\" or a \"snippet\", not both");
                if (spec.contains("file"))
                    file = spec["file"];
                const std::string fs = spec.value("filter", "linear");
                const std::string ws = spec.value("wrap", "clamp");
                if      (fs == "nearest") filter = Shader::Filter::Nearest;
                else if (fs != "linear")
                    throw std::runtime_error("texture \"" + name + "\" : filter must be "
                                             "\"nearest\" or \"linear\"");
                if      (ws == "repeat") wrap = Shader::Wrap::Repeat;
                else if (ws != "clamp")
                    throw std::runtime_error("texture \"" + name + "\" : wrap must be "
                                             "\"clamp\" or \"repeat\"");
            } else if (spec.is_string()) {
                file = spec;
            } else {
                throw std::runtime_error("texture \"" + name + "\" must be a file name "
                                         "or {file: ..., filter: ..., wrap: ...}");
            }
            if (file.empty()) {
                shader->setTexture(name, snippetTextureSpec(name, spec), filter, wrap);
                declared.push_back(name);
                continue;
            }
            // re-setting the same file is a no-op, so a hot reload does not
            // re-decode every image on every save
            shader->setTexture(name, file, filter, wrap);
            declared.push_back(name);
        }
    }
    // whatever the manifest no longer declares is unbound (the shader itself
    // is cached across rebuilds, so nothing else would drop it)
    shader->retainTextures(declared);
}

// "view" is the half-height, a number or a snippet name. Without one a shader
// has no world space and nothing can follow a point of it.
void declareShaderView(const ShaderPtr& shader, const json& item)
{
    if (!item.contains("view"))
        return;
    const json& v = item["view"];

    auto halfOf = [](const json& h) -> std::function<scalar()> {
        if (h.is_number()) {
            scalar x = h.get<scalar>();
            if (!(x > 0))
                throw std::runtime_error("\"view\" half-height must be greater than zero");
            return [x] { return x; };
        }
        if (h.is_string()) { std::string n = h; return [n] { return Snippet::get(n).num(); }; }
        throw std::runtime_error("\"view\" half-height must be a number or a snippet name");
    };
    auto centerOf = [](const json& c) -> std::function<vec2()> {
        if (c.is_array() && c.size() == 2) {
            vec2 p(c[0].get<scalar>(), c[1].get<scalar>());
            return [p] { return p; };
        }
        if (c.is_string()) { std::string n = c; return [n] { return Snippet::get(n).v2(); }; }
        throw std::runtime_error("\"view\" center must be [x, y] or a snippet name");
    };
    std::function<vec2()> origin = [] { return vec2::Zero(); };

    if (v.is_object()) {
        if (!v.contains("half"))
            throw std::runtime_error("\"view\" needs a \"half\" : half the height it "
                                     "shows, in world units");
        shader->bindView(v.contains("center") ? centerOf(v["center"]) : origin,
                         halfOf(v["half"]));
        return;
    }
    shader->bindView(origin, halfOf(v));
}

std::vector<ItemSpec> shaderItemSpecs()
{
    // a single-pass fragment shader. Multi-pass, channels and SSBOs stay on
    // the C++ side
    auto resolution = [](const json& item) {
        std::pair<int,int> wh{0, 0};
        if (!item.contains("resolution"))
            return wh;
        const json& r = item["resolution"];
        if (!r.is_array() || r.size() != 2)
            throw std::runtime_error("\"resolution\" must be [width, height]");
        return std::pair<int,int>{r[0].get<int>(), r[1].get<int>()};
    };

    return {{
        "shader", ItemSpec::Kind::Screen, {"resolution","uniforms","textures","view"},
        // the resolution is part of the key, one .frag at two sizes is two
        // primitives and a hot reload reuses the GL resources of each
        [resolution](const json& i) {
            auto [w, h] = resolution(i);
            return "shader:" + i["shader"].get<std::string>() + ":"
                 + std::to_string(w) + "x" + std::to_string(h);
        },
        [resolution](const json& i) -> PrimitivePtr {
            auto [w, h] = resolution(i);
            return Shader::FromFile(i["shader"].get<std::string>(), w, h);
        },
        // named after the item, so two placements of the same .frag under
        // different ids get their own tunable set
        [](const PrimitivePtr& p, const json& i, const std::string& name) {
            auto sh = std::static_pointer_cast<Shader>(p);
            declareShaderUniforms(sh, i, name);
            declareShaderTextures(sh, i);
            declareShaderView(sh, i);
        },
        [](const json& i) {
            return std::filesystem::path(i["shader"].get<std::string>()).stem().string();
        },
    }};
}

}
