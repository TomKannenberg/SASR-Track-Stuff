#pragma once

#include <cstdint>

struct GLFWwindow;

namespace SeEditor::Graphics::ImGui {

class ImGuiController
{
public:
    ImGuiController(int width, int height);
    ~ImGuiController();

    ImGuiController(ImGuiController const&) = delete;
    ImGuiController(ImGuiController&&) = delete;
    ImGuiController& operator=(ImGuiController const&) = delete;
    ImGuiController& operator=(ImGuiController&&) = delete;

    void WindowResized(int width, int height);
    void NewFrame();
    void Render();
    void Dispose();
    void SetPerFrameImGuiData(float deltaSeconds);
    void PollEvents();
    void SwapBuffers();
    [[nodiscard]] bool ShouldClose() const noexcept;
    [[nodiscard]] GLFWwindow* Window() const noexcept { return _window; }

private:
    void InitializeWindow();
    void InitializeImGui();
    void Cleanup();

    GLFWwindow* _window = nullptr;
    int _windowWidth = 0;
    int _windowHeight = 0;
    bool _frameBegun = false;
    bool _glfwInitialized = false;
};

} // namespace SeEditor::Graphics::ImGui
