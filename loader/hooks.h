#pragma once

#include <cstdint>

namespace loader {

// Redirects the original function at `address` to `replacement` by overwriting
// its first five bytes with a JMP. The replacement must use the same calling
// convention (XB_STDCALL, XB_FASTCALL, XB_CDECL or a thunk for LTCG ones).
void install_jump(uint32_t address, const void* replacement);

}  // namespace loader
