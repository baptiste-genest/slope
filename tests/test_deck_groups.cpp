// Deck groups : params, the "- name: id" call, steps, tags, and the errors a
// mistyped declaration or call raises. Latex is never compiled, the cache dir
// is empty so every primitive only queues its compile.
#include "slope.h"
#include "slides/deck/DeckLoader.h"
#include "content/screen_primitives/text/LateX.h"

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

static void load(const std::string& body)
{
    std::ofstream(dir / "deck.yaml") << body;
    deck.init(dir / "deck.yaml");
}

// the latex primitive with this exact source on a slide, or null
static LatexPtr findTex(SlideManager& show, int slide, const std::string& source)
{
    for (const auto& [ptr, sis] : show.getSlide(slide))
        if (auto l = std::dynamic_pointer_cast<Latex>(ptr); l && l->tex_source == source)
            return l;
    return nullptr;
}

static vec2 posOf(SlideManager& show, int slide, const PrimitivePtr& p)
{
    return show.getSlide(slide)[p].getPosition();
}

// builds and reports the error, "" when the deck built
static std::string buildError(const std::string& body)
{
    load(body);
    SlideManager show;
    try {
        deck.build(show);
    } catch (const std::exception& e) {
        return e.what();
    }
    return "";
}

static void expectError(const std::string& body, const std::string& part, int line)
{
    const std::string err = buildError(body);
    if (err.find(part) == std::string::npos) {
        std::cerr << "CHECK FAILED: error containing \"" << part << "\" at " << __FILE__
                  << ":" << line << ", got \"" << err << "\"" << std::endl;
        failures++;
    }
}

