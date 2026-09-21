// The arrow item's layouts and endpoints, and "replace" with an item that exists.
// Latex is never compiled, the cache dir is empty so every primitive only queues its compile.
#include "slope.h"
#include "slides/deck/DeckLoader.h"
#include "content/screen_primitives/text/LateX.h"
#include "content/screen_primitives/shapes/Shape2D.h"

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

    std::cout << (failures ? "FAILED" : "ok") << std::endl;
    return failures ? 1 : 0;
}
