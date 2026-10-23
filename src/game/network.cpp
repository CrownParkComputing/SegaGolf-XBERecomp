#include "kernel/kernel.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdlib>
#include <mutex>
#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {
// acLib Ethernet uses a physical board mailbox, separately from Xbox XNet.
// Preserve its asynchronous API and DMA buffers, using real loopback sockets.
// The original game produces and consumes every packet; no replies are invented.
enum class Pending { None, Accept, Connect, Receive, ReceiveFrom };
struct Socket {
    int fd = -1;
    bool udp = false, bound = false, listening = false;
    Pending pending = Pending::None;
    int result = 0, error = 0;
    uint32_t offset = 0, size = 0, flags = 0, peer = 0, peer_size = 0;
};
std::array<Socket, 64> sockets;
std::array<uint8_t, 64 * 8192> dma{};
std::mutex network_lock;
constexpr uint32_t kError = uint32_t(-1), kBusy = uint32_t(-2);
Socket* Get(uint32_t handle) {
    return handle < sockets.size() && sockets[handle].fd >= 0 ? &sockets[handle] : nullptr;
}
int Allocate(int fd, bool udp) {
    if (fd < 0) return -1;
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
    fcntl(fd, F_SETFD, FD_CLOEXEC);
    for (unsigned i = 0; i < sockets.size(); ++i) {
        if (sockets[i].fd >= 0) continue;
        sockets[i] = Socket{.fd = fd, .udp = udp};
        return i;
    }
    close(fd);
    return -1;
}
in_addr LocalAddress() {
    in_addr address{};
    const char* configured = std::getenv("SEGAGOLF_LOCAL_IP");
    if (!configured || inet_pton(AF_INET, configured, &address) != 1 || (ntohl(address.s_addr) >> 24) != 127)
        inet_pton(AF_INET, "127.0.0.2", &address);
    return address;
}
bool Address(uint32_t guest, uint32_t length, sockaddr_in& address) {
    // BSD sockaddr has separate length/family bytes, then network-order port/IP.
    if (!guest || length != 16 || kernel::G<uint8_t>(guest + 1) != 2) return false;
    address = {}; address.sin_family = AF_INET;
    address.sin_port = kernel::R16(guest + 2);
    address.sin_addr.s_addr = kernel::R32(guest + 4);
    return true;
}
bool Loopback(const sockaddr_in& address) { return (ntohl(address.sin_addr.s_addr) >> 24) == 127; }
void WriteAddress(uint32_t guest, uint32_t length, const sockaddr_in& address) {
    if (!guest || !length) return;
    std::array<uint8_t, 16> bytes{16, 2};
    std::memcpy(bytes.data() + 2, &address.sin_port, 2);
    std::memcpy(bytes.data() + 4, &address.sin_addr, 4);
    std::memcpy(kernel::HostPtr(guest), bytes.data(), std::min(16u, kernel::R32(length)));
    kernel::W32(length, 16);
}
bool BindLocal(Socket& socket) {
    if (socket.bound) return true;
    sockaddr_in address{}; address.sin_family = AF_INET; address.sin_addr = LocalAddress();
    if (bind(socket.fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) return false;
    socket.bound = true;
    return true;
}
bool Range(uint32_t offset, uint32_t size) { return offset <= dma.size() && size <= dma.size() - offset; }
int GuestError(int error) {
    switch (error) {
    case EAGAIN: return 35; // BSD EWOULDBLOCK
    case EINPROGRESS: return 36;
    case ECONNREFUSED: return 61;
    case ENETUNREACH: return 51;
    case ECONNRESET: return 54;
    default: return error;
    }
}
void Complete(Socket& socket, int result, int error = 0) {
    socket.pending = Pending::None; socket.result = result; socket.error = GuestError(error);
}
void Poll(Socket& socket) {
    sockaddr_in peer{}; socklen_t peer_size = sizeof(peer);
    switch (socket.pending) {
    case Pending::None: return;
    case Pending::Accept: {
        const int fd = accept(socket.fd, reinterpret_cast<sockaddr*>(&peer), &peer_size);
        if (fd < 0 && (errno == EAGAIN || errno == EINTR)) return;
        if (fd < 0) { Complete(socket, -1, errno); return; }
        WriteAddress(socket.peer, socket.peer_size, peer);
        const int handle = Allocate(fd, false);
        if (handle >= 0) sockets[handle].bound = true;
        Complete(socket, handle, handle < 0 ? EMFILE : 0);
        LOGI("SegaGolf: cabinet accepted socket %d", handle);
        return;
    }
    case Pending::Connect: {
        pollfd descriptor{socket.fd, POLLOUT, 0};
        if (poll(&descriptor, 1, 0) <= 0) return;
        int error = 0; socklen_t size = sizeof(error);
        if (getsockopt(socket.fd, SOL_SOCKET, SO_ERROR, &error, &size) < 0) error = errno;
        Complete(socket, error ? -1 : 0, error);
        return;
    }
    case Pending::Receive:
    case Pending::ReceiveFrom: {
        const int result = recvfrom(socket.fd, dma.data() + socket.offset, socket.size,
            socket.flags & 2 ? MSG_PEEK : 0, reinterpret_cast<sockaddr*>(&peer), &peer_size);
        if (result < 0 && (errno == EAGAIN || errno == EINTR)) return;
        if (result >= 0 && socket.pending == Pending::ReceiveFrom) WriteAddress(socket.peer, socket.peer_size, peer);
        Complete(socket, result, result < 0 ? errno : 0);
        return;
    }
    }
}
void Receive(X86Context& ctx, bool from) {
    const kernel::Args args{ctx}; std::lock_guard guard(network_lock);
    auto* socket = Get(args[0]);
    if (!socket || socket->pending != Pending::None || !Range(args[1], args[2]) ||
        (from && bool(args[4]) != bool(args[5]))) {
        kernel::Return(ctx, kError, from ? 6 : 4); return;
    }
    socket->pending = from ? Pending::ReceiveFrom : Pending::Receive;
    socket->offset = args[1]; socket->size = args[2]; socket->flags = args[3];
    socket->peer = from ? args[4] : 0; socket->peer_size = from ? args[5] : 0;
    Poll(*socket);
    kernel::Return(ctx, 0, from ? 6 : 4);
}
void Send(X86Context& ctx, bool to) {
    const kernel::Args args{ctx}; std::lock_guard guard(network_lock);
    auto* socket = Get(args[0]); sockaddr_in address{};
    if (!socket || socket->pending != Pending::None || !Range(args[1], args[2]) ||
        (to && !Address(args[4], args[5], address))) {
        kernel::Return(ctx, kError, to ? 6 : 4); return;
    }
    int result = -1, error = 0;
    if (to && socket->udp && !BindLocal(*socket)) error = errno;
    else if (to && address.sin_addr.s_addr == INADDR_BROADCAST) {
        // Broadcast unchanged packets across the virtual cabinet LAN.
        for (unsigned id = 1; id <= 16; ++id) {
            address.sin_addr.s_addr = htonl(0x7F000000u | id);
            result = sendto(socket->fd, dma.data() + args[1], args[2], MSG_NOSIGNAL,
                reinterpret_cast<sockaddr*>(&address), sizeof(address));
            if (result < 0) { error = errno; break; }
        }
    } else if (to && !Loopback(address)) error = ENETUNREACH;
    else if (to) result = sendto(socket->fd, dma.data() + args[1], args[2], MSG_NOSIGNAL,
        reinterpret_cast<sockaddr*>(&address), sizeof(address));
    else result = send(socket->fd, dma.data() + args[1], args[2], MSG_NOSIGNAL);
    if (result < 0 && !error) error = errno;
    Complete(*socket, result, error);
    LOG_FIRST(24, "SegaGolf: cabinet %s socket=%u bytes=%u result=%d error=%d", to ? "sendto" : "send", args[0], args[2], result, error);
    kernel::Return(ctx, 0, to ? 6 : 4);
}
}

