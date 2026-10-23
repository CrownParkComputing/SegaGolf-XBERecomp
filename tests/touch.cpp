#include "game/touch.h"
#include "kernel/kernel.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>

X86_EXTERN_FUNC(sub_000C6DD0);
X86_EXTERN_FUNC(sub_00147C50);
X86_EXTERN_FUNC(sub_00147E00);
X86_FUNC(__imp__sub_000C6DD0) { assert(false); }
X86_FUNC(__imp__sub_00147C50) { assert(false); }
X86_FUNC(__imp__sub_00147E00) { assert(false); }
namespace kernel { uint8_t* g_base = nullptr; }
namespace input {
Pad sample;
Pad Poll(int) { return sample; }
}
namespace {
std::array<uint8_t, 4096> memory{};
uint32_t Call(X86Func* function, std::initializer_list<uint32_t> args, bool cdecl = false) {
    X86Context ctx{};
    ctx.esp = 0x800;
    unsigned i = 0;
    for (auto value : args) kernel::W32(ctx.esp + 4 + i++ * 4, value);
    function(ctx, memory.data());
    assert(ctx.esp == 0x804 + (cdecl ? 0 : args.size() * 4));
    return ctx.eax;
}
unsigned Available() {
    assert(Call(sub_00147C50, {1, 0x300}) == 0);
    return kernel::R32(0x300);
}
golf::touch::Packet Read() {
    golf::touch::Packet packet;
    for (auto& byte : packet) byte = uint8_t(Call(sub_00147E00, {1}));
    assert(packet[0] == 0x55 && packet[9] == golf::touch::Checksum(packet));
    return packet;
}
void Send(golf::touch::Packet packet) {
    packet[9] = golf::touch::Checksum(packet);
    std::memcpy(kernel::HostPtr(0x100), packet.data(), packet.size());
    assert(Call(sub_000C6DD0, {1, 0x100, 10}, true) == 0);
}
}
int main() {
    using namespace golf::touch;
    kernel::g_base = memory.data();
    Cursor cursor;
    input::Pad pad;
    pad.lx = 100;
    cursor.Update(pad, 0.05f, true);
    assert(cursor.x == 0.5f);
    pad.lx = 32767;
    pad.ly = 32767;
    cursor.Update(pad, 0.05f, true);
    assert(cursor.x > 0.5f && cursor.y < 0.5f);
    const float x = cursor.x;
    pad.analog[6] = 255;
    cursor.Update(pad, 0.05f, true);
    assert(cursor.x - x < 0.01f);
    for (unsigned i = 0; i < 1000; ++i) cursor.Update(pad, 0.05f, true);
    assert(cursor.x == 1 && cursor.y == 0);
    pad.analog[0] = 255;
    cursor.Update(pad, 0, true);
    assert(cursor.down);
    cursor.Update(pad, 0.05f, false);
    assert(!cursor.down && cursor.x == 1 && cursor.y == 0);
    const auto corner = Contact(1, 0, 1);
    assert(corner[3] == 0xFF && corner[4] == 0xF && corner[5] == 0xFF && corner[6] == 0xF);
    const auto center = MapMouse(960, 540, 1920, 1080, false);
    assert(center.inside && center.x == 0.5f && center.y == 0.5f);
    assert(!MapMouse(100, 540, 1920, 1080, false).inside);
    assert(MapMouse(240, 0, 1920, 1080, false).inside);
    assert(!MapMouse(400, 10, 800, 800, false).inside);
    assert(MapMouse(0, 0, 1920, 1080, true).inside);
    assert(!MapMouse(0, 0, 0, 0, false).inside);

    // Exercise the exact serial ABI used by the original driver, including
    // the reset and baud query that its startup state machine requires.
    Send({0x55, 'R', '2'});
    assert(Available() == 0);
    SubmitPointer({0.25f, 0.75f, true}, true, true);
    SubmitPointer({0.25f, 0.75f, true}, false, true);
    assert(Available() == 20);
    auto packet = Read();
    assert(packet[2] == 1 && packet[3] == 0 && packet[4] == 4 && packet[5] == 0 && packet[6] == 4);
    assert(Read()[2] == 4);
    SubmitPointer({0.5f, 0.5f, true}, true, true);
    ReleasePointer();
    assert(Available() == 20 && Read()[2] == 1 && Read()[2] == 4);
    SubmitPointer({0, 0.5f, false}, true, true);
    assert(Available() == 0);
    Send({0x55, 'p'});
    assert(Available() == 10);
    packet = Read();
    assert(packet[1] == 'P' && packet[3] == 6);
    Send({0x55, 'M', 0, 7});
    assert(Available() == 10);
    packet = Read();
    assert(packet[1] == 'A' && packet[2] == '0');
    Send({0x55, 'm'});
    assert(Available() == 10);
    packet = Read();
    assert(packet[1] == 'M' && packet[3] == 7);

    input::sample.analog[0] = 255;
    assert(Available() == 10);
    packet = Read();
    assert(packet[1] == 'T' && packet[2] == 1);
    input::sample.analog[0] = 0;
    assert(Available() == 10);
    packet = Read();
    assert(packet[1] == 'T' && packet[2] == 4);
    assert(Available() == 0);
    memory[0x109] ^= 1;
    assert(Call(sub_000C6DD0, {1, 0x100, 10}, true) == 1);
    assert(Available() == 0);
    std::puts("Sega Golf touch movement, serial handshake, checksum and contact edge checks passed");
}
