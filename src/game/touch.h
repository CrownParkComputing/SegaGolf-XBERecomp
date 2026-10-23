#pragma once
#include "core/input.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace golf::touch {
using Packet = std::array<uint8_t, 10>;
inline uint8_t Checksum(const Packet& packet) {
    uint8_t sum = 0xAA;
    for (unsigned i = 0; i < 9; ++i) sum += packet[i];
    return sum;
}
inline Packet Contact(float x, float y, uint8_t state) {
    const auto raw_x = uint16_t(std::lround(std::clamp(x, 0.0f, 1.0f) * 4095));
    // Original parser at 0xC5910 reverses the physical panel's Y coordinate.
    const auto raw_y = uint16_t(std::lround((1 - std::clamp(y, 0.0f, 1.0f)) * 4095));
    Packet packet{0x55, 'T', state, uint8_t(raw_x), uint8_t(raw_x >> 8),
                  uint8_t(raw_y), uint8_t(raw_y >> 8), 1, 0, 0};
    packet[9] = Checksum(packet);
    return packet;
}
struct Cursor {
    float x = 0.5f, y = 0.5f;
    bool down = false;
    bool mouse = false;
    void Update(const input::Pad& pad, float seconds, bool active) {
        const auto axis = [](int value) {
            const float f = std::clamp(value / 32767.0f, -1.0f, 1.0f);
            return std::abs(f) <= 0.18f ? 0.0f : std::copysign((std::abs(f) - 0.18f) / 0.82f, f);
        };
        if (active) {
            const float speed = pad.analog[6] ? 0.18f : 0.65f;
            const float dx = pad.buttons & 0xC ? float(bool(pad.buttons & 8)) - float(bool(pad.buttons & 4)) : axis(pad.lx);
            const float dy = pad.buttons & 3 ? float(bool(pad.buttons & 2)) - float(bool(pad.buttons & 1)) : -axis(pad.ly);
            const float step = std::clamp(seconds, 0.0f, 0.05f) * speed;
            x = std::clamp(x + dx * step, 0.0f, 1.0f);
            y = std::clamp(y + dy * step, 0.0f, 1.0f);
        }
        down = active && (pad.analog[0] || pad.analog[7] > 64);
    }
};
struct Pointer {
    float x, y;
    bool inside;
};
inline Pointer MapMouse(float x, float y, float width, float height, bool widescreen) {
    if (width <= 0 || height <= 0) return {0, 0, false};
    const float aspect = widescreen ? 16.0f / 9.0f : 4.0f / 3.0f;
    const float game_width = std::min(width, height * aspect);
    const float game_height = game_width / aspect;
    const float u = (x - (width - game_width) / 2) / game_width;
    const float v = (y - (height - game_height) / 2) / game_height;
    return {std::clamp(u, 0.0f, 1.0f), std::clamp(v, 0.0f, 1.0f),
            u >= 0 && u <= 1 && v >= 0 && v <= 1};
}
void SubmitPointer(Pointer point, bool down, bool active);
void ReleasePointer();
void MouseWindow(void*);
void Window(void*);
void Draw(double fps);
void ToggleMenu();
Cursor ReadCursor();
}
