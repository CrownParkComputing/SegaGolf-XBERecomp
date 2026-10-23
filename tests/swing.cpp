#include "game/swing.h"
#include "game/touch.h"
#include "kernel/kernel.h"
#include <SDL3/SDL.h>
#include <cassert>
#include <cstdio>
#include <vector>

X86_EXTERN_FUNC(sub_0014B210);
X86_EXTERN_FUNC(sub_00082CA0);
X86_EXTERN_FUNC(sub_000836B0);
namespace kernel { uint8_t* g_base; }
namespace {
std::vector<uint8_t> memory(0xA00000);
constexpr uint32_t context = 0x6777AC, report = 0x900000;
unsigned strikes = 0, touches = 0;
bool menu = false;
}
namespace app {
bool Widescreen() { return false; }
bool GraphicsMenuOpen() { return menu; }
}
namespace golf::touch {
void SubmitPointer(Pointer, bool down, bool) { if (down) ++touches; }
void ReleasePointer() {}
}
// Only hardware node lookup and the cumulative swing counter are replaced. The
// analog byte conversion, input decoding and swing filter are original code.
X86_FUNC(sub_0018ADD0) { kernel::Return(ctx, 0x985E69, 0); }
X86_FUNC(sub_0018AD50) { kernel::Return(ctx, 0x98626C, 0); }
X86_FUNC(sub_0001AD40) { ++strikes; kernel::Return(ctx, 0, 0); }

