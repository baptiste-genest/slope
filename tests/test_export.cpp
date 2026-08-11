// --export end to end, the only test here needing an OpenGL context
#include "slope.h"
#include "extern/stb_image.h"
#include "GLFW/glfw3.h"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace slope;

namespace fs = std::filesystem;

// ctest's skip code, see SKIP_RETURN_CODE in tests/CMakeLists.txt
static constexpr int kSkip = 125;

#if defined(__APPLE__) || defined(_WIN32)
// probe for a context first, polyscope aborts on a failed window rather
// than throwing, and only Linux has the EGL fallback
static bool canCreateGLContext()
{
    if (!glfwInit())
        return false;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    GLFWwindow* probe = glfwCreateWindow(4, 4, "slope_export_probe", nullptr, nullptr);
    bool ok = probe != nullptr;
    if (probe)
        glfwDestroyWindow(probe);
    return ok;
}
#endif

// normalized RMSE tolerance, no two GPUs rasterize a slide identically
// picked without macOS/Windows numbers, expect to retune it
static constexpr double kMaxRMSE = 0.10;

static std::string quote(const std::string& s) { return "\"" + s + "\""; }

// compare exits non-zero on any difference, so read the figure it prints
// returns -1 when the run or the parse failed
static double compareRMSE(const fs::path& compare_bin, const std::string& a, const std::string& b,
                           const fs::path& stderr_capture)
{
    std::string cmd = quote(compare_bin.string()) + " -metric RMSE " + quote(a) + " " + quote(b)
                     + " null: 2> " + quote(stderr_capture.string());
    if (std::system(cmd.c_str())) { /* exit code is meaningless for RMSE, see above */ }

    std::ifstream f(stderr_capture);
    std::string line;
    std::getline(f, line);
    auto open = line.find('(');
    auto close = line.find(')', open);
    if (open == std::string::npos || close == std::string::npos)
        return -1;
    try {
        return std::stod(line.substr(open + 1, close - open - 1));
    } catch (...) {
        return -1;
    }
}

static int failures = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                      \
            std::cerr << "CHECK FAILED: " #cond " at " << __FILE__ << ":"    \
                       << __LINE__ << std::endl;                             \
            failures++;                                                      \
        }                                                                    \
    } while (0)

Slideshow show;

namespace {

// same construction as the stress example, just fewer subdivisions
void makeSphere(vecs& V, Faces& F, double r, int NU = 12, int NV = 6)
{
    for (int j = 0; j <= NV; ++j)
        for (int i = 0; i < NU; ++i) {
            double u = 2.0 * M_PI * i / NU, v = M_PI * j / NV;
            V.push_back(r * vec(std::sin(v) * std::cos(u), std::cos(v), std::sin(v) * std::sin(u)));
        }
    auto id = [&](int i, int j) { return size_t((j % (NV + 1)) * NU + (i % NU)); };
    for (int j = 0; j < NV; ++j)
        for (int i = 0; i < NU; ++i)
            F.push_back({id(i, j), id(i + 1, j), id(i + 1, j + 1), id(i, j + 1)});
}

vec2 scatter(int i, int n)
{
    double a = 2.4 * i, r = 0.42 * std::sqrt(double(i) / std::max(1, n));
    return vec2(0.5 + r * std::cos(a), 0.5 + r * std::sin(a));
}

} // namespace

