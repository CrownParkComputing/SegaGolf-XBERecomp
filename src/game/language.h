#pragma once
#include <cstddef>
#include <cstdint>
#include <cmath>

namespace golf::language {
struct StringPatch {
    uint32_t address, size;
    uint64_t expected;
    const char* english;
};
struct SpritePatch {
    uint64_t signature;
    uint16_t width, height;
    float u0, v0, u1, v1;
    const char* english;
    float left = 0, top = 0, right = 1, bottom = 1;
};
struct Rect { float x0, y0, x1, y1; };
inline Rect Bounds(const SpritePatch& label, float x0, float y0, float x1, float y1) {
    const float width = x1 - x0, height = y1 - y0;
    return {x0 + label.left * width, y0 + label.top * height,
            x0 + label.right * width, y0 + label.bottom * height};
}
inline bool Matches(const SpritePatch& label, uint64_t signature, uint32_t width, uint32_t height,
                    float u0, float v0, float u1, float v1) {
    if (label.signature != signature || label.width != width || label.height != height) return false;
    const float du = 1.1f / width, dv = 1.1f / height;
    return std::abs(u0 - label.u0) <= du && std::abs(v0 - label.v0) <= dv &&
           std::abs(u1 - label.u1) <= du && std::abs(v1 - label.v1) <= dv;
}
inline uint64_t Hash(const uint8_t* data, size_t size) {
    uint64_t hash = 14695981039346656037ULL;
    for (size_t index = 0; index < size; ++index) hash = (hash ^ data[index]) * 1099511628211ULL;
    return hash;
}
inline uint64_t TextureSignature(const uint8_t* data, size_t size) {
    if (size < 32) return 0;
    uint64_t hash = 14695981039346656037ULL;
    for (size_t sample = 0; sample < 16; ++sample) {
        const size_t offset = (size - 32) * sample / 15;
        for (size_t index = 0; index < 32; ++index)
            hash = (hash ^ data[offset + index]) * 1099511628211ULL;
    }
    return hash;
}
void Init();
void Loaded();
bool Enabled();
void SetEnabled(bool enabled);
void Draw();
}
