// "uniforms:" and "textures:" on an "object:" naming a shader. The deck
// declares the inputs of a shader it did not create, and a reload of the
// manifest must not silence the binds that shader's C++ owner set.
#include "slope.h"
#include "slides/deck/DeckLoader.h"
#include "content/screen_primitives/gpu/Shader.h"

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

static void writeDeck(const std::string& body)
{
    std::ofstream f(dir / "deck.yaml");
    f << body;
}

int main()
{
    dir = fs::temp_directory_path() / "slope_deck_shader_test";
    fs::remove_all(dir);
    fs::create_directories(dir);
    Options::ProjectDataPath = dir.string() + "/";
    Options::ProjectViewsPath = dir.string() + "/";

    {
        std::ofstream f(dir / "fx.frag");
        f << "uniform float knob;\nuniform float reveal;\nuniform float cpp_side;\n"
             "void main(){ fragColor = vec4(knob, reveal, cpp_side, 1.0); }\n";
    }

    // the shader belongs to C++, registered rather than written in the deck,
    // and it binds a value of its own
    auto fx = Shader::FromFile("fx.frag");
    fx->bind("cpp_side", [] { return 1.f; });

    DeckLoader deck;
    deck.registerObject("fx", fx);

    writeDeck(R"(
slides:
  - frame:
      - object: fx
        at: [0.5, 0.5]
        uniforms:
          - knob: {type: float, default: 0.25, min: 0, max: 1}
          - reveal
)");
    deck.init(dir / "deck.yaml");

    SlideManager first;
    deck.build(first);

    // the typed one is a parameter named after the object, not after the item,
    // so it keeps its name however many slides show it
    CHECK(Params::components("fx/knob") == 1);
    CHECK(fx->isBound("knob"));
    CHECK(fx->isBound("reveal"));      // a bare name, the shared namespace
    CHECK(fx->isBound("cpp_side"));

    // ── a reload that drops one uniform ─────────────────────────────────────
    writeDeck(R"(
slides:
  - frame:
      - object: fx
        at: [0.5, 0.5]
        uniforms:
          - reveal
)");
    deck.init(dir / "deck.yaml");
    SlideManager second;
    deck.build(second);

    CHECK(!fx->isBound("knob"));       // gone with the manifest entry
    CHECK(fx->isBound("reveal"));
    CHECK(fx->isBound("cpp_side"));    // ...and the owner's bind still stands

    // ── the same object on two slides declares its inputs once ──────────────
    writeDeck(R"(
slides:
  - frame:
      - object: fx
        at: [0.5, 0.5]
        uniforms:
          - reveal
  - frame:
      - object: fx
        at: [0.5, 0.5]
)");
    deck.init(dir / "deck.yaml");
    SlideManager third;
    deck.build(third);
    CHECK(fx->isBound("reveal"));
    CHECK(fx->isBound("cpp_side"));

    // ── an object that is not a shader has no uniforms ──────────────────────
    deck.registerObject("txt", std::static_pointer_cast<Primitive>(Text::Add("hello")));
    writeDeck(R"(
slides:
  - frame:
      - object: txt
        at: [0.5, 0.5]
        uniforms:
          - reveal
)");
    deck.init(dir / "deck.yaml");
    {
        SlideManager fourth;
        bool threw = false;
        try {
            deck.build(fourth);
        } catch (const std::exception&) {
            threw = true;
        }
        CHECK(threw);
    }

    // ── a mistyped placement is an error, not a read past the end ───────────
    writeDeck(R"(
slides:
  - frame:
      - object: fx
        at: [0.5]
)");
    deck.init(dir / "deck.yaml");
    {
        SlideManager fifth;
        bool threw = false;
        try {
            deck.build(fifth);
        } catch (const std::exception&) {
            threw = true;
        }
        CHECK(threw);
    }

    fs::remove_all(dir);
    if (failures == 0)
        std::cout << "all deck shader checks passed" << std::endl;
    return failures == 0 ? 0 : 1;
}
