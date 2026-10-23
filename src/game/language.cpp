#include "game/language.h"
#include "language_tables.h"
#include "kernel/kernel.h"
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

namespace golf::language {
namespace {
std::atomic<bool> enabled{true};
std::mutex lock;
struct Original { const StringPatch* patch; std::string bytes; };
std::vector<Original> originals;
void Apply(bool english) {
    for (const auto& original : originals) {
        const auto& patch = *original.patch;
        auto* destination = static_cast<char*>(kernel::HostPtr(patch.address));
        std::memset(destination, 0, patch.size + 1);
        const char* source = english ? patch.english : original.bytes.c_str();
        std::memcpy(destination, source, std::strlen(source));
    }
}
}
bool Enabled() { return enabled.load(std::memory_order_relaxed); }
void SetEnabled(bool english) {
    std::lock_guard guard(lock);
    Apply(english);
    enabled.store(english, std::memory_order_relaxed);
}
void Loaded() {
    std::lock_guard guard(lock);
    originals.clear();
    for (const auto& patch : kStrings) {
        const auto* source = static_cast<const char*>(kernel::HostPtr(patch.address));
        if (source[patch.size] || Hash(reinterpret_cast<const uint8_t*>(source), patch.size) != patch.expected ||
            std::strlen(patch.english) > patch.size) {
            LOGE("SegaGolf English: original string mismatch at %08X", patch.address);
            continue;
        }
        originals.push_back({&patch, std::string(source, patch.size)});
    }
    const char* requested = std::getenv("SEGAGOLF_LANGUAGE");
    const bool english = !requested || std::strcmp(requested, "ja") != 0;
    Apply(english);
    enabled.store(english, std::memory_order_relaxed);
    LOGR("SegaGolf language: %s; %zu validated messages, %zu sprite labels",
         english ? "English" : "Japanese", originals.size(), std::size(kSprites));
}
}
