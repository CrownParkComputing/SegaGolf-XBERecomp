#include "kernel/kernel.h"
#include "core/input.h"
#include "app/app.h"
#include "game/swing.h"
#include <mutex>

X86_EXTERN_FUNC(__imp__sub_0014AC90);
X86_EXTERN_FUNC(__imp__sub_0014A580);
namespace golf::network { void UpdateBoardAddress(); }

namespace {
std::mutex board_lock;
uint32_t reports = 0;
uint16_t coins = 0;
bool coin_down = false;
uint32_t report_updates = 0;
void Byte(uint32_t address, uint8_t value) { kernel::G<uint8_t>(address) = value; }
void BigWord(uint32_t address, uint16_t value) { Byte(address, value >> 8); Byte(address + 1, value); }

void UpdateBoard() {
    if (!reports) {
        reports = mem::AllocSystem(64);
        if (!reports) return;
        std::memset(kernel::HostPtr(reports), 0, 64);
        LOGI("SegaGolf: virtual JVS switch, coin and analog reports");
    }
    // Count getter: 0x14A810 -> 0x18ADD0. Node array/stride:
    // 0x14A850 and 0x18AD50; public node numbers start at one.
    constexpr uint32_t node = 0x98626C + 0xE0;
    Byte(0x985E69, 1);
    // Released test/service pins; the game inverts AL at 0x82B4C.
    kernel::W32(0x22337C, 0xFF);
    kernel::W32(node + 0xAC, reports);
    kernel::W32(node + 0xB0, reports + 8);
    kernel::W32(node + 0xB4, reports + 16);
    // Original capability readers at 0x14AC20, 0x14ADE0, 0x14B220.
    Byte(node + 0xCC, 2);
    Byte(node + 0xCD, 13);
    Byte(node + 0xCE, 2);
    Byte(node + 0xCF, 4);
    Byte(node + 0xD0, 16);
    Byte(node + 0xDA, 8);
    const auto pad = input::Poll(0);
    ++report_updates;
#ifdef SEGAGOLF_SDL_INPUT
    const bool active = !app::Unfocused() && !app::GraphicsMenuOpen();
#else
    const bool active = true;
#endif
    const bool coin = active && (pad.buttons & 0x20);
    if (coin && !coin_down) {
        coins = (coins + 1) & 0x3FFF;
        LOGI("SegaGolf: coin inserted");
    }
    coin_down = coin;
    BigWord(reports + 8, coins);
    Byte(reports, 0);
    // Switch decoding at 0x834E1..0x83620 and 0x82C00.
    Byte(reports + 1, active ? uint8_t(((pad.buttons & 0x10) ? 0x80 : 0) |
         (pad.analog[0] ? 2 : 0) | (pad.analog[1] ? 1 : 0)) : 0);
    Byte(reports + 2, 0);
    Byte(reports + 3, active ? uint8_t((pad.buttons & 1 ? 0x20 : 0) |
         (pad.buttons & 2 ? 0x10 : 0) | (pad.buttons & 4 ? 8 : 0) | (pad.buttons & 8 ? 4 : 0)) : 0);
    Byte(reports + 4, 0);
    BigWord(reports + 16, uint16_t((active ? pad.lx : 0) + 32768));
    BigWord(reports + 18, uint16_t(32767 - (active ? pad.ly : 0)));
    // acLib's 0x14B210 converts the big-endian report to host words;
    // 0x834A0 copies it to 0x677726, and 0x82CA0 reads channel 3.
    BigWord(reports + 20, golf::swing::Read(pad, active));
    BigWord(reports + 22, 0x8000);
}
}

// acLib's hardware discovery update (0x148B00) depends on physical port I/O
// at 0x14D480 and mailbox transfers at 0x14E470. Those have no host device.
// Keep acLib initialization, storage, input and the game's startup state
// machine; provide the status of a virtual baseboard/system controller at
// this hardware boundary instead of repeatedly uploading physical firmware.
X86_FUNC(sub_00148B00) {
    if (kernel::R32(0x8EF808)) {
        golf::network::UpdateBoardAddress();
        // 0x148C46..58 publishes baseboard version bytes at info+6/+7;
        // 0x148DA7..B6 publishes controller version bytes at info+14/+15.
        // The original startup reader at 0xD4CAA..BA requires nonzero network
        // version bytes. The later check at 0x8F9D3..FA requires >=12.07.
        // Publish that acLib protocol version for the loopback network adapter.
        kernel::G<uint8_t>(0x8EF83A) = 1;
        kernel::G<uint8_t>(0x8EF83B) = 1;
        kernel::G<uint8_t>(0x8EF848) = 0x12;
        kernel::G<uint8_t>(0x8EF849) = 0x07;
        // The original public readiness getter (0x148450) accepts state >=4.
        // State 4 is discovery's idle state (0x148DE2 and jump table 148DF8).
        kernel::W32(0x8EF818, 4);
        LOG_FIRST(1, "SegaGolf: virtual baseboard and system controller ready");
    }
    kernel::Return(ctx, 0, 0);
}

X86_FUNC(sub_0014A810) {
    std::lock_guard guard(board_lock);
    UpdateBoard();
    kernel::Return(ctx, reports ? 1 : 0, 0);
}

// Node status must describe the virtual reports, rather than failed mailbox
// requests to absent JVS hardware. The original watchdog (838E0..83969) checks
// the last two counters for accumulated/consecutive transfer failures.
X86_FUNC(sub_0014A580) {
    const kernel::Args args{ctx};
    if (args[0] != 1) { __imp__sub_0014A580(ctx, base); return; }
    if (!args[1]) { kernel::Return(ctx, 1, 2); return; }
    std::lock_guard guard(board_lock);
    UpdateBoard();
    kernel::W32(args[1], report_updates);
    kernel::W32(args[1] + 4, report_updates);
    kernel::W32(args[1] + 8, 0);
    kernel::W32(args[1] + 12, 0);
    kernel::Return(ctx, 0, 2);
}

// Refresh before the original four-argument switch report reader. Its lock,
// capability checks and report pointer handling remain unchanged.
X86_FUNC(sub_0014AC90) {
    {
        std::lock_guard guard(board_lock);
        UpdateBoard();
    }
    __imp__sub_0014AC90(ctx, base);
}
