// The arrow item's layouts and endpoints, and "replace" with an item that exists.
// Latex is never compiled, the cache dir is empty so every primitive only queues its compile.
#include "slope.h"
#include "slides/deck/DeckLoader.h"
#include "content/screen_primitives/text/LateX.h"
#include "content/screen_primitives/shapes/Shape2D.h"

#include <spdlog/sinks/ostream_sink.h>
#include <sstream>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;
using namespace slope;

static int failures = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            std::cerr << "CHECK FAILED: " #cond " at " << __FILE__ << ":"    \
                      << __LINE__ << std::endl;                              \
            failures++;                                                      \
        }                                                                    \
    } while (0)

static fs::path dir;
static DeckLoader deck;

// builds and reports the error, "" when the deck built
static std::string buildError(const std::string& body, SlideManager& show)
{
    std::ofstream(dir / "deck.yaml") << body;
    deck.init(dir / "deck.yaml");
    try {
        deck.build(show);
    } catch (const std::exception& e) {
        return e.what();
    }
    return "";
}

static LatexPtr findTex(SlideManager& show, int slide, const std::string& source)
{
    for (const auto& [ptr, sis] : show.getSlide(slide))
        if (auto l = std::dynamic_pointer_cast<Latex>(ptr); l && l->tex_source == source)
            return l;
    return nullptr;
}

static Arrow2DPtr findArrow(SlideManager& show, int slide)
{
    for (const auto& [ptr, sis] : show.getSlide(slide))
        if (auto a = std::dynamic_pointer_cast<Arrow2D>(ptr))
            return a;
    return nullptr;
}

// a scene primitive with no polyscope structure behind it
struct FakeScene : PolyscopePrimitive {};

