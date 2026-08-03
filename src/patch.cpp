#include "patch.h"

#include <windows.h>

#include <cstring>

namespace patch {
namespace {

constexpr DWORD kReadableProtect = PAGE_READONLY | PAGE_READWRITE |
                                   PAGE_WRITECOPY | PAGE_EXECUTE_READ |
                                   PAGE_EXECUTE_READWRITE |
                                   PAGE_EXECUTE_WRITECOPY;

}  // namespace

bool IsReadable(uintptr_t address, size_t size) {
    MEMORY_BASIC_INFORMATION info = {};
    const auto* pointer = reinterpret_cast<const void*>(address);
    if (VirtualQuery(pointer, &info, sizeof(info)) != sizeof(info))
        return false;
    if (info.State != MEM_COMMIT || (info.Protect & kReadableProtect) == 0)
        return false;

    const auto regionEnd =
        reinterpret_cast<uintptr_t>(info.BaseAddress) + info.RegionSize;
    return address + size <= regionEnd;
}

bool BytesEqual(uintptr_t address, const void* data, size_t size) {
    if (!IsReadable(address, size))
        return false;
    return std::memcmp(reinterpret_cast<const void*>(address), data, size) == 0;
}

bool ReadInt32(uintptr_t address, int32_t& value) {
    if (!IsReadable(address, sizeof(int32_t)))
        return false;
    value = *reinterpret_cast<const int32_t*>(address);
    return true;
}

bool WriteMemory(uintptr_t address, const void* data, size_t size) {
    auto* destination = reinterpret_cast<void*>(address);

    DWORD oldProtect = 0;
    if (!VirtualProtect(destination, size, PAGE_EXECUTE_READWRITE, &oldProtect))
        return false;

    std::memcpy(destination, data, size);
    FlushInstructionCache(GetCurrentProcess(), destination, size);

    DWORD restored = 0;
    VirtualProtect(destination, size, oldProtect, &restored);
    return true;
}

}  // namespace patch
