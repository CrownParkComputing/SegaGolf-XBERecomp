#pragma once
#include <mutex>

#include "gpu/state.h"

namespace d3d {

gpu::State& State();
std::recursive_mutex& Lock();
void SetMovieFrame(uint32_t data, uint32_t width, uint32_t height, uint32_t pitch, uint32_t x, uint32_t y);

}  // namespace d3d

namespace d3d {
// Drain native guest push commands before reading a GPU-produced report.
void FlushGuestCommands();
}