int main()
{
    dir = fs::temp_directory_path() / "slope_deck_arrow_replace_test";
    fs::remove_all(dir);
    fs::create_directories(dir / "cache");
    Options::ProjectDataPath = dir.string() + "/";
    Options::ProjectViewsPath = dir.string() + "/";
    Options::CachePath = normalizedDir(dir / "cache");

    // ── arrow layouts ────────────────────────────────────────────────────────
    {
        SlideManager show;
        CHECK(buildError(R"(
slides:
  - frame:
      - arrow:
          id: dist1
          from: [0.2, -0.2, 1]
          to: d1aa
          head: false
)", show).empty());
        CHECK(Params::components("dist1/tail") == 3);
        CHECK(Params::components("dist1/d1aa") == 2);
        auto a = findArrow(show, 0);
        CHECK(a != nullptr);
        if (a) CHECK(a->head == 0);
    }
    {
        // a bare "arrow:" reads its fields beside it, and {follow: [..]} is the plain list
        SlideManager show;
        CHECK(buildError(R"(
slides:
  - frame:
      - arrow:
        from: {follow: [0, 0, 0]}
        to: [0.5, 0.5]
        bend: 0.2
)", show).empty());
        auto a = findArrow(show, 0);
        CHECK(a != nullptr);
        if (a) CHECK(a->bend == 0.2);
    }
    {
        // an arrow with no from/to inside a map is still an error
        SlideManager show;
        CHECK(!buildError(R"(
slides:
  - frame:
      - arrow: {bend: 0.2}
)", show).empty());
    }

    // ── replace ──────────────────────────────────────────────────────────────
    const std::string two = R"(
slides:
  - frame:
      - latex: $a$
        id: a
        at: [0.2, 0.3]
      - latex: $b$
        id: b
        at: [0.8, 0.7]
      - step
      - replace: a
        with: b
)";
    {
        SlideManager show;
        CHECK(buildError(two, show).empty());
        auto a0 = findTex(show, 0, "$a$"), b0 = findTex(show, 0, "$b$");
        CHECK(a0 && b0);
        CHECK(show.getNumberSlides() >= 2);
        if (a0 && b0 && show.getNumberSlides() >= 2) {
            // a fades out, b moves to where a was
            CHECK(!show.getSlide(1).contains(a0));
            CHECK(show.getSlide(1).contains(b0));
            CHECK(show.getSlide(1)[b0].getPosition().isApprox(show.getSlide(0)[a0].getPosition()));
            CHECK(show.getSlide(0)[b0].getPosition().isApprox(vec2(0.8, 0.7)));
        }
    }
    {
        // depth is the primitive's own, an item already shown keeps it
        SlideManager show;
        buildError(two, show);
        auto b0 = findTex(show, 0, "$b$");
        CHECK(b0 != nullptr);
        if (b0) CHECK(b0->getDepth() == 0);
    }
    {
        // with a new item, as before
        SlideManager show;
        CHECK(buildError(R"(
slides:
  - frame:
      - latex: $a$
        id: a
        at: [0.2, 0.3]
      - step
      - replace: a
        with: {latex: $c$, id: c}
)", show).empty());
        auto a0 = findTex(show, 0, "$a$"), c1 = findTex(show, 1, "$c$");
        CHECK(a0 && c1);
        if (a0 && c1) CHECK(!show.getSlide(1).contains(a0));
    }
    {
        SlideManager show;
        CHECK(!buildError(R"(
slides:
  - frame:
      - latex: $a$
        id: a
      - step
      - replace: a
        with: a
)", show).empty());
    }

    // ── C++ objects : the deck never overrides a place the C++ gave, it may only give one ──
    {
        StateInSlide at_first;
        at_first.persistentTransform = PersistentTransform("first");

        auto tex = Latex::Add(TexObject("$g$"));
        auto plain = Latex::Add(TexObject("$h$"));
        PrimitiveGroup placed;
        placed << PrimitiveInSlide{tex, at_first} << plain;
        deck.registerObject("placed_group", placed);

        PrimitiveGroup bare;
        bare << plain;
        deck.registerObject("bare_group", bare);

        deck.registerObject("placed_obj", PrimitiveInSlide{Latex::Add(TexObject("$p$")), at_first});
        deck.registerObject("free_obj", PrimitiveInSlide{Latex::Add(TexObject("$f$")), StateInSlide()});

        auto error = [&](const std::string& items) {
            SlideManager show;
            return buildError("slides:\n  - frame:\n" + items, show);
        };
        auto refused = [](const std::string& e) { return e.find("already places itself") != std::string::npos; };

        // a group with a place of its own : at: and set: are both refused, plain use is not
        CHECK(refused(error("      - object: placed_group\n        at: second\n")));
        CHECK(refused(error("      - object: placed_group\n      - step\n      - set: placed_group\n        at: third\n")));
        CHECK(error("      - object: placed_group\n").empty());
        // nothing in it is a scene member the deck could place
        CHECK(error("      - object: bare_group\n        at: second\n").find("no scene member") != std::string::npos);
        // a single object
        CHECK(refused(error("      - object: placed_obj\n        at: [0.3, 0.3]\n")));
        CHECK(error("      - object: placed_obj\n").empty());
        CHECK(error("      - object: free_obj\n        at: [0.3, 0.3]\n").empty());
        CHECK(refused(error("      - object: placed_obj\n        id: o\n      - step\n      - set: o\n        at: [0.1, 0.1]\n")));
        CHECK(error("      - object: free_obj\n        id: o\n      - step\n      - set: o\n        at: [0.1, 0.1]\n").empty());
    }

    // a group with a scene member and a quantity : "at:" places the member, the quantity goes along
    {
        auto scene = NewPrimitive<FakeScene>();
        auto quantity = Latex::Add(TexObject("$q$"));
        PrimitiveGroup g;
        g << scene << quantity;
        deck.registerObject("free_group", g);

        SlideManager show;
        CHECK(buildError(R"(
slides:
  - frame:
      - object: free_group
        at: gizmo_a
      - step
      - set: free_group
        at: gizmo_b
)", show).empty());
        if (show.getNumberSlides() >= 2) {
            CHECK(show.getSlide(0).contains(quantity));
            CHECK(show.getSlide(0)[scene].persistentTransform.getLabel() == "gizmo_a");
            CHECK(show.getSlide(1)[scene].persistentTransform.getLabel() == "gizmo_b");
        }
    }

    // ── C++ places are kept : used plainly, restyled, offset, never silently dropped ──
    {
        StateInSlide at_first;
        at_first.persistentTransform = PersistentTransform("first");
        auto on_plane = Latex::Add(TexObject("$k$"));
        deck.registerObject("plane_obj", PrimitiveInSlide{on_plane, at_first});
        StateInSlide abs(vec2(0.1, 0.9));
        auto absd = Latex::Add(TexObject("$m$"));
        deck.registerObject("abs_obj", PrimitiveInSlide{absd, abs});
        StateInSlide off;
        off.setOffset(vec2(0.2, 0.2));
        auto offd = Latex::Add(TexObject("$n$"));
        deck.registerObject("offset_obj", PrimitiveInSlide{offd, off});

        SlideManager show;
        CHECK(buildError("slides:\n  - frame:\n      - object: abs_obj\n        alpha: 0.5\n"
                         "      - object: plane_obj\n", show).empty());
        if (show.getNumberSlides()) {
            CHECK(show.getSlide(0)[absd].anchor == abs.anchor);
            CHECK(show.getSlide(0)[absd].alpha == 0.5);
            CHECK(show.getSlide(0)[on_plane].persistentTransform.getLabel() == "first");
        }
        // an offset is a place too
        SlideManager show2;
        CHECK(buildError("slides:\n  - frame:\n      - object: offset_obj\n        at: [0.5, 0.5]\n",
                         show2).find("already places itself") != std::string::npos);
        // a deck offset adds to the C++ one
        StateInSlide shifted(vec2(0.3, 0.3));
        shifted.shift = vec2(0.1, 0.);
        auto shd = Latex::Add(TexObject("$s$"));
        deck.registerObject("shift_obj", PrimitiveInSlide{shd, shifted});
        SlideManager show3;
        CHECK(buildError("slides:\n  - frame:\n      - object: shift_obj\n        offset: [0, 0.1]\n",
                         show3).empty());
        if (show3.getNumberSlides())
            CHECK((show3.getSlide(0)[shd].shift - vec2(0.1, 0.1)).norm() < 1e-9);
    }

    // ── scene groups and items : what the deck cannot do fails, a set keeps the rest ──
    {
        auto scene = NewPrimitive<FakeScene>();
        PrimitiveGroup g;
        g << scene;
        deck.registerObject("scene_group", g);
        auto mesh = NewPrimitive<FakeScene>();
        deck.registerObject("scene_obj", PrimitiveInSlide{mesh, StateInSlide()});
        auto error = [&](const std::string& items) {
            SlideManager show;
            return buildError("slides:\n  - frame:\n" + items, show);
        };
        CHECK(error("      - object: scene_group\n        at: [0.3, 0.3]\n").find("transform label") != std::string::npos);
        CHECK(error("      - latex: $x$\n        id: x\n        at: [0.5, 0.5]\n"
                    "      - object: scene_group\n        below: x\n").find("not \"below:\"") != std::string::npos);
        CHECK(error("      - object: scene_group\n        at: ga\n      - step\n"
                    "      - set: scene_group\n        follow: x\n").find("not \"follow:\"") != std::string::npos);
        CHECK(error("      - latex: $x$\n        id: x\n      - set: 42\n").find("\"set\" takes the id") != std::string::npos);
        std::ofstream(dir / "flat.pos") << "0.5 0.5";
        CHECK(error("      - object: scene_obj\n        at: flat\n").find("2D label") != std::string::npos);

        SlideManager show;
        CHECK(buildError("slides:\n  - frame:\n      - object: scene_obj\n        id: m\n        at: ga\n"
                         "        alpha: 0.4\n      - step\n      - set: m\n        at: gb\n", show).empty());
        if (show.getNumberSlides() >= 2) {
            CHECK(show.getSlide(1)[mesh].alpha == 0.4);
            CHECK(show.getSlide(1)[mesh].persistentTransform.getLabel() == "gb");
        }
    }

    // ── deck lines : errors in a group call or a template point at the deck ──
    {
        SlideManager show;
        std::string e = buildError("g:\n  - remove: nope\nslides:\n  - frame:\n      - latex: $x$\n      - g\n", show);
        CHECK(e.find("(in group \"g\") (line 6)") != std::string::npos);
        SlideManager show2;
        CHECK(buildError("template:\n  - remove: nope\nslides:\n  - frame:\n      - latex: $x$\n",
                         show2).find("(line 2)") != std::string::npos);
    }

    // ── deck lines : warnings and errors say where the item is ──
    {
        std::ostringstream log;
        auto sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(log);
        spdlog::default_logger()->sinks().push_back(sink);
        SlideManager show;
        // line 5 is the item with the stray key
        buildError("slides:\n"
                   "  - frame:\n"
                   "      - latex: $a$\n"
                   "        at: [0.2, 0.2]\n"
                   "      - latex: $b$\n"
                   "        bogus: 1\n"
                   "        at: [0.6, 0.6]\n", show);
        CHECK(log.str().find("deck (line 5): ignored key \"bogus\"") != std::string::npos);
        SlideManager show2;
        std::string err = buildError("slides:\n"
                                     "  - frame:\n"
                                     "      - latex: $a$\n"
                                     "        at: [0.2, 0.2]\n"
                                     "      - remove: nothing_here\n", show2);
        CHECK(err.find("(line 5)") != std::string::npos);
        spdlog::default_logger()->sinks().pop_back();
    }

    // ── ids : two different items under one id, or an id that is also a group, warn ──
    {
        std::ostringstream log;
        auto sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(log);
        spdlog::default_logger()->sinks().push_back(sink);
        auto warned = [&](const std::string& items, const std::string& what) {
            log.str("");
            SlideManager show;
            buildError("slides:\n  - frame:\n" + items, show);
            return log.str().find(what) != std::string::npos;
        };
        CHECK(warned("      - latex: $a$\n        id: x\n        at: [0.2, 0.2]\n"
                     "      - latex: $b$\n        id: x\n        at: [0.6, 0.6]\n", "names two different items"));
        CHECK(!warned("      - latex: $a$\n        id: x\n        at: [0.2, 0.2]\n"
                      "  - frame:\n      - latex: $a$\n        id: x\n        at: [0.2, 0.2]\n", "names two"));
        CHECK(warned("      - latex: $a$\n        id: placed_group\n        at: [0.2, 0.2]\n", "name of a group"));
        CHECK(!warned("      - latex: $a$\n        id: fresh\n        at: [0.2, 0.2]\n", "Give them different ids"));
        spdlog::default_logger()->sinks().pop_back();
    }

    std::cout << (failures ? "FAILED" : "ok") << std::endl;
    return failures ? 1 : 0;
}
