// array uniforms, uploaded and clamped to the length the shader declares.
// Needs an OpenGL context, and skips when it cannot get one.
#include "slope.h"
#include "content/screen_primitives/gpu/Shader.h"
#include "GLFW/glfw3.h"
#include "imgui.h"

#include <iostream>
#include <vector>

using namespace slope;

// ctest's skip code, see SKIP_RETURN_CODE in tests/CMakeLists.txt
static constexpr int kSkip = 125;

static int failures = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            std::cerr << "CHECK FAILED: " #cond " at " << __FILE__ << ":"    \
                      << __LINE__ << std::endl;                              \
            failures++;                                                      \
        }                                                                    \
    } while (0)

// the shader copies the array into a buffer we can read back on the CPU
static const char* kSource = R"(
layout(std430, binding = 0) buffer Out { float result[]; };
uniform float w[8];
uniform int w_count;
void main() {
    for (int i = 0; i < 8; i++)
        result[i] = w[i];
    result[8] = float(w_count);
    fragColor = vec4(0.0);
}
)";

int main()
{
    if (!glfwInit()) {
        std::cout << "no GLFW, skipping" << std::endl;
        return kSkip;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    GLFWwindow* win = glfwCreateWindow(8, 8, "slope_shader_array_test", nullptr, nullptr);
    if (!win) {
        std::cout << "no OpenGL 4.3 context, skipping" << std::endl;
        glfwTerminate();
        return kSkip;
    }
    glfwMakeContextCurrent(win);

    // the shader reads its on screen rectangle while rendering, so it wants a
    // current ImGui window even when nothing is displayed
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(8, 8);
    io.DeltaTime = 1.f / 60.f;
    unsigned char* pixels = nullptr;
    int fw = 0, fh = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &fw, &fh);
    io.Fonts->TexID = (ImTextureID)1;

    auto fx = Shader::Add(kSource);
    fx->setResolution(1, 1);
    fx->setHidden();
    fx->allocBuffer(0, 9 * sizeof(float));

    TimeObject t;
    StateInSlide sis;
    std::vector<float> back(9, -1.f);

    // one draw of the hidden shader, inside a frame it can read a window from
    auto render = [&] {
        ImGui::NewFrame();
        ImGui::Begin("test");
        fx->draw(t, sis);
        ImGui::End();
        ImGui::EndFrame();
    };

    const std::vector<float> five = {1, 2, 3, 4, 5};
    fx->set("w", five);
    render();
    CHECK(fx->readBuffer(0, back));
    for (int i = 0; i < 5; i++)
        CHECK(back[i] == five[i]);
    // the elements past the upload keep whatever they held, only the count moves
    CHECK(back[8] == 5.f);

    // longer than the shader declared, clamped rather than dropped or crashing
    std::vector<float> twelve(12);
    for (int i = 0; i < 12; i++)
        twelve[i] = float(10 + i);
    fx->set("w", twelve);
    render();
    CHECK(fx->readBuffer(0, back));
    for (int i = 0; i < 8; i++)
        CHECK(back[i] == twelve[i]);
    CHECK(back[8] == 8.f);

    // live values, re-read at every draw
    std::vector<float> live = {7, 7, 7};
    fx->bindArray("w", [&] { return live; });
    render();
    CHECK(fx->readBuffer(0, back));
    CHECK(back[0] == 7.f && back[2] == 7.f);
    CHECK(back[8] == 3.f);

    live = {9, 9};
    render();
    CHECK(fx->readBuffer(0, back));
    CHECK(back[0] == 9.f);
    CHECK(back[8] == 2.f);

    fx.reset();
    ImGui::DestroyContext();
    glfwDestroyWindow(win);
    glfwTerminate();

    if (failures == 0)
        std::cout << "all shader array checks passed" << std::endl;
    return failures == 0 ? 0 : 1;
}
