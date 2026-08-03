#pragma once

#include <cstddef>
#include <cstdint>

namespace patch {

// True when the whole range is committed and readable.
bool IsReadable(uintptr_t address, size_t size);

// True when `address` holds exactly `data`. False when the range is not
// readable, so an unmapped address never counts as a match.
bool BytesEqual(uintptr_t address, const void* data, size_t size);

template <size_t N>
bool BytesEqual(uintptr_t address, const uint8_t (&data)[N]) {
    return BytesEqual(address, data, N);
}

bool ReadInt32(uintptr_t address, int32_t& value);

bool WriteMemory(uintptr_t address, const void* data, size_t size);

}  // namespace patch