namespace golf::network {
void UpdateBoardAddress() {
    // Baseboard network-info +20 is the IPv4 address (D47B9..D47D7).
    kernel::W32(0x8EF8B4, LocalAddress().s_addr);
}
}

// stdcall cleanup counts are verified against each original ret N.
X86_FUNC(sub_00152F00) { // socket, ret C at 152F9D
    const kernel::Args args{ctx}; std::lock_guard guard(network_lock);
    const bool tcp = args[1] == 1 && (args[2] == 0 || args[2] == 6);
    const bool udp = args[1] == 2 && (args[2] == 0 || args[2] == 17);
    const int handle = args[0] == 2 && (tcp || udp) ? Allocate(socket(AF_INET, udp ? SOCK_DGRAM : SOCK_STREAM, 0), udp) : -1;
    LOG_FIRST(24, "SegaGolf: loopback cabinet socket type=%u -> %d", args[1], handle);
    kernel::Return(ctx, handle, 3);
}
X86_FUNC(sub_00152D80) { // setsockopt, ret 14 at 152E50
    const kernel::Args args{ctx}; std::lock_guard guard(network_lock);
    auto* socket = Get(args[0]);
    const int option = args[2] == 4 ? SO_REUSEADDR : args[2] == 0x20 ? SO_BROADCAST : args[2] == 8 ? SO_KEEPALIVE : 0;
    const int result = socket && args[1] == 0xFFFF && option && args[3] && args[4] == 4 ?
        setsockopt(socket->fd, SOL_SOCKET, option, kernel::HostPtr(args[3]), 4) : -1;
    if (socket) Complete(*socket, result, result < 0 ? EINVAL : 0);
    kernel::Return(ctx, result, 5);
}
X86_FUNC(sub_00152E60) { // settimeout, ret 10 at 152EFA
    const kernel::Args args{ctx}; std::lock_guard guard(network_lock);
    kernel::Return(ctx, Get(args[0]) ? 0 : kError, 4);
}
X86_FUNC(sub_001508A0) { // bind, ret C at 150966
    const kernel::Args args{ctx}; std::lock_guard guard(network_lock);
    auto* socket = Get(args[0]); sockaddr_in address{}; int result = -1;
    if (socket && !socket->bound && Address(args[1], args[2], address)) {
        address.sin_addr = LocalAddress();
        result = bind(socket->fd, reinterpret_cast<sockaddr*>(&address), sizeof(address));
        if (!result) socket->bound = true;
        Complete(*socket, result, result < 0 ? errno : 0);
        LOGI("SegaGolf: cabinet bind port=%u -> %d", ntohs(address.sin_port), result);
    }
    kernel::Return(ctx, result, 3);
}
X86_FUNC(sub_001514E0) { // listen, ret 8 at 151570
    const kernel::Args args{ctx}; std::lock_guard guard(network_lock);
    auto* socket = Get(args[0]);
    const int result = socket && !socket->udp && socket->bound ? listen(socket->fd, std::max(1u, args[1])) : -1;
    if (socket) { socket->listening = result == 0; Complete(*socket, result, result < 0 ? errno : 0); }
    kernel::Return(ctx, result, 2);
}
X86_FUNC(sub_00150730) { // accept starts async operation, ret C at 15082E
    const kernel::Args args{ctx}; std::lock_guard guard(network_lock);
    auto* socket = Get(args[0]);
    const bool valid = socket && socket->listening && socket->pending == Pending::None && bool(args[1]) == bool(args[2]);
    if (valid) { socket->pending = Pending::Accept; socket->peer = args[1]; socket->peer_size = args[2]; Poll(*socket); }
    kernel::Return(ctx, valid ? 0 : kError, 3);
}
X86_FUNC(sub_00150B50) { // connect, ret C at 150C32
    const kernel::Args args{ctx}; std::lock_guard guard(network_lock);
    auto* socket = Get(args[0]); sockaddr_in address{}; uint32_t result = kError;
    if (socket && socket->pending == Pending::None && Address(args[1], args[2], address)) {
        int status = -1, error = ENETUNREACH;
        if (Loopback(address) && BindLocal(*socket)) {
            status = connect(socket->fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)); error = status < 0 ? errno : 0;
        }
        if (status < 0 && error == EINPROGRESS) socket->pending = Pending::Connect;
        else Complete(*socket, status, error);
        result = 0;
        LOG_FIRST(24, "SegaGolf: cabinet connect port=%u status=%d errno=%d", ntohs(address.sin_port), status, error);
    }
    kernel::Return(ctx, result, 3);
}
X86_FUNC(sub_0014FFA0) {
    const kernel::Args args{ctx}; std::lock_guard guard(network_lock); auto* socket = Get(args[0]);
    if (socket) Poll(*socket);
    kernel::Return(ctx, socket ? (socket->pending == Pending::None ? 1 : 0) : kError, 1);
}
X86_FUNC(sub_00153070) { // getresult, ret 4 at 1530D8
    const kernel::Args args{ctx}; std::lock_guard guard(network_lock); auto* socket = Get(args[0]);
    if (socket) Poll(*socket);
    kernel::Return(ctx, socket ? (socket->pending == Pending::None ? uint32_t(socket->result) : kBusy) : kError, 1);
}
X86_FUNC(sub_00150C40) { // getlasterror, ret 4
    const kernel::Args args{ctx}; std::lock_guard guard(network_lock); auto* socket = Get(args[0]);
    kernel::Return(ctx, socket ? socket->error : 9, 1);
}
X86_FUNC(sub_00150980) { // close, ret 4 at 150A0B
    const kernel::Args args{ctx}; std::lock_guard guard(network_lock); auto* socket = Get(args[0]);
    if (socket) { close(socket->fd); *socket = {}; }
    kernel::Return(ctx, socket ? 0 : kError, 1);
}
X86_FUNC(sub_00150E20) { // external DNS unavailable, ret 10 at 150F10
    LOG_FIRST(4, "SegaGolf: external cabinet DNS unavailable"); kernel::Return(ctx, kError, 4);
}
X86_FUNC(sub_00150340) { // DMA write(guest,offset,size), ret C at 15042E
    const kernel::Args args{ctx}; std::lock_guard guard(network_lock);
    const bool valid = args[0] && Range(args[1], args[2]);
    if (valid) std::memcpy(dma.data() + args[1], kernel::HostPtr(args[0]), args[2]);
    kernel::Return(ctx, valid ? args[2] : kError, 3);
}
X86_FUNC(sub_00150170) { // DMA read(offset,guest,size), ret C at 150268
    const kernel::Args args{ctx}; std::lock_guard guard(network_lock);
    const bool valid = args[1] && Range(args[0], args[2]);
    if (valid) std::memcpy(kernel::HostPtr(args[1]), dma.data() + args[0], args[2]);
    kernel::Return(ctx, valid ? args[2] : kError, 3);
}
X86_FUNC(sub_00152A60) { Send(ctx, true); }
X86_FUNC(sub_001527A0) { Send(ctx, false); }
X86_FUNC(sub_00151C50) { Receive(ctx, true); }
X86_FUNC(sub_00151910) { Receive(ctx, false); }

