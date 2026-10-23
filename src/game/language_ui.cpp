#include "game/language.h"

#ifdef SEGAGOLF_SDL_INPUT
#include "language_tables.h"
#include "app/app.h"
#include "gpu/backend.h"
#include "gpu/texture.h"
#include "gpu/vertex.h"
#include "kernel/kernel.h"
#include <imgui.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <unordered_set>
#include <vector>

namespace golf::language {
namespace {
struct Label { float x0, y0, x1, y1, alpha; const char* text; };
std::vector<Label> pending, frame;

bool Register(const gpu::Assembled& vertices, uint32_t index, unsigned reg, float* out, unsigned count) {
    if (!(vertices.regs & (1u << reg))) return false;
    if (!vertices.vertices.empty()) {
        const auto stride = std::popcount(uint32_t(vertices.regs)) * 4;
        const auto offset = std::popcount(uint32_t(vertices.regs) & ((1u << reg) - 1)) * 4;
        const size_t at = size_t(index) * stride + offset;
        if (at + count > vertices.vertices.size()) return false;
        std::memcpy(out, &vertices.vertices[at], count * sizeof(float));
        return true;
    }
    const auto& element = vertices.elements[reg];
    const auto& buffer = vertices.buffers[element.buffer];
    const size_t offset = size_t(index) * buffer.stride + element.offset;
    if ((reg == 0 && element.format != 2) || (reg == 9 && element.format != 16) ||
        !buffer.src || offset + count * sizeof(float) > buffer.bytes) return false;
    std::memcpy(out, buffer.src + offset, count * sizeof(float));
    return true;
}

void Observe(const gpu::State& state, uint32_t prim, uint32_t start, uint32_t count, const uint8_t* indices) {
    LOG_FIRST(1, "SegaGolf English: sprite observer active");
    if (!Enabled() || count != 4 || !state.textures[0]) return;
    gpu::TextureDesc texture;
    if (!gpu::Describe(state.textures[0], texture) || texture.format != 15 ||
        texture.cube || texture.depth != 1 || texture.width > 1024 || texture.height > 1024) return;
    const auto size = gpu::LevelSize(texture, 0);
    const auto signature = TextureSignature(static_cast<const uint8_t*>(kernel::HostPtr(texture.data)), size);
    if (std::getenv("SEGAGOLF_TRACE_LANGUAGE")) {
        static std::unordered_set<uint32_t> seen;
        if (seen.insert(texture.data).second)
            LOGI("SegaGolf English atlas: data=%08X %ux%u size=%u signature=%016llX", texture.data,
                 texture.width, texture.height, size, (unsigned long long)signature);
    }
    bool known = false;
    for (const auto& label : kSprites)
        if (label.signature == signature && label.width == texture.width && label.height == texture.height) { known = true; break; }
    if (!known) return;
    LOG_FIRST(4, "SegaGolf English: recognized atlas %016llX", (unsigned long long)signature);
    gpu::Assembled vertices;
    if (!gpu::Assemble(state, prim, start, count, indices, vertices)) return;
    LOG_FIRST(4, "SegaGolf English: sprite inputs pre=%u count=%u regs=%X", vertices.pretransformed,
              vertices.vertex_count, vertices.regs);
    if (!vertices.pretransformed || vertices.vertex_count != 4) return;
    float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
    float u0 = 1e9f, v0 = 1e9f, u1 = -1e9f, v1 = -1e9f;
    for (unsigned index = 0; index < 4; ++index) {
        float position[4], uv[2];
        if (!Register(vertices, index, 0, position, 4) || !Register(vertices, index, 9, uv, 2)) {
            LOG_FIRST(4, "SegaGolf English: sprite register read failed formats=%u/%u bytes=%u/%u", vertices.elements[0].format,
                      vertices.elements[9].format, vertices.buffers[vertices.elements[0].buffer].bytes,
                      vertices.buffers[vertices.elements[9].buffer].bytes);
            return;
        }
        x0 = std::min(x0, position[0]); y0 = std::min(y0, position[1]);
        x1 = std::max(x1, position[0]); y1 = std::max(y1, position[1]);
        u0 = std::min(u0, uv[0]); v0 = std::min(v0, uv[1]);
        u1 = std::max(u1, uv[0]); v1 = std::max(v1, uv[1]);
    }
    // The original interface is 800x600, including its offscreen UI layers.
    if (x1 - x0 < 3 || y1 - y0 < 3 || x0 < -800 || y0 < -600 || x1 > 1600 || y1 > 1200) return;
    float alpha = 1;
    if (vertices.vertices.empty() && (vertices.regs & 8)) {
        const auto& element = vertices.elements[3];
        const auto& buffer = vertices.buffers[element.buffer];
        if (element.format == 87 && buffer.src && element.offset + 4 <= buffer.bytes)
            alpha = buffer.src[element.offset + 3] / 255.0f;
    }
    if (alpha < 0.1f) return;
    for (const auto& label : kSprites) {
        if (!Matches(label, signature, texture.width, texture.height, u0, v0, u1, v1)) continue;
        const auto rectangle = Bounds(label, x0, y0, x1, y1);
        for (const auto& old : pending)
            if (old.text == label.english && std::abs(old.x0 - rectangle.x0) < 1 && std::abs(old.y0 - rectangle.y0) < 1) return;
        if (pending.size() < 512) pending.push_back({rectangle.x0, rectangle.y0, rectangle.x1, rectangle.y1, alpha, label.english});
        LOG_FIRST(12, "SegaGolf English label: %s at %.1f,%.1f %.1fx%.1f", label.english, x0, y0, x1 - x0, y1 - y0);
        return;
    }
}
void FinishFrame() { frame.swap(pending); pending.clear(); }
}

void Init() {
    if (const char* role = std::getenv("SEGAGOLF_ROLE"); role && std::strcmp(role, "server") == 0) return;
    gpu::g_draw_observer = Observe;
    gpu::g_present_observer = FinishFrame;
}
void Draw() {
    if (!Enabled()) return;
    const auto& io = ImGui::GetIO();
    const float aspect = app::Widescreen() ? 16.0f / 9.0f : 4.0f / 3.0f;
    const float width = std::min(io.DisplaySize.x, io.DisplaySize.y * aspect), height = width / aspect;
    const float sx = width / 800, sy = height / 600;
    const ImVec2 origin{(io.DisplaySize.x - width) / 2, (io.DisplaySize.y - height) / 2};
    auto* draw = ImGui::GetBackgroundDrawList();
    auto* font = ImGui::GetFont();
    draw->PushClipRect(origin, {origin.x + width, origin.y + height}, true);
    for (const auto& label : frame) {
        const ImVec2 low{origin.x + label.x0 * sx, origin.y + label.y0 * sy};
        const ImVec2 high{origin.x + label.x1 * sx, origin.y + label.y1 * sy};
        const float w = high.x - low.x, h = high.y - low.y;
        unsigned lines = 1;
        for (const char* p = label.text; *p; ++p) lines += *p == '\n';
        float size = std::min(24.0f * sy, h * 0.8f / lines);
        auto measured = font->CalcTextSizeA(size, 1e9f, 0, label.text);
        if (measured.x > w - 4 * sx) size *= std::max(1.0f, w - 4 * sx) / measured.x;
        measured = font->CalcTextSizeA(size, 1e9f, 0, label.text);
        const int opacity = int(label.alpha * 255);
        draw->AddRectFilled({low.x - sx, low.y - sy}, {high.x + sx, high.y + sy},
                            IM_COL32(10, 24, 34, opacity), 2 * sy);
        draw->AddText(font, size, {low.x + (w - measured.x) / 2, low.y + (h - measured.y) / 2},
                      IM_COL32(255, 255, 240, opacity), label.text);
    }
    draw->PopClipRect();
}
}
#else
namespace golf::language { void Init() {} void Draw() {} }
#endif
