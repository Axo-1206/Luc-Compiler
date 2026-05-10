#pragma once

#include <string>
#include <unordered_map>
#include <cstdint>
#include "debug/DebugMacros.hpp"

/**
 * @file QualifierRegistry.hpp
 * 
 * @brief Central registry for all type qualifiers (prefixed with '~')
 * 
 * Type qualifiers modify the behavior or calling convention of function types.
 * They are part of the function's type signature and affect type equality.
 * 
 * Usage in Luc code:
 *   ~async  - Function can suspend (await) and is non-blocking
 *   ~parallel - Function designed for data-parallel execution (SIMD-friendly)
 *   ~noinline - Hint to compiler: do not inline this function
 *   ~cdecl / ~stdcall / ~fastcall - Calling convention for FFI/extern functions
 *   ~heap - Hint: allocate closure on heap (vs stack)
 *   ~cold - Hint: this function is rarely called (optimize for size)
 * 
 * Syntax:
 *   let fetch ~async (url string) string = { ... }
 *   let process ~parallel (data []int) []int = { ... }
 *   type AsyncCallback = ~async (int) string
 * 
 * @note Type qualifiers are parsed as part of FuncTypeAST and stored as a
 *       bitmask for efficient runtime checks. The registry provides:
 *       - Validation of qualifier names
 *       - Bitmask allocation
 *       - Type equality rules (which qualifiers affect type identity)
 */

 // ─────────────────────────────────────────────────────────────────────────────


 /**
 * @namespace QualifierBits
 * @brief Compile-time constants for each type qualifier bit.
 *
 * These constants define the bit positions used in FuncSignature::qualifiers.
 * They are the authoritative mapping from qualifier name to bit mask, used
 * both by QualifierRegistry to assign bits and by FuncSignature for zero-cost
 * testing (e.g., `qualifiers & QualifierBits::Async`).
 *
 * Adding a new qualifier requires:
 *   1. Choose an unused bit here.
 *   2. Add an entry in QualifierRegistry’s constructor using this constant.
 */

namespace QualifierBits {
    constexpr uint32_t Async    = 1 << 0;
    constexpr uint32_t Parallel = 1 << 1;
    constexpr uint32_t CDecl    = 1 << 2;
    constexpr uint32_t StdCall  = 1 << 3;
    constexpr uint32_t FastCall = 1 << 4;
}

// ─────────────────────────────────────────────────────────────────────────────
// QualifierInfo - Metadata for a single type qualifier
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @struct QualifierInfo
 * @brief Metadata for a single type qualifier
 * 
 * Fields:
 *   name                - The qualifier name without '~' (e.g., "async")
 *   bit                 - Unique bit in the qualifier bitmask (1 << N)
 *   affectsTypeEquality - If true, two function types with this qualifier
 *                         set/unset are considered different types
 *   validOnFunction     - Can appear on function types
 *   validOnVariable     - Reserved for future: can appear on variable types
 *   validOnStruct       - Reserved for future: can appear on struct types
 */
struct QualifierInfo {
    std::string name;
    uint32_t bit;
    bool affectsTypeEquality;
    bool validOnFunction;
    bool validOnVariable;
    bool validOnStruct;
};

// ─────────────────────────────────────────────────────────────────────────────
// QualifierRegistry - Singleton registry for all type qualifiers
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @class QualifierRegistry
 * @brief Singleton registry managing all type qualifiers
 * 
 * Responsibilities:
 *   - Maintain a single source of truth for all type qualifiers
 *   - Assign unique bit positions to each qualifier
 *   - Validate qualifier names during semantic analysis
 *   - Compute equality masks (which qualifiers affect type comparison)
 *   - Provide debugging and diagnostic support
 * 
 * Usage example:
 *   const auto& reg = QualifierRegistry::instance();
 *   uint32_t asyncBit = reg.getBit("async");
 *   if (reg.isValid("parallel")) { ... }
 *   std::string all = reg.allNames(); // "~async, ~parallel, ..."
 */
class QualifierRegistry {
public:
    static const QualifierRegistry& instance() {
        static QualifierRegistry inst;
        return inst;
    }
    
    // ─────────────────────────────────────────────────────────────────────────
    // Lookup methods
    // ─────────────────────────────────────────────────────────────────────────
    
    /**
     * @brief Get the bitmask value for a qualifier by name
     * @param name The qualifier name without '~' (e.g., "async")
     * @return The bitmask value (1 << N), or 0 if the qualifier is unknown
     */
    uint32_t getBit(const std::string& name) const {
        auto it = qualifiers.find(name);
        if (it != qualifiers.end()) {
            LUC_LOG_SEMANTIC_EXTREME("QualifierRegistry::getBit: ~" << name 
                                     << " -> 0x" << std::hex << it->second.bit);
            return it->second.bit;
        }
        LUC_LOG_SEMANTIC_EXTREME("QualifierRegistry::getBit: ~" << name << " -> 0 (unknown)");
        return 0;
    }
    
    /**
     * @brief Check if a qualifier name is valid/registered
     * @param name The qualifier name without '~'
     * @return true if the qualifier exists in the registry
     */
    bool isValid(const std::string& name) const {
        bool valid = qualifiers.find(name) != qualifiers.end();
        LUC_LOG_SEMANTIC_EXTREME("QualifierRegistry::isValid: ~" << name 
                                 << " -> " << (valid ? "true" : "false"));
        return valid;
    }
    
