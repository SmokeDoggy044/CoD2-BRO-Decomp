#pragma once

namespace loader {

// Redirects the title's functions that cannot run on a PC (privileged instructions,
// direct kernel/hardware access) to replacements. Call after the image is mapped.
void install_title_patches();

}  // namespace loader
