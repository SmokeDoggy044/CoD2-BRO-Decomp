#pragma once

// Shared between the D3D front end's files (device.cpp, resource.cpp, ...).

#include <cstddef>
#include <iterator>

#include "kernel.h"
#include "types.h"

namespace render {

using xbox::XbPtr;

#define D3D_IMPL(name) {#name, reinterpret_cast<void*>(&name)}

// Each file's replacements; d3d_functions() returns them all.
extern const xbox::KernelImpl kDeviceFunctions[];
extern const size_t kDeviceFunctionCount;
extern const xbox::KernelImpl kResourceFunctions[];
extern const size_t kResourceFunctionCount;

// A resource's last reference is gone: drop anything the back end made from it.
void resource_destroyed(XbPtr resource);

}  // namespace render
