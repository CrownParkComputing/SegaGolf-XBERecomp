// Exercise the real XDK host entries with the title's verified layout. Dead
// sections of d3d8.cpp are discarded so these checks need no GPU or game boot.
#include "kernel/kernel.h"
#include "kernel/memory.h"
#include "xdk/d3d8.h"
#include <array>
#include <cassert>
#include <cstdio>

X86_EXTERN_FUNC(sub_00195A70);
X86_EXTERN_FUNC(sub_00195A90);
X86_EXTERN_FUNC(sub_00196130);
namespace kernel { uint8_t* g_base; }
namespace mem {
uint32_t AllocContiguous(uint32_t, uint32_t, uint32_t, uint32_t) {
    assert(false && "these API checks never submit GPU work");
    return 0;
}
uint32_t PhysicalFromVirtual(uint32_t address) { return address; }
uint32_t VirtualFromPhysical(uint32_t address) { return address; }
uint32_t AllocSystem(uint32_t bytes, uint32_t alignment) {
    static uint32_t next = 0x10000;
    next = (next + alignment - 1) & ~(alignment - 1);
    const auto result = next;
    next += bytes;
    return result;
}
}
namespace {
std::array<uint8_t, 0x1C0000> memory{};
uint32_t Call(X86Func* function, std::initializer_list<uint32_t> args = {}) {
    X86Context ctx{};
    ctx.esp = 0x800;
    unsigned i = 0;
    for (auto value : args) kernel::W32(ctx.esp + 4 + i++ * 4, value);
    function(ctx, memory.data());
    assert(ctx.esp == 0x804 + args.size() * 4);
    return ctx.eax;
}
}
int main() {
    using kernel::W32;
    using kernel::R32;
    kernel::g_base = memory.data();
    constexpr uint32_t surface = 0x1000, parent = 0x1100;
    W32(0x1B2090 + 0x1A04, surface);
    W32(0x1B2090 + 0x1A08, surface);
    W32(surface, 0x010D0000); // device holds the surface; no public owners
    W32(surface + 0x14, parent);
    W32(parent, 0x01040001);
    assert(Call(sub_00195A70) == surface);
    assert(R32(surface) == 0x010D0001);
    assert(R32(parent) == 0x01040002);
    // Additional public surface references share the one parent reference.
    assert(Call(sub_00195A90) == surface);
    assert(R32(surface) == 0x010D0002 && R32(parent) == 0x01040002);
    W32(0x1B2090 + 0x1A08, 0);
    assert(Call(sub_00195A90) == 0);

    // Repeated zero-to-one acquisitions must balance the original public
    // release path, which releases the parent when the last owner goes away.
    for (unsigned i = 0; i < 2000; ++i) {
        W32(surface, 0x010D0000);
        W32(parent, R32(parent) - 1);
        assert(Call(sub_00195A70) == surface);
        assert(R32(parent) == 0x01040002);
    }

    // SetTexture sends the supplied format/address to the GPU at bind time.
    // Its renderer header must survive later edits to the application header.
    W32(parent, 0x01040001);
    W32(parent + 4, 0x06A6B000);
    W32(parent + 12, 0x00011229);
    W32(parent + 16, 799 | 599 << 12 | 49u << 24);
    Call(sub_00196130, {0, parent});
    const auto bound = d3d::State().textures[0];
    assert(bound && bound != parent);
    W32(parent, 0x0103FFFF);
    W32(parent + 12, 0);
    assert((R32(bound) & 0x70000) == 0x40000);
    assert(R32(bound + 4) == 0x06A6B000 && R32(bound + 12) == 0x00011229);
    assert(R32(bound + 16) == (799 | 599 << 12 | 49u << 24));
    Call(sub_00196130, {0, 0});
    assert(d3d::State().textures[0] == 0);
    std::puts("Sega Golf surface parent references and captured texture bindings passed");
}
