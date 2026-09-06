// the label slope offers when an item has no name of its own
#include "content/screen_primitives/layout/Anchor.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <set>

static int failures = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            std::cerr << "CHECK FAILED: " #cond " at " << __FILE__ << ":"    \
                      << __LINE__ << std::endl;                              \
            failures++;                                                      \
        }                                                                    \
    } while (0)

using namespace slope;

static bool wellFormed(const std::string& l)
{
    const std::string cons = "bdfgklmnprstvz";
    const std::string vows = "aeiou";
    if (l.size() != 5)
        return false;
    for (size_t i = 0; i < l.size(); i++) {
        const std::string& want = (i % 2 == 0) ? cons : vows;
        if (want.find(l[i]) == std::string::npos)
            return false;
    }
    return true;
}

int main()
{
    auto dir = std::filesystem::temp_directory_path() / "slope_suggest_test";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    Options::ProjectViewsPath = dir.string() + "/";

    // every suggestion reads as consonant-vowel pairs, so it can be said aloud
    for (int i = 0; i < 500; i++) {
        std::string l = LabelAnchor::suggestLabel();
        CHECK(wellFormed(l));
        if (!wellFormed(l)) {
            std::cerr << "  got \"" << l << "\"\n";
            break;
        }
    }

    // a name already on disk is never offered again
    std::set<std::string> seen;
    for (int i = 0; i < 200; i++) {
        std::string l = LabelAnchor::suggestLabel();
        CHECK(!l.empty());
        CHECK(seen.insert(l).second);       // never the same twice
        std::ofstream(dir / (l + ".pos")) << "0.5 0.5 1 0 1\n";
    }

    // a name the deck already gives to an item is never offered
    {
        std::set<std::string> taken;
        for (int i = 0; i < 300; i++)
            taken.insert(LabelAnchor::suggestLabel());
        LabelAnchor::reserveNames(taken);
        for (int i = 0; i < 300; i++) {
            std::string l = LabelAnchor::suggestLabel();
            CHECK(!l.empty());
            CHECK(!taken.count(l));
        }
        LabelAnchor::reserveNames({});
    }

    std::filesystem::remove_all(dir);
    std::cout << (failures ? std::to_string(failures) + " FAILED\n" : "all passed\n");
    return failures ? 1 : 0;
}
