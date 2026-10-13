#include "hooks.h"

#include <cstring>

#include "log.h"

namespace loader {

void install_jump(uint32_t address, const void* replacement) {
    // The image is mapped writable and executable, so no protection change is needed.
    auto* code = reinterpret_cast<uint8_t*>(uintptr_t{address});
    const auto target = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(replacement));
    const uint32_t relative = target - (address + 5);
    code[0] = 0xE9;  // JMP rel32
    std::memcpy(code + 1, &relative, 4);
    LOG_DEBUG("hooks", "%08x -> %08x", address, target);
}

}  // namespace loader