    /**
     * @brief Get full metadata for a qualifier
     * @param name The qualifier name without '~'
     * @return Pointer to QualifierInfo, or nullptr if not found
     */
    const QualifierInfo* getInfo(const std::string& name) const {
        auto it = qualifiers.find(name);
        return it != qualifiers.end() ? &it->second : nullptr;
    }

    /**
     * @brief Attempt to set a qualifier's bit in the given mask.
     * @param qualifiers Reference to the qualifier bitmask to modify.
     * @param name The qualifier name without '~' (e.g., "async").
     * @return true if the qualifier was recognized and the bit was set,
     *         false if the qualifier is unknown.
     *
     * Typical parser usage:
     *   if (!QualifierRegistry::instance().applyQualifier(sig.qualifiers, "async"))
     *       error("unknown qualifier ~" + name);
     */
    bool applyQualifier(uint32_t& qualifiers, const std::string& name) const {
        uint32_t bit = getBit(name);
        if (!bit) return false;
        qualifiers |= bit;
        return true;
    }
    
    // ─────────────────────────────────────────────────────────────────────────
    // Diagnostic helpers
    // ─────────────────────────────────────────────────────────────────────────
    
    /**
     * @brief Get a comma-separated list of all registered qualifier names
     * @return String like "~async, ~parallel, ~noinline, ..." for error messages
     */
    std::string allNames() const {
        std::string result;
        for (const auto& [name, info] : qualifiers) {
            if (!result.empty()) result += ", ";
            result += "~" + name;
        }
        LUC_LOG_SEMANTIC_EXTREME("QualifierRegistry::allNames: " << result);
        return result;
    }
    
    /** @brief Return the (cached) bitmask of qualifiers that affect type equality. */
    uint32_t equalityMask() const {
        return cachedEqualityMask;
    }
    
    /**
     * @brief Get all registered qualifiers (for debugging/inspection)
     * @return Const reference to the internal map
     */
    const std::unordered_map<std::string, QualifierInfo>& getAll() const {
        return qualifiers;
    }
    
    // ─────────────────────────────────────────────────────────────────────────
    // Convenience accessors for commonly used qualifiers (codegen optimization)
    // ─────────────────────────────────────────────────────────────────────────
    
    uint32_t asyncBit()    const { return QualifierBits::Async; }
    uint32_t parallelBit() const { return QualifierBits::Parallel; }
    uint32_t cdeclBit()    const { return QualifierBits::CDecl; }
    uint32_t stdcallBit()  const { return QualifierBits::StdCall; }
    uint32_t fastcallBit() const { return QualifierBits::FastCall; }
    
private:
    QualifierRegistry() {
        LUC_LOG_SEMANTIC_VERBOSE("QualifierRegistry: initializing...");
        
        // ─────────────────────────────────────────────────────────────────────
        // SINGLE SOURCE OF TRUTH - Add new qualifiers here only
        // ─────────────────────────────────────────────────────────────────────
        // Format: add(name, bit, affectsTypeEquality, validOnFunction, 
        //              validOnVariable, validOnStruct)
        //
        // Bit allocation:
        //   async:    1 << 0  (bit 0)
        //   parallel: 1 << 1  (bit 1) - NEW
        //   noinline: 1 << 2  (bit 2)
        //   cdecl:    1 << 3  (bit 3)
        //   stdcall:  1 << 4  (bit 4)
        //   fastcall: 1 << 5  (bit 5)
        //   heap:     1 << 6  (bit 6)
        //   cold:     1 << 7  (bit 7)
        //   (bits 8-31 reserved for future)
        // ─────────────────────────────────────────────────────────────────────
        
        // Execution model qualifiers - affect type equality (async vs sync are different types)
        add("async",    QualifierBits::Async,    true,  true,  false, false);
        add("parallel", QualifierBits::Parallel, true,  true,  false, false);
        
        // Calling convention qualifiers - affect type equality (cdecl vs stdcall are different)
        add("cdecl",    QualifierBits::CDecl,    true,  true,  false, false);
        add("stdcall",  QualifierBits::StdCall,  true,  true,  false, false);
        add("fastcall", QualifierBits::FastCall, true,  true,  false, false);
        
        // Compute cached equality mask once after registration
        cachedEqualityMask = 0;
        for (const auto& [name, info] : qualifiers) {
            if (info.affectsTypeEquality) cachedEqualityMask |= info.bit;
        }

        LUC_LOG_SEMANTIC_VERBOSE("QualifierRegistry: initialized " << qualifiers.size() 
                                 << " qualifiers");
    }
    
    /**
     * @brief Internal helper to register a qualifier
     * @param name Qualifier name (without '~')
     * @param bit Unique bit position
     * @param affectsTypeEquality Whether this qualifier changes type identity
     * @param onFunc Valid on function types
     * @param onVar Valid on variable types (reserved)
     * @param onStruct Valid on struct types (reserved)
     */
    void add(const std::string& name, uint32_t bit, bool affectsTypeEquality,
             bool onFunc, bool onVar, bool onStruct) {
        qualifiers[name] = {name, bit, affectsTypeEquality, onFunc, onVar, onStruct};
        LUC_LOG_SEMANTIC_EXTREME("\tRegistered ~" << name 
                                 << " (bit=0x" << std::hex << bit 
                                 << ", affectsType=" << (affectsTypeEquality ? "true" : "false") << ")");
    }
    
    uint32_t cachedEqualityMask = 0;
    std::unordered_map<std::string, QualifierInfo> qualifiers;
    // uint32_t nextBit = 1 << 8;  // Next available bit (for future expansion, e.g., ~gpu = 1<<8)
};