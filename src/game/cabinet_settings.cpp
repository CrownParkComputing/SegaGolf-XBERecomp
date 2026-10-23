#include "kernel/kernel.h"
#include <cstdlib>
#include <cstring>

X86_EXTERN_FUNC(__imp__sub_0001B030);

// The original backup reader selects factory defaults when erased. Apply an
// explicitly selected cabinet role after that reader, before any network tasks
// are initialized. These are the game's own operator settings, not wait flags.
X86_FUNC(sub_0001B030) {
    __imp__sub_0001B030(ctx, base);
    const char* role = std::getenv("SEGAGOLF_ROLE");
    if (!role) return;
    uint32_t mode;
    uint8_t id;
    if (!std::strcmp(role, "server")) { mode = 2; id = 0; }
    else if (!std::strcmp(role, "main")) { mode = 0; id = 1; }
    else if (!std::strcmp(role, "sub")) { mode = 1; id = 2; }
    else { LOGE("SegaGolf: unknown SEGAGOLF_ROLE '%s'", role); return; }
    kernel::W32(0x4C125C, mode);
    kernel::G<uint8_t>(0x4C1260) = id;
    LOGI("SegaGolf: operator role=%s mode=%u id=%u", role, mode, id);
}
