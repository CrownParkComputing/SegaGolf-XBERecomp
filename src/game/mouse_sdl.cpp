#include "game/touch.h"
#include "game/swing.h"

#ifdef SEGAGOLF_SDL_INPUT
#include <SDL3/SDL.h>
#include "app/app.h"
#include <mutex>

namespace golf::touch {
namespace {
std::mutex mouse_lock;
SDL_WindowID window_id;
float x, y, width = 1, height = 1;
bool focused = false, held = false, swinging = false;

void Submit() {
    SubmitPointer(MapMouse(x, y, width, height, app::Widescreen()), held,
                  focused && !app::GraphicsMenuOpen());
}

bool SDLCALL Watch(void*, SDL_Event* event) {
    std::lock_guard guard(mouse_lock);
    switch (event->type) {
    case SDL_EVENT_MOUSE_MOTION:
        if (event->motion.windowID == window_id) {
            const float aspect = app::Widescreen() ? 16.0f / 9.0f : 4.0f / 3.0f;
            const float game_height = std::min(width / aspect, height);
            if (swinging && game_height > 0 && focused && !app::GraphicsMenuOpen())
                golf::swing::Move(event->motion.yrel / game_height);
            x = event->motion.x; y = event->motion.y;
            Submit();
        }
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (event->button.windowID == window_id) {
            x = event->button.x; y = event->button.y;
            if (event->button.button == SDL_BUTTON_LEFT) {
                held = event->button.down;
                Submit();
            } else if (event->button.button == SDL_BUTTON_RIGHT) {
                if (event->button.down && focused && !app::GraphicsMenuOpen() &&
                    MapMouse(x, y, width, height, app::Widescreen()).inside) {
                    swinging = true;
                    golf::swing::Begin();
                } else if (!event->button.down) {
                    swinging = false;
                    golf::swing::Release();
                }
            }
        }
        break;
    case SDL_EVENT_WINDOW_RESIZED:
        if (event->window.windowID == window_id) {
            width = float(event->window.data1); height = float(event->window.data2);
            Submit();
        }
        break;
    case SDL_EVENT_WINDOW_FOCUS_GAINED:
        if (event->window.windowID == window_id) focused = true;
        break;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
    case SDL_EVENT_WINDOW_MOUSE_LEAVE:
        if (event->window.windowID == window_id) {
            if (event->type == SDL_EVENT_WINDOW_FOCUS_LOST) {
                focused = false;
                swinging = false;
                golf::swing::Release();
            }
            held = false;
            ReleasePointer();
        }
        break;
    default: break;
    }
    return true;
}
}

void MouseWindow(void* window) {
    auto* handle = static_cast<SDL_Window*>(window);
    int w = 1, h = 1;
    SDL_GetWindowSize(handle, &w, &h);
    window_id = SDL_GetWindowID(handle);
    width = float(w); height = float(h);
    focused = (SDL_GetWindowFlags(handle) & SDL_WINDOW_INPUT_FOCUS) != 0;
    SDL_GetMouseState(&x, &y);
    Submit();
    SDL_AddEventWatch(Watch, nullptr);
    SDL_SetWindowRelativeMouseMode(handle, false);
    SDL_ShowCursor();
}
}
#else
namespace golf::touch { void MouseWindow(void*) {} }
#endif