int main()
{
    dir = fs::temp_directory_path() / "slope_deck_groups_test";
    fs::remove_all(dir);
    fs::create_directories(dir / "cache");
    Options::ProjectDataPath = dir.string() + "/";
    Options::ProjectViewsPath = dir.string() + "/";
    Options::CachePath = normalizedDir(dir / "cache");

    // ── params : whole values keep their type, ${} goes inside strings ──────
    load(R"(
fig:
  params: {label: , pos: [0.5, 0.5]}
  items:
    - latex: ${label} for ${id}, $x^2$ and ${n}^2$
      at: $pos
slides:
  - frame:
      - fig: conv
        label: residual
        pos: [0.2, 0.3]
      - fig: logc
        label: error
        pos: [0.8, 0.3]
)");
    {
        SlideManager show;
        deck.build(show);
        auto a = findTex(show, 0, "residual for conv, $x^2$ and ${n}^2$");
        auto b = findTex(show, 0, "error for logc, $x^2$ and ${n}^2$");
        CHECK(a != nullptr);
        CHECK(b != nullptr);
        if (a && b)
            CHECK(posOf(show, 0, a)(0) < posOf(show, 0, b)(0));
        // each call is tagged by its id and by the group name
        CHECK(show.hasGroup("conv"));
        CHECK(show.hasGroup("logc"));
        CHECK(show.hasGroup("fig"));
    }

    // ── a key left empty keeps its default, no id is fine without $id ───────
    load(R"(
card:
  params: {label: default text}
  items:
    - latex: ${label}
      at: [0.5, 0.5]
slides:
  - frame:
      - card:
        label:
)");
    {
        SlideManager show;
        deck.build(show);
        CHECK(findTex(show, 0, "default text") != nullptr);
    }

    // ── a plain list, removed by the group name ─────────────────────────────
    load(R"(
caption:
  - latex: a caption
    at: [0.5, 0.9]
slides:
  - frame:
      - caption
      - step
      - remove: caption
)");
    {
        SlideManager show;
        deck.build(show);
        CHECK(show.getNumberSlides() == 2);
        CHECK(findTex(show, 0, "a caption") != nullptr);
        CHECK(findTex(show, 1, "a caption") == nullptr);
    }

    // ── steps : the frame goes on after the group's last step ───────────────
    load(R"(
reveal:
  params: {what: }
  items:
    - latex: ${what} one
      at: [0.3, 0.3]
    - step
    - latex: ${what} two
      at: [0.6, 0.6]
outer:
  - latex: outer line
    at: [0.5, 0.9]
  - reveal:
    what: nested
slides:
  - frame:
      - reveal: r1
        what: first
      - latex: after the call
        at: [0.5, 0.1]
      - step
      - remove: r1
  - frame:
      - outer
)");
    {
        SlideManager show;
        deck.build(show);
        CHECK(show.getNumberSlides() == 5);
        CHECK(findTex(show, 0, "first one") != nullptr);
        CHECK(findTex(show, 0, "first two") == nullptr);
        CHECK(findTex(show, 0, "after the call") == nullptr);
        CHECK(findTex(show, 1, "first two") != nullptr);
        CHECK(findTex(show, 1, "after the call") != nullptr);
        // removed on every step it spans, the frame's own item stays
        CHECK(findTex(show, 2, "first one") == nullptr);
        CHECK(findTex(show, 2, "first two") == nullptr);
        CHECK(findTex(show, 2, "after the call") != nullptr);
        // nested in a group with no step of its own
        CHECK(findTex(show, 3, "nested one") != nullptr);
        CHECK(findTex(show, 3, "outer line") != nullptr);
        CHECK(findTex(show, 4, "nested two") != nullptr);
        CHECK(findTex(show, 4, "outer line") != nullptr);
    }

    // ── a reused group runs its remove and set every time ───────────────────
    load(R"(
clear:
  - remove: note
mover:
  - set: hello
    at: [0.8, 0.8]
  - latex: mover own
    at: [0.5, 0.5]
slides:
  - frame:
      - latex: note one
        id: note
        at: [0.3, 0.4]
      - step
      - clear
      - step
      - latex: note two
        id: note
        at: [0.3, 0.6]
      - step
      - clear
  - frame:
      - latex: hello
        id: hello
        at: [0.2, 0.3]
      - step
      - mover
  - frame:
      - latex: hello
        id: hello
        at: [0.2, 0.3]
      - step
      - mover
      - step
      - remove: mover
)");
    {
        SlideManager show;
        deck.build(show);
        CHECK(show.getNumberSlides() == 9);
        CHECK(findTex(show, 2, "note two") != nullptr);
        CHECK(findTex(show, 3, "note two") == nullptr);
        auto hello = findTex(show, 6, "hello");
        CHECK(hello != nullptr);
        if (hello) {
            CHECK(posOf(show, 7, hello)(0) > posOf(show, 6, hello)(0));
            // the target of a set is not the group's, removing the group keeps it
            CHECK(show.getSlide(8).count(hello) == 1);
        }
        CHECK(findTex(show, 7, "mover own") != nullptr);
        CHECK(findTex(show, 8, "mover own") == nullptr);
    }

    // ── the same content is the same primitive, on another slide and after a
    //    rebuild, so neither a reuse nor a hot reload cross-fades it ─────────
    load(R"(
tagged:
  - latex: shared text
    at: [0.5, 0.5]
slides:
  - frame:
      - latex: shared text
        at: [0.5, 0.5]
      - tagged: t1
      - step
      - remove: t1
  - frame:
      - tagged
)");
    {
        SlideManager first, second;
        deck.build(first);
        deck.build(second);
        auto a = findTex(first, 0, "shared text");
        CHECK(a != nullptr);
        // tagged although it was on the slide before the call
        CHECK(findTex(first, 1, "shared text") == nullptr);
        CHECK(findTex(first, 2, "shared text") == a);
        CHECK(findTex(second, 2, "shared text") == a);
    }

    // ── "group:" on a call tags what it placed ──────────────────────────────
    load(R"(
card:
  - latex: tagged card
    at: [0.5, 0.5]
slides:
  - frame:
      - card:
        group: extra
      - step
      - remove: extra
)");
    {
        SlideManager show;
        deck.build(show);
        CHECK(findTex(show, 0, "tagged card") != nullptr);
        CHECK(findTex(show, 1, "tagged card") == nullptr);
    }

    // ── a template may call a group without steps ───────────────────────────
    CHECK(buildError(R"(
foot:
  - latex: footer
    at: [0.5, 0.95]
template:
  - foot
slides:
  - frame:
      - latex: body
        at: [0.5, 0.5]
  - frame:
      - latex: other body
        at: [0.5, 0.5]
)") == "");

    // ── errors ──────────────────────────────────────────────────────────────
    expectError(R"(
plot:
  - latex: x
slides:
  - frame:
      - plot
)", "has the name of an item type", __LINE__);

    expectError(R"(
card:
  params: {title: none}
  items: [{latex: x, at: [0.5, 0.5]}]
slides: [{frame: [card]}]
)", "the call would read as that item", __LINE__);

    expectError(R"(
caption: [{latex: c, at: [0.5, 0.5]}]
card:
  params: {caption: none}
  items: [{latex: x, at: [0.5, 0.5]}]
slides: [{frame: [card]}]
)", "has a param named like the group", __LINE__);

    expectError(R"(
card:
  params: {y-range: 1}
  items: [{latex: x, at: [0.5, 0.5]}]
slides: [{frame: [card]}]
)", "must be letters, digits and _", __LINE__);

    expectError(R"(
card:
  params: {id: x}
  items: [{latex: x, at: [0.5, 0.5]}]
slides: [{frame: [card]}]
)", "cannot take a param named \"id\"", __LINE__);

    expectError(R"(
card:
  params: {label: }
  items: [{latex: "${label}", at: [0.5, 0.5]}]
slides: [{frame: [{card: a}]}]
)", "needs \"label:\"", __LINE__);

    expectError(R"(
card: [{latex: "hi ${id}", at: [0.5, 0.5]}]
slides: [{frame: [card]}]
)", "uses $id", __LINE__);

    expectError(R"(
card: [{latex: x, at: [0.5, 0.5]}]
slides: [{frame: [{card: a/b}]}]
)", "may only hold letters, digits, _ and -", __LINE__);

    expectError(R"(
card: [{latex: x, at: [0.5, 0.5]}]
slides: [{frame: [{card: , id: c}]}]
)", "takes its id after the name", __LINE__);

    expectError(R"(
card: [{latex: x, at: [0.5, 0.5]}]
slides: [{frame: [{card: [1, 2]}]}]
)", "is its id, a name", __LINE__);

    expectError(R"(
a: [{latex: x, at: [0.5, 0.5]}, b]
b: [a]
slides: [{frame: [a]}]
)", "uses itself", __LINE__);

    expectError(R"(
a: [{latex: x, at: [0.5, 0.5]}]
b: [{latex: why, at: [0.5, 0.5]}]
slides: [{frame: [{a: , b: }]}]
)", "calls two groups", __LINE__);

    expectError(R"(
reveal: [{latex: x, at: [0.5, 0.5]}, step, {latex: why, at: [0.5, 0.5]}]
template: [reveal]
slides: [{frame: [{latex: z, at: [0.5, 0.5]}]}]
)", "the template uses a group with \"step\"", __LINE__);

    expectError(R"(
card: [{latex: x}]
slides: [{frame: [{stack: [card]}]}]
)", "a stack cannot call a group", __LINE__);

    expectError(R"(
slides: [{frame: [nothing_declared]}]
)", "unknown group or item id \"nothing_declared\"", __LINE__);

    fs::remove_all(dir);
    if (failures == 0)
        std::cout << "all deck group checks passed" << std::endl;
    return failures == 0 ? 0 : 1;
}