namespace {
void Frame(uint16_t position) {
    // Channel 3 in the cabinet's wire report. Other axes deliberately differ
    // to detect accidentally reading a cursor axis or swapping the bytes.
    for (unsigned i = 0; i < 4; ++i) {
        const uint16_t value = i == 2 ? position : 0x1234 + i;
        memory[report + 2 * i] = value >> 8;
        memory[report + 2 * i + 1] = value;
    }
    X86Context ctx{};
    ctx.esp = 0x1000;
    kernel::W32(0x1004, 1); kernel::W32(0x1008, 0x2000);
    kernel::W32(0x100C, 0); kernel::W32(0x1010, 0);
    sub_0014B210(ctx, memory.data());
    assert(ctx.esp == 0x1014 && ctx.eax == 0);
    // Original 0x834A0 copies this returned host-order array to record+0xA.
    std::memcpy(memory.data() + 0x677726, kernel::HostPtr(kernel::R32(0x2000)), 16);
    ctx = {}; ctx.esp = 0x1000; ctx.esi = context; ctx.ecx = 0;
    sub_00082CA0(ctx, memory.data());
    assert(ctx.esp == 0x1004 && kernel::R16(context + 0xB4) == position);
    ctx = {}; ctx.esp = 0x1000; ctx.ecx = context;
    sub_000836B0(ctx, memory.data());
    assert(ctx.esp == 0x1004);
}
void Mouse(SDL_Window* window, uint32_t type, float y, float relative = 0) {
    SDL_Event event{};
    event.type = type;
    if (type == SDL_EVENT_MOUSE_MOTION) {
        event.motion.windowID = SDL_GetWindowID(window);
        event.motion.x = 400; event.motion.y = y; event.motion.yrel = relative;
    } else if (type == SDL_EVENT_MOUSE_BUTTON_DOWN || type == SDL_EVENT_MOUSE_BUTTON_UP) {
        event.button.windowID = SDL_GetWindowID(window);
        event.button.button = SDL_BUTTON_RIGHT;
        event.button.down = type == SDL_EVENT_MOUSE_BUTTON_DOWN;
        event.button.x = 400; event.button.y = y;
    } else event.window.windowID = SDL_GetWindowID(window);
    assert(SDL_PushEvent(&event));
}
}
int main() {
    using namespace golf::swing;
    kernel::g_base = memory.data();
    memory[0x985E69] = 1;
    kernel::W32(0x98626C + 0xE0 + 0xB4, report);
    memory[0x98626C + 0xE0 + 0xCF] = 4;
    kernel::W32(0x678A68, 15000); kernel::W32(0x678A6C, 8000); kernel::W32(0x678A70, 45000);
    kernel::G<float>(0x231F90) = 100; kernel::G<float>(0x232010) = 1500;
    input::Pad pad;
    Control control;
    assert(control.Read(pad, true) == 0);
    pad.ry = -1000; assert(control.Read(pad, true) == 0);
    pad.ry = -32768; assert(control.Read(pad, true) == kFullBackswing);
    assert(control.Read(pad, false) == 0);
    pad.ry = 0;
    assert(SDL_Init(SDL_INIT_VIDEO));
    auto* window = SDL_CreateWindow("Swing test", 800, 600, 0);
    assert(window);
    golf::touch::MouseWindow(window);
    Mouse(window, SDL_EVENT_WINDOW_FOCUS_GAINED, 0);
    for (unsigned i = 0; i < 20; ++i) Frame(Read(pad, true));
    assert(strikes == 0 && kernel::R16(context + 0x204) == 0);
    Mouse(window, SDL_EVENT_MOUSE_BUTTON_DOWN, 250);
    for (unsigned i = 0; i < 30; ++i) {
        Mouse(window, SDL_EVENT_MOUSE_MOTION, 250 + (i + 1) * 5, 5);
        Frame(Read(pad, true));
    }
    const auto backswing = Read(pad, true);
    assert(backswing > 30000 && backswing < 33000);
    for (unsigned i = 0; i < 20; ++i) Frame(Read(pad, true));
    assert(strikes == 0 && kernel::G<uint8_t>(context + 0x214));
    assert(kernel::R16(context + 0x20E) == backswing);
    for (unsigned i = 0; i < 6; ++i) {
        Mouse(window, SDL_EVENT_MOUSE_MOTION, 400 - (i + 1) * 25, -25);
        Frame(Read(pad, true));
    }
    assert(Read(pad, true) == 0 && strikes == 1);
    assert(kernel::G<uint8_t>(context + 0x20C) && kernel::G<uint8_t>(context + 0x20D));
    Mouse(window, SDL_EVENT_MOUSE_BUTTON_UP, 250);
    for (unsigned i = 0; i < 30; ++i) Frame(Read(pad, true));
    assert(strikes == 1 && touches == 0);
    // Releasing a held backswing and the right stick's return both reach the
    // same detector; holding still must never produce repeated strikes.
    Mouse(window, SDL_EVENT_MOUSE_BUTTON_DOWN, 250);
    Mouse(window, SDL_EVENT_MOUSE_MOTION, 460, 210);
    for (unsigned i = 0; i < 20; ++i) Frame(Read(pad, true));
    assert(Read(pad, true) == 45000 && strikes == 1);
    Mouse(window, SDL_EVENT_MOUSE_BUTTON_UP, 460);
    Frame(Read(pad, true)); assert(strikes == 2);
    for (unsigned i = 0; i < 20; ++i) Frame(Read(pad, true));
    pad.ry = -32768;
    for (unsigned i = 0; i < 20; ++i) Frame(Read(pad, true));
    pad.ry = 0; Frame(Read(pad, true)); assert(strikes == 3);
    Mouse(window, SDL_EVENT_MOUSE_BUTTON_DOWN, 250);
    Mouse(window, SDL_EVENT_MOUSE_MOTION, 460, 210);
    Mouse(window, SDL_EVENT_WINDOW_FOCUS_LOST, 0);
    assert(Read(pad, true) == 0);
    Mouse(window, SDL_EVENT_WINDOW_FOCUS_GAINED, 0);
    menu = true; Mouse(window, SDL_EVENT_MOUSE_BUTTON_DOWN, 250);
    Mouse(window, SDL_EVENT_MOUSE_MOTION, 460, 210);
    assert(Read(pad, true) == 0);
    SDL_DestroyWindow(window); SDL_Quit();
    std::puts("Mouse/stick -> JVS channel 3 -> original decoder/filter: backswing latch and three forward strokes passed");
}
