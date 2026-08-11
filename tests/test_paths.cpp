// every path parseCLI() sets up must exist on disk and be writable
#include "slope.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

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

static bool isWritableDir(const fs::path& p)
{
    std::error_code ec;
    if (!fs::is_directory(p, ec))
        return false;
    fs::path probe = p / ".slope_write_probe";
    std::ofstream f(probe);
    bool ok = f.is_open();
    f.close();
    fs::remove(probe, ec);
    return ok;
}

int main()
{
    fs::path project = fs::temp_directory_path() / "slope_test_paths";
    std::error_code ec;
    fs::remove_all(project, ec);
    // not pre-created, a brand-new project path is the reported case

    std::vector<std::string> arg_storage = {
        (project / "test_paths").string(), "--project_path", project.string()
    };
    std::vector<char*> argv;
    for (auto& a : arg_storage) argv.push_back(a.data());

    int rc = slope::parseCLI((int)argv.size(), argv.data());
    CHECK(rc == 0);

    CHECK(isWritableDir(slope::Options::ProjectPath));
    CHECK(isWritableDir(slope::Options::CachePath));
    CHECK(isWritableDir(slope::Options::ProjectViewsPath));

    fs::remove_all(project, ec);

    if (failures) {
        std::cerr << failures << " check(s) failed" << std::endl;
        return 1;
    }
    std::cout << "project path tests passed" << std::endl;
    return 0;
}
