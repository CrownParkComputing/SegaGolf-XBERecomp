#include "xdk/d3d8.h"
#include "kernel/kernel.h"

X86_EXTERN_FUNC(__imp__sub_001944C0);

// The game submits two empty visibility queries at 0x41410 and polls them
// before its first Present. Drain the original Begin/End push commands and
// wait for real renderer reports before running the original result reader.
// The public reader at 0x1944C0 returns with ret 0xC at 0x19451E.
X86_FUNC(sub_001944C0) {
    d3d::FlushGuestCommands();
    __imp__sub_001944C0(ctx, base);
}
