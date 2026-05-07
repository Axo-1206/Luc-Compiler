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
    
    /**
     * @brief Compute the bitmask of qualifiers that affect type equality
     * @return Bitmask where bits are set for qualifiers where affectsTypeEquality = true
     * 
     * Used by TypeChecker::isEqual() to determine if two function types are
     * considered equal. Qualifiers like ~async affect type identity, while
     * hints like ~noinline do not.
     */
    uint32_t equalityMask() const {
        uint32_t mask = 0;
        for (const auto& [name, info] : qualifiers) {
            if (info.affectsTypeEquality) {
                mask |= info.bit;
                LUC_LOG_SEMANTIC_EXTREME("QualifierRegistry::equalityMask: adding ~" 
                                         << name << " (0x" << std::hex << info.bit << ")");
            }
        }
        return mask;
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
    
    uint32_t asyncBit() const { return getBit("async"); }
    uint32_t parallelBit() const { return getBit("parallel"); }
    uint32_t noinlineBit() const { return getBit("noinline"); }
    uint32_t cdeclBit() const { return getBit("cdecl"); }
    uint32_t stdcallBit() const { return getBit("stdcall"); }
    uint32_t fastcallBit() const { return getBit("fastcall"); }
    uint32_t heapBit() const { return getBit("heap"); }
    uint32_t coldBit() const { return getBit("cold"); }
    
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
        add("async",    1 << 0,  true,  true,  false, false);
        add("parallel", 1 << 1,  true,  true,  false, false);  // ← NEW
        
        // Optimization hints - do NOT affect type equality
        add("noinline", 1 << 2,  false, true,  false, false);
        add("heap",     1 << 6,  false, true,  false, false);
        add("cold",     1 << 7,  false, true,  false, false);
        
        // Calling convention qualifiers - affect type equality (cdecl vs stdcall are different)
        add("cdecl",    1 << 3,  true,  true,  false, false);
        add("stdcall",  1 << 4,  true,  true,  false, false);
        add("fastcall", 1 << 5,  true,  true,  false, false);
        
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
    
    std::unordered_map<std::string, QualifierInfo> qualifiers;
    uint32_t nextBit = 1 << 8;  // Next available bit (for future expansion, e.g., ~gpu = 1<<8)
};