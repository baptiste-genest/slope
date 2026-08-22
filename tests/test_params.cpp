// driving a parameter from C++, the counterpart of reading it
#include "slope.h"

#include <cmath>
#include <filesystem>
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

#define CHECK_NEAR(a, b) CHECK(std::abs((a) - (b)) < 1e-6)

int main()
{
    fs::path dir = fs::temp_directory_path() / "slope_params_test";
    fs::remove_all(dir);
    fs::create_directories(dir);
    Options::ProjectViewsPath = dir.string() + "/";

    auto amp = Params::Add("test/amplitude", 0.2, 0., 1.);
    CHECK_NEAR((scalar)amp, 0.2);

    CHECK(Params::write("test/amplitude", scalar(0.7)));
    CHECK_NEAR((scalar)amp, 0.7);

    // out of the declared bounds, the panel could not reach that value either
    CHECK(Params::write("test/amplitude", scalar(4)));
    CHECK_NEAR((scalar)amp, 1.);

    // the handle writes without a lookup, same clamping
    amp.set(-3);
    CHECK_NEAR((scalar)amp, 0.);

    // a write is a drive and not an edit, nothing to save
    CHECK(!Params::hasDirty());

    CHECK(!Params::write("test/nothing_of_that_name", scalar(1)));

    auto handle = Params::AddVec("test/handle", vec(0, 1, 0));
    CHECK(Params::write("test/handle", vec(1, 2, 3)));
    CHECK_NEAR((*handle)(1), 2.);

    // fewer components than the parameter has is refused, not padded
    const scalar two[2] = {5, 6};
    CHECK(Params::write("test/handle", two, 2) == 0);
    CHECK_NEAR((*handle)(0), 1.);

    // read and write agree on how wide the parameter is
    scalar out[4] = {0, 0, 0, 0};
    CHECK(Params::read("test/handle", out) == 3);
    CHECK(Params::write("test/handle", out, 3) == 3);

    auto n = Params::AddDir("test/normal", vec(0, 0, 1));
    Params::write("test/normal", vec(0, 5, 0));
    CHECK_NEAR((*n).norm(), 1.);
    CHECK_NEAR((*n)(1), 1.);

    auto steps = Params::AddInt("test/steps", 4, 0, 64);
    CHECK(Params::write("test/steps", scalar(7.6)));
    CHECK((int)steps == 8);

    auto tint = Params::AddColor("test/tint", RGBA(0.f, 0.f, 0.f, 1.f));
    CHECK(Params::write("test/tint", RGBA(0.25f, 0.5f, 0.75f, 1.f)));
    CHECK_NEAR((*tint).Value.y, 0.5);

    CHECK(!Params::hasDirty());

    // visibility is a property of the parameter, readable back by name
    CHECK(Params::getVisible("test/handle") == Params::Visible::None);
    handle.show(Params::Visible::Handle);
    CHECK(Params::getVisible("test/handle") == Params::Visible::Handle);
    Params::setVisible("test/handle", Params::Visible::Both);
    CHECK(Params::getVisible("test/handle") == Params::Visible::Both);
    CHECK(Params::parseVisible("panel") == Params::Visible::Panel);
    bool threw = false;
    try { Params::parseVisible("sometimes"); } catch (const std::exception&) { threw = true; }
    CHECK(threw);
    // showing a parameter is not editing it either
    CHECK(!Params::hasDirty());

    fs::remove_all(dir);
    if (failures == 0)
        std::cout << "all params checks passed" << std::endl;
    return failures == 0 ? 0 : 1;
}
