#include "game/swing.h"
#include <mutex>

namespace golf::swing {
namespace {
std::mutex lock;
Control control;
}
void Begin() { std::lock_guard guard(lock); control.Begin(); }
void Move(float vertical_fraction) { std::lock_guard guard(lock); control.Move(vertical_fraction); }
void Release() { std::lock_guard guard(lock); control.Release(); }
uint16_t Read(const input::Pad& pad, bool active) {
    std::lock_guard guard(lock);
    return control.Read(pad, active);
}
}
