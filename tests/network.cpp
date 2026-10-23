#include "kernel/kernel.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <chrono>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <poll.h>
#include <unistd.h>

X86_EXTERN_FUNC(sub_00152F00);
X86_EXTERN_FUNC(sub_00152D80);
X86_EXTERN_FUNC(sub_001508A0);
X86_EXTERN_FUNC(sub_001514E0);
X86_EXTERN_FUNC(sub_00150730);
X86_EXTERN_FUNC(sub_0014FFA0);
X86_EXTERN_FUNC(sub_00153070);
X86_EXTERN_FUNC(sub_00150980);
X86_EXTERN_FUNC(sub_00150E20);
X86_EXTERN_FUNC(sub_00150B50);
X86_EXTERN_FUNC(sub_00152E60);
X86_EXTERN_FUNC(sub_00150340);
X86_EXTERN_FUNC(sub_00150170);
X86_EXTERN_FUNC(sub_001527A0);
X86_EXTERN_FUNC(sub_00151910);
X86_EXTERN_FUNC(sub_00152A60);
X86_EXTERN_FUNC(sub_00151C50);
X86_EXTERN_FUNC(sub_00166BD0);

namespace kernel { uint8_t* g_base = nullptr; }
namespace {
std::array<uint8_t, 4096> memory{};
constexpr uint32_t kError = uint32_t(-1), kBusy = uint32_t(-2);

uint32_t Call(X86Func* function, std::initializer_list<uint32_t> args, bool cdecl = false) {
    X86Context ctx{};
    ctx.esp = 0x800;
    ctx.ebx = 0x12345678;
    kernel::W32(ctx.esp, 0xDEADBEEF);
    unsigned i = 0;
    for (auto value : args) kernel::W32(ctx.esp + 4 + i++ * 4, value);
    function(ctx, memory.data());
    assert(ctx.esp == 0x804 + (cdecl ? 0 : args.size() * 4));
    assert(ctx.ebx == 0x12345678);
    return ctx.eax;
}
uint32_t Finish(uint32_t handle) {
    for (unsigned poll = 0; poll < 1000; ++poll) {
        if (Call(sub_0014FFA0, {handle}) == 1) return Call(sub_00153070, {handle});
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    assert(false && "socket operation did not finish");
    return kError;
}
int HostSocket(bool udp, sockaddr_in& address) {
    const int fd = socket(AF_INET, udp ? SOCK_DGRAM : SOCK_STREAM, 0);
    assert(fd >= 0);
    address = {}; address.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.16", &address.sin_addr);
    assert(bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0);
    socklen_t size = sizeof(address);
    assert(getsockname(fd, reinterpret_cast<sockaddr*>(&address), &size) == 0);
    return fd;
}
void GuestAddress(const sockaddr_in& address) {
    std::memset(memory.data() + 0x100, 0, 16);
    memory[0x101] = 2;
    std::memcpy(memory.data() + 0x102, &address.sin_port, 2);
    std::memcpy(memory.data() + 0x104, &address.sin_addr, 4);
}
void WaitReadable(uint32_t handle) {
    for (unsigned poll = 0; poll < 1000; ++poll) {
        kernel::W32(0x300, 1u << handle);
        kernel::W32(0x304, 0);
        if (Call(sub_00166BD0, {handle + 1, 0x300, 0, 0, 0}, true) == 1) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    assert(false && "received payload not reported by select");
}
}

int main() {
    kernel::g_base = memory.data();
    setenv("SEGAGOLF_LOCAL_IP", "127.0.0.16", 1);
    sockaddr_in address{};
    const int reservation = HostSocket(false, address);
    close(reservation);
    assert(Call(sub_00152F00, {0, 1, 0}) == kError);
    const auto first = Call(sub_00152F00, {2, 1, 0});
    assert(first == 0);
    kernel::W32(0x200, 1);
    assert(Call(sub_00152D80, {first, 0xFFFF, 4, 0x200, 4}) == 0);

    // Reproduce the BSD sockaddr built by the original listener at 0xDFD88.
    GuestAddress(address);
    assert(Call(sub_001508A0, {first, 0x100, 16}) == 0);
    assert(Call(sub_001514E0, {first, 0}) == 0);
    assert(Call(sub_00150730, {first, 0, 0}) == 0);
    for (unsigned poll = 0; poll < 100; ++poll)
        assert(Call(sub_0014FFA0, {first}) == 0);
    assert(Call(sub_00153070, {first}) == kBusy);
    assert(Call(sub_00150730, {first, 0, 0}) == kError);

    const auto second = Call(sub_00152F00, {2, 1, 6});
    assert(second == 1);
    assert(Call(sub_001508A0, {second, 0x100, 16}) == kError);
    assert(Call(sub_00150980, {first}) == 0);
    assert(Call(sub_0014FFA0, {first}) == kError);
    assert(Call(sub_001508A0, {second, 0x100, 16}) == 0);
    assert(Call(sub_00150980, {second}) == 0);

    // Real TCP handshake and exact payload through the original DMA ABI.
    const auto listener = Call(sub_00152F00, {2, 1, 0});
    assert(Call(sub_001508A0, {listener, 0x100, 16}) == 0);
    assert(Call(sub_001514E0, {listener, 1}) == 0);
    assert(Call(sub_00150730, {listener, 0, 0}) == 0);
    const auto client = Call(sub_00152F00, {2, 1, 0});
    assert(Call(sub_00152E60, {client, 1000, 1000, 1000}) == 0);
    assert(Call(sub_00150B50, {client, 0x100, 16}) == 0);
    assert(Finish(client) == 0);
    const auto accepted = Finish(listener);
    assert(accepted < 64 && accepted != client && accepted != listener);
    constexpr char payload[] = "original cabinet bytes";
    std::memcpy(memory.data() + 0x240, payload, sizeof(payload));
    assert(Call(sub_00150340, {0x240, 8192, sizeof(payload)}) == sizeof(payload));
    assert(Call(sub_001527A0, {client, 8192, sizeof(payload), 0}) == 0);
    assert(Finish(client) == sizeof(payload));
    WaitReadable(accepted);
    assert(Call(sub_00151910, {accepted, 16384, sizeof(payload), 0}) == 0);
    assert(Finish(accepted) == sizeof(payload));
    assert(Call(sub_00150170, {16384, 0x280, sizeof(payload)}) == sizeof(payload));
    assert(!std::memcmp(memory.data() + 0x280, payload, sizeof(payload)));
    assert(Call(sub_00150340, {0x240, 64 * 8192 - 1, 2}) == kError);
    for (auto handle : {client, accepted, listener}) assert(Call(sub_00150980, {handle}) == 0);

    // Real UDP request/reply, BSD source address and nonblocking readiness.
    const int host = HostSocket(true, address);
    GuestAddress(address);
    const auto udp = Call(sub_00152F00, {2, 2, 0});
    assert(Call(sub_00152A60, {udp, 8192, sizeof(payload), 0, 0x100, 16}) == 0);
    assert(Finish(udp) == sizeof(payload));
    pollfd readable{host, POLLIN, 0};
    assert(poll(&readable, 1, 1000) == 1);
    char received[64]{};
    socklen_t length = sizeof(address);
    assert(recvfrom(host, received, sizeof(received), 0, reinterpret_cast<sockaddr*>(&address), &length) == sizeof(payload));
    assert(!std::memcmp(received, payload, sizeof(payload)));
    assert(sendto(host, received, sizeof(payload), 0, reinterpret_cast<sockaddr*>(&address), length) == sizeof(payload));
    WaitReadable(udp);
    kernel::W32(0x220, 16);
    assert(Call(sub_00151C50, {udp, 16384, sizeof(payload), 0, 0x200, 0x220}) == 0);
    assert(Finish(udp) == sizeof(payload));
    assert(memory[0x201] == 2 && memory[0x204] == 127);
    assert(kernel::R32(0x220) == 16);
    assert(Call(sub_00150170, {16384, 0x280, sizeof(payload)}) == sizeof(payload));
    assert(!std::memcmp(memory.data() + 0x280, payload, sizeof(payload)));
    assert(Call(sub_00150980, {udp}) == 0);
    close(host);
    const auto dns = Call(sub_00152F00, {2, 2, 0});
    assert(dns == 0);
    assert(Call(sub_00150E20, {0x200, 0, 0, dns}) == kError);
    assert(Call(sub_0014FFA0, {dns}) == 1);
    assert(Call(sub_00150980, {dns}) == 0);

    // Descriptor exhaustion and release must not corrupt the game's 64-slot
    // handle space, and closing a listener cancels its pending operation.
    for (unsigned handle = 0; handle < 64; ++handle)
        assert(Call(sub_00152F00, {2, 1, 0}) == handle);
    assert(Call(sub_00152F00, {2, 1, 0}) == kError);
    for (unsigned handle = 0; handle < 64; ++handle)
        assert(Call(sub_00150980, {handle}) == 0);
    assert(Call(sub_00150980, {0}) == kError);
    std::puts("Sega Golf socket ABI, real TCP/UDP payload, DMA and select checks passed");
}
