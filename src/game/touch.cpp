#include "game/touch.h"
#include "kernel/kernel.h"
#include "app/app.h"
#include <chrono>
#include <deque>
#include <mutex>

X86_EXTERN_FUNC(__imp__sub_000C6DD0);
X86_EXTERN_FUNC(__imp__sub_00147C50);
X86_EXTERN_FUNC(__imp__sub_00147E00);

namespace golf::touch {
namespace {
std::mutex lock;
Cursor cursor;
std::deque<uint8_t> received;
bool reported_down = false;
uint8_t baud = 6, mode = 7, delay = 5;
std::array<uint8_t, 2> calibration{3, 1};
auto last_poll = std::chrono::steady_clock::now();
auto last_report = last_poll;

void Queue(Packet packet) {
    packet[9] = Checksum(packet);
    if (received.size() <= 240) received.insert(received.end(), packet.begin(), packet.end());
}

bool Command(const Packet& request) {
    if (request[0] != 0x55 || request[9] != Checksum(request)) return false;
    Packet response{0x55, 'A', '0', '0', '0', '0'};
    switch (request[1]) {
    case 'R': received.clear(); reported_down = false; return true;
    case 'a': break;
    case 'p': response = {0x55, 'P', '0', baud, 4}; break;
    case 'P': baud = request[3]; break;
    case 'm': response = {0x55, 'M', 0, mode}; break;
    case 'M': mode = request[3]; break;
    case 'b': response = {0x55, 'B', calibration[0], calibration[1]}; break;
    case 'B': calibration = {request[2], request[3]}; break;
    case 'd': response = {0x55, 'D', delay, delay}; break;
    case 'D': delay = request[3]; break;
    default: response[2] = '1'; break;
    }
    Queue(response);
    LOG_FIRST(24, "SegaGolf: touch panel command %c -> %c", request[1], response[1]);
    return true;
}

void Report(std::chrono::steady_clock::time_point now) {
    if (cursor.down != reported_down || (cursor.down && now - last_report >= std::chrono::milliseconds(16))) {
        Queue(Contact(cursor.x, cursor.y, cursor.down ? (reported_down ? 2 : 1) : 4));
        LOG_FIRST(32, "SegaGolf: touch %s at %.0f,%.0f", cursor.down ? "press/hold" : "release", cursor.x * 799, cursor.y * 599);
        reported_down = cursor.down;
        last_report = now;
    }
}
void Poll() {
    const auto now = std::chrono::steady_clock::now();
    const auto pad = input::Poll(0);
#ifdef SEGAGOLF_SDL_INPUT
    const bool active = !app::Unfocused() && !app::GraphicsMenuOpen();
#else
    const bool active = true;
#endif
    if ((pad.buttons & 15) || std::abs(int(pad.lx)) > 5898 || std::abs(int(pad.ly)) > 5898 ||
        pad.analog[0] || pad.analog[7] > 64) cursor.mouse = false;
    if (!cursor.mouse) cursor.Update(pad, std::chrono::duration<float>(now - last_poll).count(), active);
    else if (!active) cursor.down = false;
    last_poll = now;
    Report(now);
}
}
Cursor ReadCursor() { std::lock_guard guard(lock); return cursor; }
void SubmitPointer(Pointer point, bool down, bool active) {
    std::lock_guard guard(lock);
    cursor.mouse = true;
    cursor.x = point.x;
    cursor.y = point.y;
    cursor.down = down && active && point.inside;
    // Queue edges at the event boundary so a quick click between serial polls
    // still reaches the original panel parser as a complete press/release.
    Report(std::chrono::steady_clock::now());
}
void ReleasePointer() {
    std::lock_guard guard(lock);
    cursor.down = false;
    Report(std::chrono::steady_clock::now());
}
}

// Touch device serial packet boundary: cdecl(port,buffer,size), ret at
// 0xC6DFD. Logical port 1 is the panel (0xC69D5); port 0 is the card reader.
// Leave the original checksum/parser, calibration, filtering and gestures active.
X86_FUNC(sub_000C6DD0) {
    const kernel::Args args{ctx};
    if (args[0] != 1) { __imp__sub_000C6DD0(ctx, base); return; }
    std::lock_guard guard(golf::touch::lock);
    bool valid = args[1] && args[2] == 10;
    if (valid) {
        golf::touch::Packet packet;
        std::memcpy(packet.data(), kernel::HostPtr(args[1]), packet.size());
        valid = golf::touch::Command(packet);
    }
    kernel::Return(ctx, valid ? 0 : 1, 0);
}

// acLib serial available(port,count), stdcall 2, ret 8 at 0x147C7B.
X86_FUNC(sub_00147C50) {
    const kernel::Args args{ctx};
    if (args[0] != 1) { __imp__sub_00147C50(ctx, base); return; }
    std::lock_guard guard(golf::touch::lock);
    golf::touch::Poll();
    if (args[1]) kernel::W32(args[1], uint32_t(golf::touch::received.size()));
    kernel::Return(ctx, args[1] ? 0 : 1, 2);
}

// acLib serial byte reader(port), stdcall 1, ret 4 at 0x147E26.
X86_FUNC(sub_00147E00) {
    const kernel::Args args{ctx};
    if (args[0] != 1) { __imp__sub_00147E00(ctx, base); return; }
    std::lock_guard guard(golf::touch::lock);
    uint8_t value = 0;
    if (!golf::touch::received.empty()) {
        value = golf::touch::received.front();
        golf::touch::received.pop_front();
    }
    kernel::Return(ctx, value, 1);
}

// System-controller serial configuration wrapper: 0x18AC80 tail-jumps to
// the physical API at 0x22D640 (ret 4 at 0x22D7E2). The recompiler folds
// that tail target into this wrapper, so intercept the reachable entry.
// The virtual panel uses its own queue; baud selection is acknowledged above.
X86_FUNC(sub_0018AC80) {
    kernel::Return(ctx, 0, 1);
}
