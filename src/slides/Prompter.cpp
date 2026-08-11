// polyscope only links glad on Windows/Linux (see deps/glad/src/CMakeLists.txt);
// macOS uses the native GL framework directly, same as Shader.cpp's own loader
#ifdef __APPLE__
#include <OpenGL/gl3.h>
#else
#include "glad/glad.h"      // must come before any GLFW or GL headers
#endif
#include "GLFW/glfw3.h"
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include "Prompter.h"

#include <fstream>
#include <iostream>
#include <spdlog/spdlog.h>

slope::Prompter::Prompter(std::string script_file) : script_file(script_file) {}

slope::Prompter::~Prompter()
{
    if (!window) return;

    GLFWwindow*   main_win = glfwGetCurrentContext();
    ImGuiContext* main_ctx = ImGui::GetCurrentContext();

    glfwMakeContextCurrent(window);
    ImGui::SetCurrentContext(ctx);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext(ctx);
    glfwDestroyWindow(window);

    glfwMakeContextCurrent(main_win);
    ImGui::SetCurrentContext(main_ctx);
}

void slope::Prompter::initWindow()
{
    GLFWwindow*   main_win = glfwGetCurrentContext();
    ImGuiContext* main_ctx = ImGui::GetCurrentContext();

    window = glfwCreateWindow(900, 600, "slope \xe2\x80\x94 prompter", NULL, main_win);
    if (!window) {
        spdlog::error("[prompter] failed to create window");
        return;
    }
    glfwShowWindow(window);

    glfwMakeContextCurrent(window);

    ctx = ImGui::CreateContext();
    ImGui::SetCurrentContext(ctx);

    ImGui_ImplGlfw_InitForOpenGL(window, false);
    ImGui_ImplOpenGL3_Init("#version 150");

    ImGuiIO& io = ImGui::GetIO();
    io.FontGlobalScale = 2.0f;

    ImGui::StyleColorsDark();
    ImGui::GetStyle().WindowPadding = ImVec2(20, 20);
    ImGui::GetStyle().ItemSpacing   = ImVec2(8, 12);

    glfwMakeContextCurrent(main_win);
    ImGui::SetCurrentContext(main_ctx);
}

void slope::Prompter::render(const std::string& text, TimeStamp fromBegin)
{
    if (!window)
        initWindow();

    if (!window || glfwWindowShouldClose(window)) return;

    GLFWwindow*   main_win = glfwGetCurrentContext();
    ImGuiContext* main_ctx = ImGui::GetCurrentContext();

    glfwMakeContextCurrent(window);
    ImGui::SetCurrentContext(ctx);

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.08f, 0.08f, 1.f));
    ImGui::Begin("##prompter", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoMove       | ImGuiWindowFlags_NoBringToFrontOnFocus);

    auto total_sec = (int)TimeFrom(fromBegin);
    ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.f),
        "%02d:%02d", total_sec / 60, total_sec % 60);
    ImGui::Separator();
    ImGui::Spacing();

    if (!text.empty())
        ImGui::TextWrapped("%s", text.c_str());

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::Render();

    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
    glViewport(0, 0, w, h);
    glClearColor(0.08f, 0.08f, 0.08f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(window);

    glfwMakeContextCurrent(main_win);
    ImGui::SetCurrentContext(main_ctx);
}

void slope::Prompter::write(promptTag tag, TimeStamp fromBegin)
{
    if (!scripts.contains(tag)) {
        erase(fromBegin);
        return;
    }
    render(scripts.at(tag), fromBegin);
}

void slope::Prompter::erase(TimeStamp fromBegin)
{
    render("", fromBegin);
}

void slope::Prompter::loadScript()
{
    std::ifstream script(script_file);
    if (!script.is_open()) {
        spdlog::error("[prompter] cannot open script file: {}", script_file);
        return;
    }
    std::string line;
    while (std::getline(script, line))
    {
        if (current_tag.empty() && line.empty())
            continue;
        if (!line.empty() && line[0] == '[') {
            line.erase(remove(line.begin(), line.end(), '['), line.end());
            line.erase(remove(line.begin(), line.end(), ']'), line.end());
            current_tag = line;
        } else if (!line.empty() && current_tag.empty()) {
            std::cerr << "[prompter] invalid script format: missing [TAG] before text" << std::endl;
        } else {
            if (!scripts.contains(current_tag))
                scripts[current_tag] = "";
            scripts[current_tag] += line + "\n";
        }
    }
}
