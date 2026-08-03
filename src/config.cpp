#include "config.h"

#include <cstring>

#include "resource.h"

namespace config {
namespace {

bool ReadRaw(const char* path, const char* section, const char* key,
             char (&value)[64]) {
    GetPrivateProfileStringA(section, key, "", value,
                             static_cast<DWORD>(sizeof(value)), path);
    return value[0] != '\0';
}

// GetPrivateProfileString is the only INI reader available here, and the one
// setting in this file is a plain 0 or 1, so it is parsed directly rather than
// through strtol and the process locale.
bool ReadBool(const char* path, const char* section, const char* key,
              bool fallback) {
    char value[64] = {};
    if (!ReadRaw(path, section, key, value))
        return fallback;

    const char* cursor = value;
    while (*cursor == ' ' || *cursor == '\t')
        ++cursor;

    if (*cursor < '0' || *cursor > '9')
        return fallback;

    bool anyNonZero = false;
    while (*cursor >= '0' && *cursor <= '9') {
        if (*cursor != '0')
            anyNonZero = true;
        ++cursor;
    }
    return anyNonZero;
}

}  // namespace

bool GetPath(HMODULE module, char (&path)[MAX_PATH]) {
    const DWORD length = GetModuleFileNameA(module, path, MAX_PATH);
    if (length == 0 || length >= MAX_PATH)
        return false;

    char* dot = std::strrchr(path, '.');
    if (!dot)
        return false;

    const size_t used = static_cast<size_t>(dot - path);
    if (used + 5 > MAX_PATH)
        return false;

    std::memcpy(dot, ".ini", 5);
    return true;
}

void CreateDefault(HMODULE module, const char* path) {
    if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES)
        return;

    const HRSRC resource =
        FindResourceW(module, MAKEINTRESOURCEW(IDR_DEFAULT_INI), RT_RCDATA);
    if (!resource)
        return;

    const HGLOBAL loaded = LoadResource(module, resource);
    const void* data = loaded ? LockResource(loaded) : nullptr;
    const DWORD size = SizeofResource(module, resource);
    if (!data || size == 0)
        return;

    const HANDLE file = CreateFileA(path, GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                    FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return;

    DWORD written = 0;
    const BOOL written_ok = WriteFile(file, data, size, &written, nullptr);
    CloseHandle(file);
    if (!written_ok || written != size)
        DeleteFileA(path);
}

Settings Load(const char* path) {
    Settings settings;
    settings.log = ReadBool(path, "general", "log", false);
    return settings;
}

}  // namespace config
