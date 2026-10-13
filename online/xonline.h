#pragma once

// Xbox Live / network (XONLINE, XNET libraries) replacement. Xbox Live is gone, so
// for now the network is simply unavailable: XOnlineStartup fails and every other
// XONLINE/XNET function traps by name if a title reaches it. (System link over the
// local network could come later.)

#include <span>

#include "kernel.h"

namespace online {

std::span<const xbox::KernelImpl> xonline_functions();

}  // namespace online
