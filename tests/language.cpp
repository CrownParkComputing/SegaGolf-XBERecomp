#include "game/language.h"
#include "language_tables.h"
#include "kernel/kernel.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <vector>

namespace kernel { uint8_t* g_base; }
namespace {
uint32_t Word(const std::vector<uint8_t>& bytes, size_t at) {
    uint32_t value;
    assert(at + 4 <= bytes.size());
    std::memcpy(&value, bytes.data() + at, 4);
    return value;
}
}
int main(int argc, char** argv) {
    assert(argc == 2);
    using namespace golf::language;
    std::ifstream input(argv[1], std::ios::binary);
    assert(input);
    std::vector<uint8_t> image{std::istreambuf_iterator<char>(input), {}};
    std::vector<uint8_t> memory(0x300000, 0xCC);
    const auto base = Word(image, 0x104), count = Word(image, 0x11C), headers = Word(image, 0x120);
    for (uint32_t index = 0; index < count; ++index) {
        const auto section = headers - base + index * 56;
        const auto address = Word(image, section + 4), offset = Word(image, section + 12), size = Word(image, section + 16);
        if (address + size <= memory.size()) {
            assert(offset + size <= image.size());
            std::memcpy(memory.data() + address, image.data() + offset, size);
        }
    }
    kernel::g_base = memory.data();
    const auto japanese = memory;
    setenv("SEGAGOLF_LANGUAGE", "ja", 1);
    Loaded();
    assert(!Enabled() && memory == japanese);
    std::vector<bool> allowed(memory.size());
    SetEnabled(true);
    assert(Enabled());
    for (const auto& patch : kStrings) {
        assert(Hash(japanese.data() + patch.address, patch.size) == patch.expected);
        assert(std::strlen(patch.english) <= patch.size);
        assert(std::strcmp(reinterpret_cast<const char*>(memory.data() + patch.address), patch.english) == 0);
        for (uint32_t offset = 0; offset <= patch.size; ++offset) allowed[patch.address + offset] = true;
    }
    for (size_t index = 0; index < memory.size(); ++index)
        if (!allowed[index]) assert(memory[index] == japanese[index]);
    SetEnabled(false);
    assert(!Enabled() && memory == japanese);
    SetEnabled(true); SetEnabled(false);
    assert(memory == japanese);
    // A changed original must be left alone, even when other entries apply.
    const auto damaged = kStrings[0].address;
    memory[damaged] ^= 1;
    const auto before = memory;
    Loaded(); SetEnabled(true);
    assert(memory[damaged] == before[damaged]);
    SetEnabled(false); assert(memory == before);
    std::vector<uint8_t> texture(4096);
    for (size_t index = 0; index < texture.size(); ++index) texture[index] = index % 251;
    assert(TextureSignature(texture.data(), texture.size()) == 0x247F2D7B9C840855ULL);
    assert(TextureSignature(texture.data(), 0) == 0);
    for (const auto& sprite : kSprites) {
        assert(sprite.width && sprite.height && sprite.u0 < sprite.u1 && sprite.v0 < sprite.v1);
        assert(sprite.u0 >= 0 && sprite.v0 >= 0 && sprite.u1 <= 1 && sprite.v1 <= 1);
    }
    // Actual first-hole Spin quad UVs. This checks the XST-to-renderer V
    // reversal and prevents a valid atlas from silently missing every label.
    unsigned matches = 0;
    for (const auto& sprite : kSprites) {
        if (Matches(sprite, 0x31700341562FDA21ULL, 1024, 1024,
                    0.8447265625f, 0.6513671875f, 0.9072265625f, 0.6669921875f)) {
            assert(std::strcmp(sprite.english, "Spin") == 0);
            ++matches;
        }
        assert(!Matches(sprite, 0, 1024, 1024, 0, 0, 1, 1));
    }
    assert(matches == 1);
    bool distance_checked = false, shots_checked = false;
    for (const auto& sprite : kSprites) {
        if (std::strcmp(sprite.english, "To go") == 0) {
            const auto box = Bounds(sprite, 0, 0, 74, 16);
            assert(box.x0 == 0 && box.x1 <= 30); // distance digits start farther right
            distance_checked = true;
        }
        if (std::strcmp(sprite.english, "Shots\nleft") == 0 && sprite.width == 1024) {
            const auto box = Bounds(sprite, 0, 0, 128, 64);
            assert(box.x1 < 64); // leave the live number in the right-hand circle visible
            shots_checked = true;
        }
    }
    assert(distance_checked && shots_checked);
    std::puts("English messages fit original slots, preserve surrounding memory, restore Japanese, and reject altered originals; sprite signatures match Python");
}
