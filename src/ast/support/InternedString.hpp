#pragma once

#include <cstdint>

/**
 * @brief A lightweight, 32-bit identifier for an interned string.
 *
 * The actual string data is owned by a StringPool. Because the StringPool
 * is bound to a CompilerSession and passed by reference, InternedString does
 * not have a `str()` method itself. You must use `pool.lookup(internedStr)`.
 */
struct InternedString {
    uint32_t id = 0; // 0 represents an invalid or empty string

    InternedString() = default;
    explicit InternedString(uint32_t id) : id(id) {}

    bool operator==(InternedString other) const { return id == other.id; }
    bool operator!=(InternedString other) const { return id != other.id; }

    bool isValid() const { return id != 0; }
};
