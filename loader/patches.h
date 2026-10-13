#pragma once

namespace loader {

// Redirects the title's functions that cannot run on a PC (privileged instructions,
// direct kernel/hardware access) to replacements. Call after the image is mapped.
void install_title_patches();

class SymbolTable;

// Redirects the SDK libraries the port replaces (DirectSound, ...), found by name in
// the Ghidra symbols. Returns false if a replaced library is present but incomplete.
bool install_sdk_replacements(const SymbolTable& symbols);

}  // namespace loader