// cdecl select normally reads physical poller bits. Retain its descriptor-bitset
// format and nonblocking behavior (the original ignores the timeout argument).
X86_FUNC(sub_00166BD0) {
    const kernel::Args args{ctx}; std::lock_guard guard(network_lock);
    const unsigned count = std::min(64u, args[0]); int ready = 0;
    for (unsigned handle = 0; handle < count; ++handle) {
        auto* socket = Get(handle); pollfd descriptor{socket ? socket->fd : -1, 0, 0};
        const uint32_t mask = 1u << (handle % 32), word = handle / 32 * 4;
        const short events[] = {POLLIN, POLLOUT, POLLPRI};
        for (unsigned set = 1; set <= 3; ++set)
            if (args[set] && (kernel::R32(args[set] + word) & mask)) descriptor.events |= events[set - 1];
        if (descriptor.events) poll(&descriptor, 1, 0);
        for (unsigned set = 1; set <= 3; ++set) {
            if (!args[set] || !(kernel::R32(args[set] + word) & mask)) continue;
            if (descriptor.revents & (events[set - 1] | POLLHUP | POLLERR)) ++ready;
            else kernel::W32(args[set] + word, kernel::R32(args[set] + word) & ~mask);
        }
    }
    kernel::Return(ctx, ready, 0);
}
// cdecl local-IP string getter at the physical board configuration boundary.
X86_FUNC(sub_00165F10) {
    const kernel::Args args{ctx}; const auto address = LocalAddress();
    const bool valid = args[0] && inet_ntop(AF_INET, &address, reinterpret_cast<char*>(kernel::HostPtr(args[0])), 16);
    kernel::Return(ctx, valid ? 0 : kError, 0);
}
