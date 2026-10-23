#include "kernel/kernel.h"
#include <cstdlib>
#include <sstream>

X86_EXTERN_FUNC(__imp__sub_00053FE0);
X86_EXTERN_FUNC(__imp__sub_000C6A90);

namespace {
bool TraceStartup() {
    static const bool enabled = std::getenv("SEGAGOLF_TRACE_STARTUP") != nullptr;
    return enabled;
}

std::string GuestString(uint32_t address) {
    std::string result;
    for (unsigned i = 0; address && i < 256 && mem::IsCommitted(address + i); ++i) {
        const auto ch = kernel::G<uint8_t>(address + i);
        if (!ch) break;
        if (ch < 32 && ch != '\n' && ch != '\r' && ch != '\t') return {};
        result += char(ch);
    }
    return result;
}

void TraceTasks() {
    using kernel::R32;
    LOGI("SegaGolf startup phase=%u timer=%u lib enabled=%u state=%u ready=%02X/%02X",
         R32(0x7CA7A8), R32(0x7CA7D8), R32(0x8EF808), R32(0x8EF818),
         kernel::G<uint8_t>(0x8EF848), kernel::G<uint8_t>(0x8EF849));
    LOGI("SegaGolf startup timeout=%d blockers=%u services=%d/%d/%d/%d file-load=%u",
         int32_t(R32(0x7CA7B0)), R32(0x7CA7B4), int32_t(R32(0x7CA7B8)),
         int32_t(R32(0x7CA7BC)), int32_t(R32(0x7CA7C0)), int32_t(R32(0x7CA7C4)), R32(0x7CA7C8));
    LOGI("SegaGolf touch startup=%d ready=%u error=%d", int32_t(R32(0x718BA8)), R32(0x718BAC), int32_t(R32(0x718B44)));
    LOGI("SegaGolf auth state=%u DNS state=%d socket=%d budget=%d HTTP=%u", R32(0x6DE750),
         int32_t(R32(0x6DE764)), int32_t(R32(0x6DE768)), int32_t(R32(0x4E5CC8)), R32(0x6762E4));
    LOGI("SegaGolf matching enabled=%u mode=%u id=%u state=%u/%u object=%08X connected=%u queue=%u/%u op=%u",
         kernel::G<uint8_t>(0x299848), R32(0x4C125C), kernel::G<uint8_t>(0x4C1260), R32(0x6B9920), R32(0x6B9924),
         R32(0x696494), R32(0x6963E0), R32(0x69909C), R32(0x6990A0), R32(0x6990A4));
    const uint32_t root = R32(0x52EB38);
    uint32_t task = root;
    for (unsigned i = 0; task && i < 96; ++i) {
        if (!mem::IsCommitted(task) || !mem::IsCommitted(task + 0x50)) break;
        const uint32_t argument = R32(task + 0x14);
        std::ostringstream words;
        for (unsigned j = 0; argument && j < 8 && mem::IsCommitted(argument + j * 4); ++j)
            words << std::hex << R32(argument + j * 4) << ' ';
        LOGI("SegaGolf task %08X '%s' flags=%X update=%08X arg=%08X [%s]",
             task, GuestString(R32(task + 0x18)).c_str(), R32(task + 4),
             R32(task + 0x30), argument, words.str().c_str());
        // Original depth-first successor at 0x53E60: child, sibling, parent.
        if (R32(task + 0x44)) task = R32(task + 0x44);
        else {
            unsigned depth = 0;
            while (task && !R32(task + 0x4C) && ++depth < 96) {
                task = R32(task + 0x40);
                if (task == root) { task = 0; break; }
                if (task && !mem::IsCommitted(task + 0x50)) { task = 0; break; }
            }
            task = task && depth < 96 ? R32(task + 0x4C) : 0;
        }
    }
}
}

// This build compiles its variadic debug logger to a bare ret at 0x3F680.
// Trace format and raw arguments without interpreting guest variadic types or
// changing the original registers, flags, or caller-owned argument stack.
X86_FUNC(sub_0003F680) {
    if (TraceStartup()) {
        const kernel::Args args{ctx};
        LOGI("SegaGolf trace %08X: %s | %08X %08X %08X %08X",
             kernel::R32(ctx.esp), GuestString(args[0]).c_str(), args[1], args[2], args[3], args[4]);
    }
    ctx.esp += 4;
}

// Observe the task scheduler; always run its original implementation.
X86_FUNC(sub_00053FE0) {
    if (TraceStartup()) {
        static unsigned frames = 0;
        if (frames++ % 120 == 0) TraceTasks();
    }
    __imp__sub_00053FE0(ctx, base);
}

X86_FUNC(sub_000C6A90) {
    __imp__sub_000C6A90(ctx, base);
    if (TraceStartup()) {
        static uint8_t previous = 0;
        const auto state = kernel::G<uint8_t>(0x718AA4);
        if (state != previous) {
            previous = state;
            LOGI("SegaGolf original touch parser: state=%02X x=%.1f y=%.1f", state,
                 kernel::G<float>(0x718A88), kernel::G<float>(0x718A8C));
        }
    }
}
