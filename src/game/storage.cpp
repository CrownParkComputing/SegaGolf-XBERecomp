#include "kernel/kernel.h"
#include <array>
#include <ctime>
#include <mutex>

namespace {
std::mutex storage_lock;
template <size_t N> struct Storage {
    std::array<uint8_t, N> bytes;
    Storage() { bytes.fill(0xFF); }
};
Storage<0x2000> eeprom;
Storage<0x20000> backup;
Storage<0x10000> controller_firmware;

template <size_t N>
void Transfer(X86Context& ctx, Storage<N>& storage, bool write, const char* name) {
    const kernel::Args args{ctx};
    const uint32_t offset = args[0], bytes = args[1], buffer = args[2];
    if (!buffer || offset > N || bytes > N - offset) {
        kernel::Return(ctx, kernel::kInvalidParameter, 4);
        return;
    }
    std::lock_guard guard(storage_lock);
    if (write) std::memcpy(storage.bytes.data() + offset, kernel::HostPtr(buffer), bytes);
    else std::memcpy(kernel::HostPtr(buffer), storage.bytes.data() + offset, bytes);
    LOG_FIRST(12, "SegaGolf: virtual %s %s at %X, %u bytes", name, write ? "write" : "read", offset, bytes);
    kernel::Return(ctx, 0, 4);
}
}

// Public JVS storage APIs: offset, byte count, buffer, wait flag; stdcall
// ret 0x10. Keep the original acLib initialization, checksums and credit
// manager active while supplying the storage that was on the arcade board.
// These initial buffers are volatile; cabinet persistence is not implemented.
X86_FUNC(sub_0022DDE0) { Transfer(ctx, eeprom, false, "EEPROM"); }
X86_FUNC(sub_0022DFC0) { Transfer(ctx, eeprom, true, "EEPROM"); }
X86_FUNC(sub_0022E1A0) { Transfer(ctx, backup, false, "backup SRAM"); }
X86_FUNC(sub_0022E2F0) { Transfer(ctx, backup, true, "backup SRAM"); }
X86_FUNC(sub_0022D220) { Transfer(ctx, controller_firmware, false, "controller firmware"); }
X86_FUNC(sub_0022D460) { Transfer(ctx, controller_firmware, true, "controller firmware"); }

// JvsRTC_Read(offset, bytes, buffer, wait), ret 0x10 at 0x22EB17.
// The Chihiro RTC stores calendar fields in BCD.
X86_FUNC(sub_0022EA50) {
    const kernel::Args args{ctx};
    const uint32_t offset = args[0], bytes = args[1], buffer = args[2];
    if (!buffer || offset > 16 || bytes > 16 - offset) {
        kernel::Return(ctx, kernel::kInvalidParameter, 4);
        return;
    }
    const auto now = std::time(nullptr);
    std::tm utc{};
    gmtime_r(&now, &utc);
    const auto bcd = [](int value) { return uint8_t((value / 10) * 16 + value % 10); };
    std::array<uint8_t, 16> data{};
    data[0] = bcd(utc.tm_sec); data[1] = bcd(utc.tm_min);
    data[6] = bcd(utc.tm_hour); data[7] = bcd(utc.tm_wday);
    data[8] = bcd(utc.tm_mday); data[9] = bcd(utc.tm_mon + 1);
    data[10] = bcd((utc.tm_year + 1900) % 100); data[11] = bcd((utc.tm_year + 1900) / 100);
    std::memcpy(kernel::HostPtr(buffer), data.data() + offset, bytes);
    LOG_FIRST(1, "SegaGolf: virtual cabinet RTC uses host UTC");
    kernel::Return(ctx, 0, 4);
}
