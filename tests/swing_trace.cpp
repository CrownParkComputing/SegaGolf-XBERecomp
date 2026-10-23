// Input-smoke builds only: observe the original routines without changing input
// contexts, calibration, gameplay flags or shot results.
#include "kernel/kernel.h"
X86_EXTERN_FUNC(__imp__sub_00083160);
X86_EXTERN_FUNC(__imp__sub_000836B0);
X86_FUNC(sub_00083160) {
    __imp__sub_00083160(ctx, base);
    LOGR("SegaGolf swing calibration: start=%u rest=%u full=%u",
         kernel::R32(0x678A68), kernel::R32(0x678A6C), kernel::R32(0x678A70));
}
X86_FUNC(sub_000836B0) {
    const uint32_t input = ctx.ecx;
    __imp__sub_000836B0(ctx, base);
    if (input != 0x6777AC) return;
    static uint16_t last = 0;
    const auto raw = kernel::R16(input + 0xB4);
    if (raw != last) {
        LOGR("SegaGolf swing sensor: raw=%u average=%u velocity=%.0f forward=%u rest=%u",
             raw, kernel::R16(input + 0x204), kernel::G<float>(input + 0x208),
             kernel::G<uint8_t>(input + 0x20C), kernel::G<uint8_t>(input + 0x20D));
        last = raw;
    }
}
