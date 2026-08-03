#pragma once

#include <windows.h>

namespace config {

struct Settings {
    bool log = false;
};

// Builds "<module directory>\<module name>.ini". Returns false when the path
// does not fit into MAX_PATH.
bool GetPath(HMODULE module, char (&path)[MAX_PATH]);

// Writes the canonical configuration embedded at build time when the file is
// missing. The resource is compiled from Config\VehiclePedDamageFix.ini, so
// the generated file is byte for byte identical to it.
void CreateDefault(HMODULE module, const char* path);

Settings Load(const char* path);

}  // namespace config
