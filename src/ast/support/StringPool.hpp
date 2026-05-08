#pragma once

#include "InternedString.hpp"
#include <string_view>
#include <vector>
#include <unordered_map>
#include <memory>

class StringPool {
    std::unordered_map<std::string_view, uint32_t> internMap;
    std::vector<std::string_view> strings; // Maps ID -> string_view

    // Bump allocator to store the actual string characters contiguously
    std::vector<std::unique_ptr<char[]>> blocks;
    char* currentBlock = nullptr;
    size_t currentOffset = 0;
    static constexpr size_t BLOCK_SIZE = 64 * 1024; // 64 KB blocks

    std::string_view allocateString(std::string_view s);

public:
    StringPool();
    ~StringPool() = default;

    // Disallow copy/move to ensure stable references if needed
    StringPool(const StringPool&) = delete;
    StringPool& operator=(const StringPool&) = delete;

    InternedString intern(std::string_view s);
    std::string_view lookup(InternedString s) const;
};
