/**
 * @file NameMangler.hpp
 * @brief Name mangling for symbols, types, and methods.
 */

#pragma once

#include "ast/support/StringPool.hpp"
#include "ast/TypeAST.hpp"
#include "ast/DeclAST.hpp"
#include "semantic/SymbolTable.hpp"
#include <string>
#include <string_view>

namespace NameMangler {

    // Forward declaration with context
    inline std::string mangleType(TypeAST* type, StringPool& pool, SymbolTable* symbols);
    
    // Helper to unwrap type aliases
    inline TypeAST* unwrapAliases(TypeAST* type, SymbolTable* symbols) {
        if (!type || !symbols) return type;
        
        while (type && type->isa<NamedTypeAST>()) {
            auto* named = type->as<NamedTypeAST>();
            Symbol* sym = symbols->lookup(named->name);
            if (!sym || sym->kind != SymbolKind::TypeAlias) break;
            if (!sym->type) break;
            type = sym->type;
        }
        return type;
    }
    
    // Get primitive kind string
    inline std::string primitiveKindToString(PrimitiveKind kind) {
        switch (kind) {
            case PrimitiveKind::Bool:     return "bool";
            case PrimitiveKind::Byte:     return "byte";
            case PrimitiveKind::Short:    return "short";
            case PrimitiveKind::Int:      return "int";
            case PrimitiveKind::Long:     return "long";
            case PrimitiveKind::Ubyte:    return "ubyte";
            case PrimitiveKind::Ushort:   return "ushort";
            case PrimitiveKind::Uint:     return "uint";
            case PrimitiveKind::Ulong:    return "ulong";
            case PrimitiveKind::Int8:     return "int8";
            case PrimitiveKind::Int16:    return "int16";
            case PrimitiveKind::Int32:    return "int32";
            case PrimitiveKind::Int64:    return "int64";
            case PrimitiveKind::Uint8:    return "uint8";
            case PrimitiveKind::Uint16:   return "uint16";
            case PrimitiveKind::Uint32:   return "uint32";
            case PrimitiveKind::Uint64:   return "uint64";
            case PrimitiveKind::Float:    return "float";
            case PrimitiveKind::Double:   return "double";
            case PrimitiveKind::Decimal:  return "decimal";
            case PrimitiveKind::String:   return "string";
            case PrimitiveKind::Char:     return "char";
            case PrimitiveKind::Any:      return "any";
            default:                      return "unknown";
        }
    }
    
    // Main type mangling function
    inline std::string mangleType(TypeAST* type, StringPool& pool, SymbolTable* symbols) {
        if (!type) return "unknown";
        
        // Unwrap aliases if we have symbol table
        TypeAST* underlying = type;
        if (symbols) {
            underlying = unwrapAliases(type, symbols);
        }
        
        switch (underlying->kind) {
            case ASTKind::PrimitiveType: {
                auto pt = static_cast<PrimitiveTypeAST*>(underlying);
                return "P" + primitiveKindToString(pt->primitiveKind);
            }
            
            case ASTKind::NamedType: {
                auto nt = static_cast<NamedTypeAST*>(underlying);
                std::string res = "N" + std::string(pool.lookup(nt->name));
                if (!nt->genericArgs.empty()) {
                    res += "<";
                    for (size_t i = 0; i < nt->genericArgs.size(); ++i) {
                        if (i > 0) res += ",";
                        res += mangleType(nt->genericArgs[i].get(), pool, symbols);
                    }
                    res += ">";
                }
                return res;
            }
            
            case ASTKind::NullableType: {
                auto nt = static_cast<NullableTypeAST*>(underlying);
                return "O" + mangleType(nt->inner.get(), pool, symbols);
            }
            
            case ASTKind::ResultType: {
                auto rt = static_cast<ResultTypeAST*>(underlying);
                std::string res = "R" + mangleType(rt->inner.get(), pool, symbols);
                if (rt->errorType) {
                    res += "E" + mangleType(rt->errorType.get(), pool, symbols);
                } else {
                    res += "N";  // nil error
                }
                return res;
            }
            
            case ASTKind::ArrayType: {
                auto at = static_cast<ArrayTypeAST*>(underlying);
                std::string prefix;
                switch (at->arrayKind) {
                    case ArrayKind::Slice:   prefix = "A"; break;
                    case ArrayKind::Dynamic: prefix = "D"; break;
                    case ArrayKind::Fixed:   prefix = "F" + std::to_string(at->size); break;
                }
                return prefix + mangleType(at->element.get(), pool, symbols);
            }
            
            case ASTKind::RefType: {
                auto rt = static_cast<RefTypeAST*>(underlying);
                return "Rf" + mangleType(rt->inner.get(), pool, symbols);
            }
            
            case ASTKind::PtrType: {
                auto pt = static_cast<PtrTypeAST*>(underlying);
                return "Pp" + mangleType(pt->inner.get(), pool, symbols);
            }
            
            case ASTKind::FuncType: {
                auto ft = static_cast<FuncTypeAST*>(underlying);
                std::string res = "Fn";
                
                // Add qualifiers
                if (ft->isAsync()) res += "A";
                if (ft->isNullable()) res += "N";
                if (ft->isParallel()) res += "P";
                
                res += "(";
                // Parameters - flatten all groups
                for (const auto& param : ft->sig.allParams) {
                    if (param && param->type) {
                        res += mangleType(param->type.get(), pool, symbols);
                    } else {
                        res += "void";
                    }
                }
                res += ")";
                
                // Return types
                if (ft->sig.returnTypes.empty()) {
                    res += "V";
                } else if (ft->sig.returnTypes.size() == 1) {
                    res += mangleType(ft->sig.returnTypes[0].get(), pool, symbols);
                } else {
                    res += "M";
                    for (const auto& ret : ft->sig.returnTypes) {
                        res += mangleType(ret.get(), pool, symbols);
                    }
                }
                return res;
            }
            
            default:
                return "Unknown";
        }
    }
    
    // Overload without symbol table (for contexts where aliases are already resolved)
    inline std::string mangleType(TypeAST* type, StringPool& pool) {
        return mangleType(type, pool, nullptr);
    }
    
    // Method mangling: Type::method
    inline std::string mangleMethod(std::string_view parent, std::string_view method) {
        std::string result;
        result.reserve(parent.size() + method.size() + 2);
        result.append(parent);
        result.append("::");
        result.append(method);
        return result;
    }
    
    // Enum variant mangling: EnumName::variantName
    inline std::string mangleEnumVariant(std::string_view enumName, std::string_view variant) {
        std::string result;
        result.reserve(enumName.size() + variant.size() + 2);
        result.append(enumName);
        result.append("::");
        result.append(variant);
        return result;
    }
    
    // From entry mangling: TargetType::from::SourceType
    inline std::string mangleFrom(std::string_view target, TypeAST* paramType, StringPool& pool) {
        std::string result;
        result.reserve(target.size() + 16);
        result.append(target);
        result.append("::from::");
        if (paramType) {
            result.append(mangleType(paramType, pool));
        } else {
            result.append("void");
        }
        return result;
    }
    
    // Prefix for from lookup
    inline std::string getFromPrefix(std::string_view target) {
        return std::string(target) + "::from::";
    }

} // namespace NameMangler