#include "ImGuiController.hpp"

#include "ImGuiControls.hpp"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include <glad/glad.h>
#include <imgui.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>

namespace {

[[nodiscard]] constexpr const char* GetGlslVersion()
{
#if defined(__APPLE__)
    return "#version 150";
#else
    return "#version 330 core";
#endif
}

void SetupImGuiContext(GLFWwindow* window)
{
    IMGUI_CHECKVERSION();
    ::ImGui::CreateContext();
    ::ImGuiIO& io = ::ImGui::GetIO();
    io.ConfigFlags |= ::ImGuiConfigFlags_NavEnableKeyboard;
    ::ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(GetGlslVersion());
    SeEditor::Graphics::ImGui::ImGuiControls::Initialize();
}

} // namespace

namespace SeEditor::Graphics::ImGui {

namespace {

void GlfwErrorCallback(int error, const char* description)
{
    std::cerr << "[ImGuiController] GLFW error (" << error << "): " << description << std::endl;
}

} // namespace

ImGuiController::ImGuiController(int width, int height)
    : _windowWidth(width)
    , _windowHeight(height)
{
    InitializeWindow();
    InitializeImGui();
    std::cout << "[ImGuiController] created (" << _windowWidth << "x" << _windowHeight << ")" << std::endl;
}

ImGuiController::~ImGuiController()
{
    Dispose();
}

void ImGuiController::InitializeWindow()
{
    if (_glfwInitialized)
        return;

    if (!glfwInit())
        throw std::runtime_error("Failed to initialize GLFW");

    _glfwInitialized = true;
    glfwSetErrorCallback(GlfwErrorCallback);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif

    _window = glfwCreateWindow(_windowWidth, _windowHeight, "CppSLib Editor", nullptr, nullptr);
    if (!_window)
        throw std::runtime_error("Failed to create GLFW window");

    glfwMakeContextCurrent(_window);
    glfwSwapInterval(0);

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
        throw std::runtime_error("Failed to initialize OpenGL loader");

    glViewport(0, 0, _windowWidth, _windowHeight);
    glfwSetWindowUserPointer(_window, this);
    glfwSetFramebufferSizeCallback(_window, [](GLFWwindow* window, int width, int height) {
        if (auto* controller = reinterpret_cast<ImGuiController*>(glfwGetWindowUserPointer(window)))
            controller->WindowResized(width, height);
    });
}

void ImGuiController::InitializeImGui()
{
    SetupImGuiContext(_window);
}

void ImGuiController::WindowResized(int width, int height)
{
    if (width == 0 || height == 0)
        return;

    _windowWidth = width;
    _windowHeight = height;
    glViewport(0, 0, width, height);
}

void ImGuiController::NewFrame()
{
    if (_frameBegun)
        return;

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ::ImGui::NewFrame();
    _frameBegun = true;
}

void ImGuiController::Render()
{
    if (!_frameBegun)
        return;

    ::ImGui::Render();
    // The scene renderer clears the framebuffer; avoid wiping it again here.
    glDisable(GL_DEPTH_TEST);
    ImGui_ImplOpenGL3_RenderDrawData(::ImGui::GetDrawData());
    _frameBegun = false;
}

void ImGuiController::SetPerFrameImGuiData(float deltaSeconds)
{
    ::ImGuiIO& io = ::ImGui::GetIO();
    io.DeltaTime = deltaSeconds > 0.0f ? deltaSeconds : (io.DeltaTime > 0.0f ? io.DeltaTime : 1.0f / 60.0f);
    io.DisplaySize = ::ImVec2(static_cast<float>(_windowWidth), static_cast<float>(_windowHeight));
    io.DisplayFramebufferScale = ::ImVec2(1.0f, 1.0f);
}

void ImGuiController::PollEvents()
{
    glfwPollEvents();
}

void ImGuiController::SwapBuffers()
{
    if (_window)
        glfwSwapBuffers(_window);
}

bool ImGuiController::ShouldClose() const noexcept
{
    return _window == nullptr || glfwWindowShouldClose(_window);
}

void ImGuiController::Dispose()
{
    Cleanup();
}

void ImGuiController::Cleanup()
{
    if (::ImGui::GetCurrentContext())
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ::ImGui::DestroyContext();
    }

    if (_window)
    {
        glfwDestroyWindow(_window);
        _window = nullptr;
    }

    if (_glfwInitialized)
    {
        glfwTerminate();
        _glfwInitialized = false;
    }
}

} // namespace SeEditor::Graphics::ImGui
