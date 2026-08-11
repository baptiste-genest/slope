// Exercises LabelAnchor's .pos persistence (Anchor.cpp): the on-disk format
// slide positions are saved to and reloaded from across editing sessions.
#include "slope.h"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

static int failures = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                      \
            std::cerr << "CHECK FAILED: " #cond " at " << __FILE__ << ":"    \
                       << __LINE__ << std::endl;                             \
            failures++;                                                      \
        }                                                                    \
    } while (0)

static bool nearlyEqual(double a, double b) { return std::fabs(a - b) < 1e-9; }

int main()
{
    fs::path tmp = fs::temp_directory_path() / "slope_test_anchor";
    std::error_code ec;
    fs::remove_all(tmp, ec);
    fs::create_directories(tmp);

    slope::Options::ProjectViewsPath = slope::normalizedDir(tmp);

    // constructing a LabelAnchor writes a default .pos the first time a label is seen
    fs::path posfile = tmp / "my_label.pos";
    CHECK(!fs::exists(posfile));
    auto a = slope::LabelAnchor::Add("my_label");
    CHECK(fs::exists(posfile));
    CHECK(nearlyEqual(a->getPos().x(), 0.5));
    CHECK(nearlyEqual(a->getPos().y(), 0.5));
    CHECK(nearlyEqual(a->getScale(), 1.0));

    // a pre-existing .pos file (e.g. from a previous slide session) must be
    // loaded as-is: the constructor writes a default only when the file is
    // missing, and this label has not been read before, so nothing else has
    // populated the in-memory session cache for it yet
    {
        fs::path existing = tmp / "existing_label.pos";
        std::ofstream(existing) << "0.1 0.2 3\n";
        auto b = slope::LabelAnchor::Add("existing_label");
        CHECK(nearlyEqual(b->getPos().x(), 0.1));
        CHECK(nearlyEqual(b->getPos().y(), 0.2));
        CHECK(nearlyEqual(b->getScale(), 3.0));
    }

    // the editor writes new positions to a session cache first, and only
    // saveAllDirty() persists them to disk
    slope::LabelAnchor::writeToSessionAt("my_label", 0.25, 0.75, 2.0);
    CHECK(slope::LabelAnchor::hasDirty());
    {
        std::ifstream f(posfile);
        double x, y, s;
        f >> x >> y >> s;
        CHECK(!nearlyEqual(x, 0.25)); // not flushed to disk yet
    }
    slope::LabelAnchor::saveAllDirty();
    CHECK(!slope::LabelAnchor::hasDirty());
    {
        std::ifstream f(posfile);
        double x, y, s;
        CHECK(bool(f >> x >> y >> s));
        CHECK(nearlyEqual(x, 0.25));
        CHECK(nearlyEqual(y, 0.75));
        CHECK(nearlyEqual(s, 2.0));
    }

    // a label whose .pos is missing/corrupt falls back to sane defaults
    // instead of throwing, so a broken checkout never crashes a presentation
    {
        std::ofstream(tmp / "orphan.pos") << "not a number\n";
        auto c = std::make_shared<slope::LabelAnchor>("orphan");
        CHECK(nearlyEqual(c->getPos().x(), 0.5));
        CHECK(nearlyEqual(c->getPos().y(), 0.5));
    }

    fs::remove_all(tmp, ec);

    if (failures) {
        std::cerr << failures << " check(s) failed" << std::endl;
        return 1;
    }
    std::cout << "anchor .pos persistence tests passed" << std::endl;
    return 0;
}
