#include "game/touch.h"
#include "game/swing.h"
#include "app/app.h"
#include "core/log.h"
#include <cstdlib>

#ifdef SEGAGOLF_SDL_INPUT
#include <SDL3/SDL.h>
#include <imgui.h>

namespace golf::touch {
#ifdef SEGAGOLF_INPUT_SMOKE
void StartInputSmoke(void*);
#endif
namespace {
app::Settings draft;
bool refresh = true;
}

void Window(void* window) {
    MouseWindow(window);
    if (const char* role = std::getenv("SEGAGOLF_ROLE")) {
        const char* player = std::getenv("SEGAGOLF_PLAYER");
        const std::string title = std::string("Sega Golf Club - ") +
            (player && std::string_view(player) != "0" ? std::string("Player ") + player : role);
        auto* handle = static_cast<SDL_Window*>(window);
        SDL_SetWindowTitle(handle, title.c_str());
        if (const char* background = std::getenv("SEGAGOLF_BACKGROUND_SERVER");
            background && std::string_view(background) == "1") {
            SDL_HideWindow(handle);
            LOGR("SegaGolf: network server runs in the background with offscreen Vulkan");
        }
    }
    LOGR("SegaGolf controls: left-click/drag touches the panel; hold right mouse and pull down for backswing, then push up or release to strike; right stick down/return or K/release also swings; Backspace coin; Enter start; F10/R3 settings");
#ifdef SEGAGOLF_INPUT_SMOKE
    StartInputSmoke(window);
#else
    (void)window;
#endif
}

void ToggleMenu() {
    if (const char* background = std::getenv("SEGAGOLF_BACKGROUND_SERVER");
        background && std::string_view(background) == "1") return;
    ReleasePointer();
    golf::swing::Release();
    app::SetGraphicsMenuOpen(!app::GraphicsMenuOpen());
    refresh = true;
}

void Draw(double fps) {
    auto& io = ImGui::GetIO();
    if (!app::GraphicsMenuOpen()) {
        const auto point = ReadCursor();
        if (point.mouse) return;
        // Match the shared renderer's letterboxing (vulkan_present.cpp).
        const float aspect = app::Widescreen() ? 16.0f / 9.0f : 4.0f / 3.0f;
        const float width = std::min(io.DisplaySize.x, io.DisplaySize.y * aspect);
        const float height = width / aspect;
        const ImVec2 at{(io.DisplaySize.x - width) / 2 + point.x * width,
                        (io.DisplaySize.y - height) / 2 + point.y * height};
        const auto color = point.down ? IM_COL32(255, 180, 45, 255) : IM_COL32(50, 235, 255, 255);
        auto* draw = ImGui::GetForegroundDrawList();
        draw->AddCircleFilled(at, 9, IM_COL32(0, 0, 0, 180));
        draw->AddCircle(at, 7, color, 20, 2);
        draw->AddLine({at.x - 12, at.y}, {at.x + 12, at.y}, color);
        draw->AddLine({at.x, at.y - 12}, {at.x, at.y + 12}, color);
        return;
    }
    if (refresh) { draft = app::Get(); refresh = false; }
    bool visible = true;
    ImGui::SetNextWindowSize({std::min(500.0f, std::max(200.0f, io.DisplaySize.x - 24)),
                              std::min(440.0f, std::max(200.0f, io.DisplaySize.y - 24))}, ImGuiCond_Appearing);
    ImGui::SetNextWindowPos({io.DisplaySize.x / 2, io.DisplaySize.y / 2}, ImGuiCond_Appearing, {.5f, .5f});
    if (ImGui::Begin("Sega Golf controls and settings | F10 / R3", &visible, ImGuiWindowFlags_NoCollapse)) {
        ImGui::Text("%.1f FPS", fps);
        ImGui::TextWrapped("Mouse: point directly at a button, then left-click. Hold and drag for touch gestures; release to lift. Controller/keyboard: left stick, D-pad or arrows move; A / Z / Space or right trigger touches. Left trigger / Q / Shift slows movement.");
        ImGui::TextWrapped("Swing: hold the RIGHT mouse button, pull down to raise the club, pause at the desired power, then push up to hit. Releasing also returns the club. Controller: pull the right stick down, then return it. Keyboard: hold K for a full backswing, release to hit.");
        ImGui::TextWrapped("Start / Enter: start. Back / Backspace: insert a coin. F10 / R3 / Escape: resume.");
        ImGui::Separator();
        int display = int(draft.display_mode);
        if (ImGui::Combo("Display", &display, "Fullscreen\0Borderless\0Windowed\0"))
            draft.display_mode = app::DisplayMode(display);
        int size[2] = {int(draft.width), int(draft.height)};
        if (ImGui::InputInt2("Output size (0: desktop)", size)) {
            draft.width = uint32_t(std::clamp(size[0], 0, 7680));
            draft.height = uint32_t(std::clamp(size[1], 0, 4320));
        }
        int scale = int(draft.render_percent);
        if (ImGui::SliderInt("Render scale %", &scale, 25, 200)) draft.render_percent = uint32_t(scale);
        int cap = int(draft.fps_cap);
        if (ImGui::SliderInt("FPS cap (0: unlimited)", &cap, 0, 120)) draft.fps_cap = uint32_t(cap);
        ImGui::Checkbox("VSync", &draft.vsync);
        if (ImGui::Button("Apply")) { app::Get() = draft; app::ApplyDisplay(); app::Save(); }
        ImGui::SameLine();
        if (ImGui::Button("Resume")) visible = false;
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) visible = false;
    }
    ImGui::End();
    if (!visible) ToggleMenu();
}
}
#else
namespace golf::touch {
void Window(void*) {}
void Draw(double) {}
void ToggleMenu() {}
}
#endif
