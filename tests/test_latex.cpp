// the real pdflatex/imagemagick pipeline, stopping short of loadImage()
#include "slope.h"
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

int main()
{
    fs::path tmp = fs::temp_directory_path() / "slope_test_latex";
    std::error_code ec;
    fs::remove_all(tmp, ec);
    fs::create_directories(tmp);

    slope::Options::CachePath = slope::normalizedDir(tmp);
    slope::Options::LogPath = (tmp / "slope.log").string();

    // the non-batched path, used by ensureRendered()
    {
        std::string doc = slope::WriteTexFile("Hello \\LaTeX{} World", false);
        slope::path png = tmp / "single.png";
        try {
            slope::GenerateLatex(png, doc);
        } catch (const std::exception &e) {
            std::cerr << "GenerateLatex threw: " << e.what() << std::endl;
            failures++;
        }
        CHECK(fs::exists(png));
        CHECK(fs::exists(png) && fs::file_size(png) > 0);
        CHECK(fs::exists(slope::BaselinePath(png)));

        // pdflatex/convert intermediates must not leak into the cache dir
        CHECK(!fs::exists(tmp / "single.tex"));
        CHECK(!fs::exists(tmp / "single.pdf"));
        CHECK(!fs::exists(tmp / "single.aux"));
        CHECK(!fs::exists(tmp / "single.log"));
    }

    // a formula compile, exercising the $...$ body used by Formula::Add
    {
        std::string doc = slope::WriteTexFile("x^2 + y^2 = z^2", true);
        slope::path png = tmp / "formula.png";
        try {
            slope::GenerateLatex(png, doc);
        } catch (const std::exception &e) {
            std::cerr << "GenerateLatex (formula) threw: " << e.what() << std::endl;
            failures++;
        }
        CHECK(fs::exists(png));
        double baseline = slope::ReadBaseline(png);
        CHECK(baseline > 0);
    }

    // the batched path, one pdflatex run split back out by name
    {
        slope::path png1 = tmp / "batch1.png", png2 = tmp / "batch2.png";
        std::vector<slope::LatexJob> jobs = {
            {png1, slope::TexBody("A", false), false},
            {png2, slope::TexBody("B", false), false},
        };
        slope::GenerateLatexBatch(jobs);
        CHECK(fs::exists(png1));
        CHECK(fs::exists(png2));
        CHECK(fs::exists(png1) && fs::file_size(png1) > 0);
        CHECK(fs::exists(png2) && fs::file_size(png2) > 0);

        // the splitter maps pages back by index, a mixup shifts every formula
        auto readAll = [](const fs::path &p) {
            std::ifstream f(p, std::ios::binary);
            return std::string(std::istreambuf_iterator<char>(f), {});
        };
        CHECK(readAll(png1) != readAll(png2));
    }

    // an invalid document must fail loudly
    {
        bool threw = false;
        try {
            slope::GenerateLatex(tmp / "broken.png",
                                  "\\documentclass{article}\\begin{document}"
                                  "\\undefinedslopetestcommand"
                                  "\\end{document}\n");
        } catch (const std::exception &) {
            threw = true;
        }
        CHECK(threw);
        CHECK(!fs::exists(tmp / "broken.png"));
    }

    fs::remove_all(tmp, ec);

    if (failures) {
        std::cerr << failures << " check(s) failed" << std::endl;
        return 1;
    }
    std::cout << "latex compilation tests passed" << std::endl;
    return 0;
}
