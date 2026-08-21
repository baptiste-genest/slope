#include "slides/deck/items/DeckItem.h"
#include "slides/deck/items/JsonRead.h"
#include "content/polyscope_primitives/Mesh.h"
#include "content/polyscope_primitives/PolyscopeSnippets.h"
#include "content/polyscope_primitives/Point.h"
#include <filesystem>

namespace slope {

static SnippetSurface::Spec surfaceSpec(const json& item)
{
    SnippetSurface::Spec spec;
    spec.fn = item["surface"];
    spec.name = item.value("id", spec.fn);
    if (item.contains("u")) spec.u = readVec2(item["u"], "u");
    if (item.contains("v")) spec.v = readVec2(item["v"], "v");
    if (item.contains("resolution")) {
        const json& r = item["resolution"];
        if (r.is_array()) {
            vec2 n = readVec2(r, "resolution");
            spec.res_u = int(n(0));
            spec.res_v = int(n(1));
        } else
            spec.res_u = spec.res_v = r.get<int>();
    }
    if (item.contains("closed")) {
        const json& c = item["closed"];
        if (c.is_array()) {
            if (c.size() != 2)
                throw std::runtime_error("\"closed\" must be a bool or [u, v]");
            spec.closed_u = c[0].get<bool>();
            spec.closed_v = c[1].get<bool>();
        } else
            spec.closed_u = spec.closed_v = c.get<bool>();
    }
    spec.smooth = item.value("smooth", true);
    return spec;
}

static SnippetCurve::Spec curveSpec(const json& item)
{
    SnippetCurve::Spec spec;
    spec.fn = item["curve"];
    spec.name = item.value("id", spec.fn);
    if (item.contains("u")) spec.u = readVec2(item["u"], "u");
    spec.resolution = item.value("resolution", 200);
    spec.closed = item.value("closed", false);
    spec.radius = item.value("radius", -1.);
    return spec;
}

std::vector<ItemSpec> sceneItemSpecs()
{
    std::vector<ItemSpec> specs;

    specs.push_back({
        "mesh", ItemSpec::Kind::Scene, {"id","at","alpha","smooth","normalize","group"},
        [](const json& i) {
            return "mesh:" + i["mesh"].get<std::string>()
                 + (i.value("smooth", true) ? ":smooth" : "")
                 + (i.value("normalize", false) ? ":norm" : "");
        },
        [](const json& i) -> PrimitivePtr {
            auto m = Mesh::Add(i["mesh"].get<std::string>(), i.value("smooth", true));
            if (i.value("normalize", false))
                m->normalize();
            return m;
        },
        nullptr,
        [](const json& i) {
            return std::filesystem::path(i["mesh"].get<std::string>()).stem().string();
        },
    });

    // cached on identity alone, so editing the domain or the resolution
    // reconfigures the object in place instead of building a second one
    specs.push_back({
        "surface", ItemSpec::Kind::Scene,
        {"id","at","alpha","smooth","u","v","resolution","closed","group"},
        [](const json& i) {
            auto spec = surfaceSpec(i);
            return "surface:" + spec.name + ":" + spec.fn;
        },
        [](const json& i) -> PrimitivePtr { return SnippetSurface::Add(surfaceSpec(i)); },
        [](const PrimitivePtr& p, const json& i, const std::string&) {
            std::static_pointer_cast<SnippetSurface>(p)->configure(surfaceSpec(i));
        },
        [](const json& i) { return surfaceSpec(i).name; },
    });

    specs.push_back({
        "curve", ItemSpec::Kind::Scene,
        {"id","at","alpha","u","resolution","closed","radius","group"},
        [](const json& i) {
            auto spec = curveSpec(i);
            return "curve:" + spec.name + ":" + spec.fn;
        },
        [](const json& i) -> PrimitivePtr { return SnippetCurve::Add(curveSpec(i)); },
        [](const PrimitivePtr& p, const json& i, const std::string&) {
            std::static_pointer_cast<SnippetCurve>(p)->configure(curveSpec(i));
        },
        [](const json& i) { return curveSpec(i).name; },
    });

    // "point: <snippet>" rides a snippet variable, "point: [x,y,z]" sits still
    specs.push_back({
        "point", ItemSpec::Kind::Scene, {"id","at","alpha","radius","group"},
        [](const json& i) {
            return "point:" + i["point"].dump() + ":" + std::to_string(i.value("radius", 0.05));
        },
        [](const json& i) -> PrimitivePtr {
            LiveVec p = readLiveVec(i["point"], "point");
            return Point::Add(DynamicParam([p](const TimeObject&) { return p.value(); }),
                              i.value("radius", 0.05));
        },
        nullptr,
        [](const json& i) {
            return i["point"].is_string() ? i["point"].get<std::string>() : "point";
        },
    });

    return specs;
}

}