int main()
{
#if defined(__APPLE__) || defined(_WIN32)
    if (!canCreateGLContext()) {
        std::cout << "no usable OpenGL context available here (no EGL-style headless "
                     "fallback on this platform) -- skipping" << std::endl;
        return kSkip;
    }
#endif

    fs::path project = fs::temp_directory_path() / "slope_test_export";
    std::error_code ec;
    fs::remove_all(project, ec);
    fs::create_directories(project);

    const int W = 320, H = 180; // small on purpose : this is a correctness check, not a render benchmark

    // CachePath comes from argv[0]'s parent, so it must be a real path
    std::vector<std::string> arg_storage = {
        (project / "test_export").string(), "--project_path", project.string(),
        "--export", "--resolution", std::to_string(W) + "x" + std::to_string(H)
    };
    std::vector<char*> argv;
    for (auto& a : arg_storage) argv.push_back(a.data());

    show.init("export_test", (int)argv.size(), argv.data());
    if (show.helpWanted()) {
        std::cerr << "CLI parse failed" << std::endl;
        return 1;
    }

    const int N = 6; // primitives per slide

    // text, filled shapes, boxes
    show << Title("Text, shapes, boxes")->at(TOP);
    for (int i = 0; i < N; ++i)
        show << Text::Add("slope " + std::to_string(i))->at(scatter(i, N));
    for (int i = 0; i < N; ++i) {
        auto c = Shape2D::Circle(vec2(0.5, 0.5), 0.03, 32);
        c->style.filled = true;
        show << c->at(scatter(i, N));
    }
    for (int i = 0; i < N; ++i)
        show << FixedBox::Add(vec2(0.05, 0.03))->at(scatter(i, N));

    // label anchors, the .pos files test_anchor.cpp covers directly
    show << newFrame << Title("Label anchors")->at(TOP);
    for (int i = 0; i < N; ++i)
        show << Text::Add("label " + std::to_string(i))->at("export_test_" + std::to_string(i));

    // latex through the primitive layer, not GenerateLatex() directly
    show << newFrame << Title("LaTeX")->at(TOP);
    for (int i = 0; i < N; ++i)
        show << Formula::Add("\\alpha_{" + std::to_string(i) + "} = \\int_0^1 x^"
                              + std::to_string(i) + " dx")->at(scatter(i, N));

    // polyscope meshes, the translucent one exercises depth peeling
    {
        vecs V; Faces F;
        makeSphere(V, F, 0.3);
        show << newFrame << Title("Meshes")->at(TOP);
        for (int i = 0; i < N; ++i)
            show << Mesh::Add(V, F, true)->at(vec(0.6 * (i % 3) - 0.6, 0.6 * (i / 3) - 0.6, 0));
        show << newFrame << Title("Translucent meshes")->at(TOP);
        for (int i = 0; i < N; ++i)
            show << Mesh::Add(V, F, true)->at(vec(0.4 * (i % 3) - 0.4, 0.4 * (i / 3) - 0.4, 0), 0.4);
    }

    // a fragment shader, rendered on the GPU rather than placed
    show << newFrame << Title("Shader")->at(TOP);
    show << Shader::Add(R"(
        void main() {
            vec2 uv = gl_FragCoord.xy / iResolution;
            fragColor = vec4(uv.x, uv.y, 0.5 + 0.5*sin(iTime), 1.);
        })")->at(vec2(0.5, 0.5));

    show.run(); // ExportMode routes this to exportPDF()

    // ── verification ────────────────────────────────────────────────────
    const int slide_count = 6;
    for (int i = 0; i < slide_count; ++i) {
        fs::path png = fs::temp_directory_path() / ("slope_export_slide_" + std::to_string(i) + ".png");
        CHECK(fs::exists(png));
        if (!fs::exists(png))
            continue;
        CHECK(fs::file_size(png) > 0);

        int w, h, channels;
        unsigned char* pixels = stbi_load(png.string().c_str(), &w, &h, &channels, 3);
        CHECK(pixels != nullptr);
        if (!pixels)
            continue;
        CHECK(w == W);
        CHECK(h == H);

        // a failed render is a uniform color, a real slide never is
        bool uniform = true;
        for (int p = 3; p < w * h * 3 && uniform; p += 3)
            if (pixels[p] != pixels[0] || pixels[p + 1] != pixels[1] || pixels[p + 2] != pixels[2])
                uniform = false;
        CHECK(!uniform);
        stbi_image_free(pixels);
    }

    fs::path pdf = project / "export_test.pdf";
    CHECK(fs::exists(pdf));
    CHECK(fs::exists(pdf) && fs::file_size(pdf) > 0);

    // regenerate the committed reference with:
    //   SLOPE_EXPORT_SAVE_REFERENCE=tests/export_reference.pdf ./build/tests/test_export
    if (const char* save_to = std::getenv("SLOPE_EXPORT_SAVE_REFERENCE")) {
        std::error_code cec;
        fs::copy_file(pdf, save_to, fs::copy_options::overwrite_existing, cec);
        if (cec) {
            std::cerr << "could not save reference to " << save_to << ": " << cec.message() << std::endl;
            failures++;
        } else {
            std::cout << "reference saved to " << save_to << std::endl;
        }
    } else if (fs::exists(pdf)) {
        // each page against the reference, tolerant of per-pixel noise
        fs::path reference = fs::path(SLOPE_TEST_SOURCE_DIR) / "export_reference.pdf";
        fs::path convert_bin(Options::PathToCONVERT);
        fs::path compare_bin = convert_bin.parent_path() / "compare";
#ifdef _WIN32
        compare_bin += ".exe";
#endif
        CHECK(fs::exists(reference));
        CHECK(fs::exists(compare_bin));
        if (fs::exists(reference) && fs::exists(compare_bin)) {
            for (int i = 0; i < slide_count; ++i) {
                fs::path diff_out = fs::temp_directory_path()
                                   / ("slope_export_diff_" + std::to_string(i) + ".txt");
                double rmse = compareRMSE(compare_bin,
                                          reference.string() + "[" + std::to_string(i) + "]",
                                          pdf.string() + "[" + std::to_string(i) + "]",
                                          diff_out);
                CHECK(rmse >= 0);
                if (rmse >= 0) {
                    if (rmse > kMaxRMSE)
                        std::cerr << "slide " << i << " differs from the reference by RMSE "
                                  << rmse << " (max " << kMaxRMSE << ")" << std::endl;
                    CHECK(rmse <= kMaxRMSE);
                }
                std::error_code dec;
                fs::remove(diff_out, dec);
            }
        }
    }

    fs::remove_all(project, ec);

    if (failures) {
        std::cerr << failures << " check(s) failed" << std::endl;
        return 1;
    }
    std::cout << "export pipeline tests passed" << std::endl;
    return 0;
}
