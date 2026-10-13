#pragma once

// Direct3D 9: the system's on Windows, DXVK's (dxvk-native) on Linux.

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include <d3d9.h>
