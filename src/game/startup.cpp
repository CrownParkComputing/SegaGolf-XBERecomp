#include "kernel/kernel.h"
#include <atomic>

namespace {
// The native host schedules its threads. Preserve the title's process quantum
// property without dereferencing the absent Xbox KPROCESS through KTHREAD+44.
std::atomic<uint32_t> quantum_ticks{0};
}

// XSetProcessQuantumLength: 0xE7629..0xE763C takes milliseconds, multiplies
// by three, writes KPROCESS+14, and returns with ret 4.
X86_FUNC(sub_000E7629) {
    const kernel::Args args{ctx};
    quantum_ticks.store(args[0] * 3u);
    LOG_FIRST(1, "SegaGolf: requested process quantum %u ms; host schedules threads", args[0]);
    kernel::Return(ctx, 0, 1);
}

// The matching getter at 0xE763F divides the stored ticks by three; ret.
X86_FUNC(sub_000E763F) {
    kernel::Return(ctx, quantum_ticks.load() / 3u, 0);
}

// OutputDebugStringA uses console debugger interrupts 2D/3 at 0xE723B;
// the verified public wrapper returns with ret 4 at 0xE723F.
X86_FUNC(sub_000E7210) {
    const kernel::Args args{ctx};
    LOGI("SegaGolf debug: %s", kernel::ReadCString(args[0]).c_str());
    kernel::Return(ctx, 0, 1);
}
