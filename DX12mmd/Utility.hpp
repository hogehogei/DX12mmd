#pragma once

#include <cstdint>

size_t AlignmentedSize(size_t size, size_t align)
{
    if (size % align == 0) {
        return size;
    }
    return size + align - (size % align);
}