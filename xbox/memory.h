#pragma once

// Xbox memory: virtual memory (NtAllocateVirtualMemory and friends), contiguous
// "physical" memory (MmAllocateContiguousMemory*) and the physical page budget.

#include <cstdint>

#include "kernel.h"

namespace xbox {

// Reserves the address ranges from `config`. Called by kernel_init.
bool memory_init(const MemoryConfig& config);

// Charges (positive) or returns (negative) pages for memory the kernel does not
// allocate itself: the XBE image and its sections. Returns false if the budget is
// now exceeded; the charge is kept either way.
bool charge_image_pages(int32_t pages);

}  // namespace xbox
