#pragma once

#include <memory>
#include <vector>
#include <utility>
#include <algorithm>
#include <cstdint>

struct ASTDeleter {
    void operator()(void*) const { /* no-op */ }
};

template <typename T>
using ASTPtr = std::unique_ptr<T, ASTDeleter>;

class ASTArena {
    std::vector<std::unique_ptr<char[]>> blocks;
    char* currentBlock = nullptr;
    size_t currentOffset = 0;
    static constexpr size_t BLOCK_SIZE = 64 * 1024;

public:
    ASTArena() = default;
    ~ASTArena() = default;

    // Disallow copy/move
    ASTArena(const ASTArena&) = delete;
    ASTArena& operator=(const ASTArena&) = delete;

    template<typename T, typename... Args>
    T* alloc(Args&&... args) {
        constexpr size_t size = sizeof(T);
        constexpr size_t align = alignof(T);

        size_t padding = 0;
        if (currentBlock) {
            uintptr_t addr = reinterpret_cast<uintptr_t>(currentBlock + currentOffset);
            padding = (align - (addr % align)) % align;
        }

        if (!currentBlock || currentOffset + padding + size > BLOCK_SIZE) {
            size_t allocSize = std::max(BLOCK_SIZE, size + align);
            blocks.push_back(std::make_unique<char[]>(allocSize));
            currentBlock = blocks.back().get();
            currentOffset = 0;
            
            uintptr_t addr = reinterpret_cast<uintptr_t>(currentBlock);
            padding = (align - (addr % align)) % align;
        }

        char* ptr = currentBlock + currentOffset + padding;
        currentOffset += padding + size;

        return new(ptr) T(std::forward<Args>(args)...);
    }

    template<typename T, typename... Args>
    ASTPtr<T> make(Args&&... args) {
        return ASTPtr<T>(alloc<T>(std::forward<Args>(args)...));
    }

    template<typename T, typename... Args>
    ASTPtr<T> makeExpr(Args&&... args) { return make<T>(std::forward<Args>(args)...); }

    template<typename T, typename... Args>
    ASTPtr<T> makeStmt(Args&&... args) { return make<T>(std::forward<Args>(args)...); }

    template<typename T, typename... Args>
    ASTPtr<T> makeDecl(Args&&... args) { return make<T>(std::forward<Args>(args)...); }

    template<typename T, typename... Args>
    ASTPtr<T> makeType(Args&&... args) { return make<T>(std::forward<Args>(args)...); }

    template<typename T, typename... Args>
    ASTPtr<T> makePattern(Args&&... args) { return make<T>(std::forward<Args>(args)...); }
};
