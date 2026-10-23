// Opt-in native validation build only. Send real SDL keyboard events through
// the host's normal input path; do not write guest input or state variables.
#include <SDL3/SDL.h>
#include "core/log.h"
#include <cstdlib>
#include <cstring>
#include <atomic>

namespace golf::touch {
namespace {
struct Step {
    uint32_t ms; SDL_Scancode key; bool down; float x = -1, y = -1;
    uint8_t button = SDL_BUTTON_LEFT; float dy = 0;
};
constexpr Step steps[] = {
    {100000, SDL_SCANCODE_UNKNOWN, true},
    {101000, SDL_SCANCODE_BACKSPACE, true}, {101500, SDL_SCANCODE_BACKSPACE, false},
    {103000, SDL_SCANCODE_RETURN, true}, {103500, SDL_SCANCODE_RETURN, false},
    {110000, SDL_SCANCODE_UNKNOWN, true, 400, 300}, {111000, SDL_SCANCODE_UNKNOWN, false, 400, 300},
    {117000, SDL_SCANCODE_UNKNOWN, true, 150, 300}, {118000, SDL_SCANCODE_UNKNOWN, false, 150, 300},
    {125000, SDL_SCANCODE_F10, true}, {125100, SDL_SCANCODE_F10, false},
    {128000, SDL_SCANCODE_ESCAPE, true}, {128100, SDL_SCANCODE_ESCAPE, false},
    {155000, SDL_SCANCODE_F3, true}, {155100, SDL_SCANCODE_F3, false},
    {225000, SDL_SCANCODE_F3, true}, {225100, SDL_SCANCODE_F3, false},
    {240000, SDL_SCANCODE_UNKNOWN, true, 730, 455}, {241000, SDL_SCANCODE_UNKNOWN, false, 730, 455},
    {260000, SDL_SCANCODE_F3, true}, {260100, SDL_SCANCODE_F3, false},
    {270000, SDL_SCANCODE_UNKNOWN, true, 400, 250, SDL_BUTTON_RIGHT},
    {270100, SDL_SCANCODE_UNKNOWN, true, 400, 292, 0, 42},
    {270200, SDL_SCANCODE_UNKNOWN, true, 400, 334, 0, 42},
    {270300, SDL_SCANCODE_UNKNOWN, true, 400, 376, 0, 42},
    {270400, SDL_SCANCODE_UNKNOWN, true, 400, 418, 0, 42},
    {270500, SDL_SCANCODE_UNKNOWN, true, 400, 460, 0, 42},
    {272000, SDL_SCANCODE_F3, true}, {272100, SDL_SCANCODE_F3, false},
    {274000, SDL_SCANCODE_UNKNOWN, true, 400, 418, 0, -42},
    {274025, SDL_SCANCODE_UNKNOWN, true, 400, 376, 0, -42},
    {274050, SDL_SCANCODE_UNKNOWN, true, 400, 334, 0, -42},
    {274075, SDL_SCANCODE_UNKNOWN, true, 400, 292, 0, -42},
    {274100, SDL_SCANCODE_UNKNOWN, true, 400, 250, 0, -42},
    {274200, SDL_SCANCODE_UNKNOWN, false, 400, 250, SDL_BUTTON_RIGHT},
    {276000, SDL_SCANCODE_F3, true}, {276100, SDL_SCANCODE_F3, false},
    {290000, SDL_SCANCODE_F3, true}, {290100, SDL_SCANCODE_F3, false},
};
SDL_WindowID window_id;
std::atomic<int> window_width{800}, window_height{600};
uint64_t start;
unsigned next = 0;
bool SDLCALL SizeWatch(void*, SDL_Event* event) {
    if (event->type == SDL_EVENT_WINDOW_RESIZED && event->window.windowID == window_id) {
        window_width = event->window.data1;
        window_height = event->window.data2;
    }
    return true;
}
Uint32 SDLCALL Tick(void*, SDL_TimerID, Uint32) {
    while (next < std::size(steps) && SDL_GetTicks() - start >= steps[next].ms) {
        const auto step = steps[next++];
        SDL_Event event{};
        if (step.x >= 0) {
            const float w = float(window_width.load()), h = float(window_height.load());
            const float game_w = std::min(w, h * 4 / 3), game_h = game_w * 3 / 4;
            if (step.button) {
                event.type = step.down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
                event.button.windowID = window_id;
                event.button.button = step.button;
                event.button.down = step.down;
                event.button.x = (w - game_w) / 2 + step.x / 800 * game_w;
                event.button.y = (h - game_h) / 2 + step.y / 600 * game_h;
            } else {
                event.type = SDL_EVENT_MOUSE_MOTION;
                event.motion.windowID = window_id;
                event.motion.x = (w - game_w) / 2 + step.x / 800 * game_w;
                event.motion.y = (h - game_h) / 2 + step.y / 600 * game_h;
                event.motion.yrel = step.dy / 600 * game_h;
            }
        } else if (step.key == SDL_SCANCODE_UNKNOWN) {
            event.type = SDL_EVENT_WINDOW_FOCUS_GAINED;
            event.window.windowID = window_id;
        } else {
            event.type = step.down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
            event.key.windowID = window_id;
            event.key.scancode = step.key;
            event.key.key = SDL_GetKeyFromScancode(step.key, SDL_KMOD_NONE, false);
            event.key.down = step.down;
        }
        SDL_PushEvent(&event);
        LOGR("SegaGolf SDL input smoke: scancode %u %s", unsigned(step.key), step.down ? "down" : "up");
    }
    return next < std::size(steps) ? 20 : 0;
}
}
void StartInputSmoke(void* window) {
    if (const char* role = std::getenv("SEGAGOLF_ROLE"); role && !std::strcmp(role, "server")) return;
    window_id = SDL_GetWindowID(static_cast<SDL_Window*>(window));
    int w, h;
    SDL_GetWindowSize(static_cast<SDL_Window*>(window), &w, &h);
    window_width = w; window_height = h;
    SDL_AddEventWatch(SizeWatch, nullptr);
    start = SDL_GetTicks();
    SDL_AddTimer(20, Tick, nullptr);
}
}
