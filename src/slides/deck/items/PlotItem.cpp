#include "slides/deck/items/DeckItem.h"
#include "slides/deck/items/JsonRead.h"
#include "content/screen_primitives/plots/Plot.h"
#include "content/screen_primitives/plots/Scatter.h"
#include "content/screen_primitives/plots/Legend.h"
#include <algorithm>

namespace slope {

namespace {

// The board a plot is drawn in, named outright or the last one declared.
std::string& lastBoard()
{
    static std::string name;
    return name;
}

std::string boardOf(const json& item, const char* type = "plot")
{
    const std::string b = item.value("board", lastBoard());
    if (b.empty())
        throw std::runtime_error(std::string("\"") + type + ": " +
                                 item[type].get<std::string>() +
                                 "\" has no board : name one with \"board:\", or declare "
                                 "a board item above it");
    return b;
}

// A deck value in the shape its parameter is saved in. Only "#rrggbb"
// needs converting.
json settingValue(const json& v)
{
    if (v.is_string()) {
        const std::string s = v;
        if (!s.empty() && s[0] == '#') {
            const RGBA c = parseColor(v);
            return json::array({c.Value.x, c.Value.y, c.Value.z, c.Value.w});
        }
    }
    return v;
}

// Every key of an item naming one of `known`, set on that namespace. A
// setting written here is fixed and never a knob.
void applySettings(Settings& ns, const std::vector<std::string>& known,
                   const json& item)
{
    for (const auto& [key, value] : item.items())
        if (std::find(known.begin(), known.end(), key) != known.end())
            ns.set(key, settingValue(value));
}

std::set<std::string> fieldsOf(std::vector<std::string> own,
                               const std::vector<std::string>& settings)
{
    std::set<std::string> f(own.begin(), own.end());
    f.insert(settings.begin(), settings.end());
    return f;
}

std::pair<int,int> resolution(const json& item)
{
    if (!item.contains("resolution")) return {1100, 620};
    const json& r = item["resolution"];
    if (!r.is_array() || r.size() != 2)
        throw std::runtime_error("\"resolution\" must be [width, height]");
    return {r[0].get<int>(), r[1].get<int>()};
}

// the points of a scatter, without the sources that only suit a line
ScatterPtr makeScatter(const json& item)
{
    const std::string name = item["scatter"];
    const std::string board = boardOf(item, "scatter");

    if (item.contains("data"))
        return Scatter::Add(name, board, path(item["data"].get<std::string>()));
    if (item.contains("values")) {
        std::vector<scalar> v;
        for (const auto& n : item["values"]) v.push_back(n.get<scalar>());
        const vec2 span = item.contains("span") ? readVec2(item["span"], "span") : vec2(0, 1);
        return Scatter::Add(name, board, v, span);
    }
    if (item.contains("points")) {
        std::vector<vec2> p;
        for (const auto& q : item["points"]) p.push_back(readVec2(q, "points"));
        return Scatter::Add(name, board, p);
    }
    throw std::runtime_error("\"scatter: " + name + "\" needs one source : \"data\" (a csv), "
                             "\"values\" or \"points\"");
}

// one source key, and it says how the numbers are read
PlotPtr makePlot(const json& item)
{
    const std::string name = item["plot"];
    const std::string board = boardOf(item);

    if (item.contains("data"))
        return Plot::Add(name, board, path(item["data"].get<std::string>()));
    if (item.contains("snippet"))
        return Plot::FromSnippet(name, board, item["snippet"].get<std::string>());
    if (item.contains("values")) {
        std::vector<scalar> v;
        for (const auto& n : item["values"]) v.push_back(n.get<scalar>());
        const vec2 span = item.contains("span") ? readVec2(item["span"], "span") : vec2(0, 1);
        return Plot::Add(name, board, v, span);
    }
    if (item.contains("points")) {
        std::vector<vec2> p;
        for (const auto& q : item["points"]) p.push_back(readVec2(q, "points"));
        return Plot::Add(name, board, p);
    }
    throw std::runtime_error("\"plot: " + name + "\" needs one source : \"data\" (a csv), "
                             "\"snippet\" (a Lua function), \"values\" or \"points\". "
                             "A formula is a Lua section, there is no formula parser");
}

// each publishes under its own name, so one walk of the item serves all
bool applyFigureSettings(const PrimitivePtr& prim, const json& item)
{
    if (auto b = std::dynamic_pointer_cast<Board>(prim)) {
        applySettings(b->settings, Board::settingNames(), item);
        return true;
    }
    if (auto p = std::dynamic_pointer_cast<Plot>(prim)) {
        applySettings(p->settings, Plot::settingNames(), item);
        if (item.contains("caption")) p->caption = item["caption"].get<std::string>();
        return true;
    }
    if (auto p = std::dynamic_pointer_cast<Scatter>(prim)) {
        applySettings(p->settings, Scatter::settingNames(), item);
        if (item.contains("caption")) p->caption = item["caption"].get<std::string>();
        return true;
    }
    if (auto p = std::dynamic_pointer_cast<Legend>(prim)) {
        applySettings(p->settings, Legend::settingNames(), item);
        return true;
    }
    return false;
}

} // namespace

std::vector<ItemSpec> plotItemSpecs()
{
    return {
        {
            "board", ItemSpec::Kind::Screen,
            fieldsOf({"board", "resolution"}, Board::settingNames()),
            // the name is the identity, one object across the whole deck
            [](const json& i) {
                auto [w, h] = resolution(i);
                lastBoard() = i["board"].get<std::string>();
                return "board:" + i["board"].get<std::string>() + ":"
                     + std::to_string(w) + "x" + std::to_string(h);
            },
            [](const json& i) -> PrimitivePtr {
                auto [w, h] = resolution(i);
                // left to the deck, or to the Tuner when it says nothing
                return Board::Add(i["board"].get<std::string>(), std::nullopt,
                                  std::nullopt, w, h);
            },
            [](const PrimitivePtr& p, const json& i, const std::string&) {
                // under the board's own name, an "id:" does not change it
                applyFigureSettings(p, i);
            },
            [](const json& i) { return i["board"].get<std::string>(); },
        },
        {
            "plot", ItemSpec::Kind::Screen,
            fieldsOf({"plot", "board", "data", "snippet", "values", "points", "span",
                      "caption"},
                     Plot::settingNames()),
            // the source is the key, so editing a colour rebuilds nothing
            [](const json& i) {
                std::string src;
                for (const char* k : {"data", "snippet", "values", "points", "span"})
                    if (i.contains(k)) src += std::string(k) + "=" + i[k].dump() + ";";
                return "plot:" + i["plot"].get<std::string>() + ":" + boardOf(i) + ":" + src;
            },
            [](const json& i) -> PrimitivePtr { return makePlot(i); },
            [](const PrimitivePtr& p, const json& i, const std::string&) {
                applyFigureSettings(p, i);
            },
            [](const json& i) { return i["plot"].get<std::string>(); },
        },
        {
            "scatter", ItemSpec::Kind::Screen,
            fieldsOf({"scatter", "board", "data", "values", "points", "span", "caption"},
                     Scatter::settingNames()),
            [](const json& i) {
                std::string src;
                for (const char* k : {"data", "values", "points", "span"})
                    if (i.contains(k)) src += std::string(k) + "=" + i[k].dump() + ";";
                return "scatter:" + i["scatter"].get<std::string>() + ":"
                     + boardOf(i, "scatter") + ":" + src;
            },
            [](const json& i) -> PrimitivePtr { return makeScatter(i); },
            [](const PrimitivePtr& p, const json& i, const std::string&) {
                applyFigureSettings(p, i);
            },
            [](const json& i) { return i["scatter"].get<std::string>(); },
        },
        {
            // "legend: fig" is the legend of the board "fig", publishing
            // under "fig_legend/"
            "legend", ItemSpec::Kind::Screen,
            fieldsOf({"legend"}, Legend::settingNames()),
            [](const json& i) { return "legend:" + i["legend"].get<std::string>(); },
            [](const json& i) -> PrimitivePtr {
                return Legend::Add(i["legend"].get<std::string>());
            },
            [](const PrimitivePtr& p, const json& i, const std::string&) {
                applyFigureSettings(p, i);
            },
            // not the board's own id, which names the frame
            [](const json& i) { return Legend::nameFor(i["legend"].get<std::string>()); },
        },
    };
}

}
