#pragma once
#include "core/input.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace golf::swing {
// Original calibration defaults (0x22660): start 15000, rest 8000,
// full backswing 45000. JVS channel 3 reaches input+0xB4 via 0x82CA0;
// 0x836B0 filters its position and detects the forward stroke.
inline constexpr uint16_t kFullBackswing = 45000;
class Control {
public:
    void Begin() { held_ = true; position_ = 0; }
    void Move(float vertical_fraction) {
        if (held_) position_ = std::clamp(position_ + vertical_fraction / 0.35f, 0.0f, 1.0f);
    }
    void Release() { held_ = false; position_ = 0; }
    uint16_t Read(const input::Pad& pad, bool active) {
        if (!active) { Release(); return 0; }
        if (held_) return uint16_t(std::lround(position_ * kFullBackswing));
        // Console axes are positive upwards. Neutral is the club's rest,
        // rather than the midpoint of an unsigned joystick axis.
        const float back = std::clamp(-float(pad.ry) / 32768.0f, 0.0f, 1.0f);
        return uint16_t(std::lround(std::max(0.0f, (back - 0.12f) / 0.88f) * kFullBackswing));
    }
private:
    float position_ = 0;
    bool held_ = false;
};
void Begin();
void Move(float vertical_fraction);
void Release();
uint16_t Read(const input::Pad& pad, bool active);
}
