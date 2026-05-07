#include "AttributeRegistry.hpp"
#include "diagnostics/DiagnosticEngine.hpp"
#include "diagnostics/DiagnosticCodes.hpp"
#include "debug/DebugMacros.hpp"

// ─────────────────────────────────────────────────────────────────────────────
// Helper to get string representation of AttributeContext
// ─────────────────────────────────────────────────────────────────────────────
static std::string contextToString(AttributeContext ctx) {
    std::string result;
    if (hasFlag(ctx, AttributeContext::Func)) result += "Func ";
    if (hasFlag(ctx, AttributeContext::Var)) result += "Var ";
    if (hasFlag(ctx, AttributeContext::Struct)) result += "Struct ";
    if (hasFlag(ctx, AttributeContext::Impl)) result += "Impl ";
    if (hasFlag(ctx, AttributeContext::Main)) result += "Main";
    return result.empty() ? "None" : result;
}

// ─────────────────────────────────────────────────────────────────────────────
// Custom validators for specific attributes
// ─────────────────────────────────────────────────────────────────────────────

static bool validateExternArgs(const std::vector<AttributeArgAST>& args,
                                const std::string& declName,
                                DiagnosticEngine& dc,
                                const SourceLocation& loc) {
    LUC_LOG_SEMANTIC_VERBOSE("validateExternArgs: declName='" << declName 
                             << "', argCount=" << args.size());
    
    if (args.empty()) {
        LUC_LOG_SEMANTIC("\tERROR: @extern missing symbol name");
        dc.error(DiagnosticCategory::Semantic, loc, DiagCode::E2011,
                 "'@extern' requires at least one argument: the C symbol name");
        return false;
    }
    
    if (args[0].argKind != AttributeArgAST::ArgKind::StringLit) {
        LUC_LOG_SEMANTIC("\tERROR: @extern first arg not string");
        dc.error(DiagnosticCategory::Semantic, args[0].loc, DiagCode::E2011,
                 "'@extern' first argument must be a string literal (the C symbol name)");
        return false;
    }
    
    LUC_LOG_SEMANTIC_EXTREME("\t@extern symbol: '" << args[0].value << "'");
    
    if (args.size() >= 2) {
        if (args[1].argKind != AttributeArgAST::ArgKind::StringLit) {
            LUC_LOG_SEMANTIC("\tERROR: @extern second arg not string");
            dc.error(DiagnosticCategory::Semantic, args[1].loc, DiagCode::E2011,
                     "'@extern' second argument must be a string literal (calling convention)");
            return false;
        }
        LUC_LOG_SEMANTIC_EXTREME("\t@extern calling convention: '" << args[1].value << "'");
    }
    
    if (args.size() > 2) {
        LUC_LOG_SEMANTIC("\tERROR: @extern too many args");
        dc.error(DiagnosticCategory::Semantic, loc, DiagCode::E2011,
                 "'@extern' takes at most 2 arguments: (symbol_name, calling_convention)");
        return false;
    }
    
    return true;
}

static bool validateDeprecatedArgs(const std::vector<AttributeArgAST>& args,  const std::string& declName,
                                    DiagnosticEngine& dc, const SourceLocation& loc) {
    LUC_LOG_SEMANTIC_VERBOSE("validateDeprecatedArgs: declName='" << declName 
                             << "', argCount=" << args.size());
    
    if (args.size() > 1) {
        LUC_LOG_SEMANTIC("\tERROR: @deprecated too many args");
        dc.error(DiagnosticCategory::Semantic, loc, DiagCode::E2011,
                 "'@deprecated' takes at most one string argument");
        return false;
    }
    
    if (!args.empty()) {
        if (args[0].argKind != AttributeArgAST::ArgKind::StringLit) {
            LUC_LOG_SEMANTIC("\tERROR: @deprecated arg not string");
            dc.error(DiagnosticCategory::Semantic, args[0].loc, DiagCode::E2011,
                     "'@deprecated' argument must be a string literal message");
            return false;
        }
        LUC_LOG_SEMANTIC_EXTREME("\t@deprecated message: '" << args[0].value << "'");
    }
    
    return true;
}

static bool validateMainOnlyArgs(const std::vector<AttributeArgAST>& args, const std::string& declName,
                                  DiagnosticEngine& dc, const SourceLocation& loc) {
    LUC_LOG_SEMANTIC_VERBOSE("validateMainOnlyArgs: declName='" << declName 
                             << "', argCount=" << args.size());
    
    if (!args.empty()) {
        LUC_LOG_SEMANTIC("\tERROR: @aot/@jit takes no args");
        dc.error(DiagnosticCategory::Semantic, loc, DiagCode::E2011,
                 "'@aot' and '@jit' take no arguments");
        return false;
    }
    
    if (declName != "main") {
        LUC_LOG_SEMANTIC("\tERROR: @aot/@jit on non-main function '" << declName << "'");
        dc.error(DiagnosticCategory::Semantic, loc, DiagCode::E3016,
                 "'@aot' and '@jit' are only valid on the 'main' entry point function");
        return false;
    }
    
    LUC_LOG_SEMANTIC_EXTREME("\t@aot/@jit validated on main function");
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Attribute table - single source of truth
// ─────────────────────────────────────────────────────────────────────────────
struct AttributeTableEntry {
    const char* name;
    uint32_t contexts;
    bool takesArgs;
    int minArgs;
    int maxArgs;
    uint32_t allowedArgKinds;
    bool requiresConst;
    const char* exclusiveWith;
    bool (*validator)(const std::vector<AttributeArgAST>&,
                      const std::string&, DiagnosticEngine&, const SourceLocation&);
};

static const AttributeTableEntry kAttributeTable[] = {
    {"extern", 
     static_cast<uint32_t>(AttributeContext::Func | AttributeContext::Var),
     true, 1, 2, 
     static_cast<uint32_t>(AttrArgKind::String),
     true, "", validateExternArgs},
    
    {"inline", 
     static_cast<uint32_t>(AttributeContext::Func),
     false, 0, 0, 0,
     false, "", nullptr},
    
    {"noinline", 
     static_cast<uint32_t>(AttributeContext::Func),
     false, 0, 0, 0,
     false, "", nullptr},
    
    {"packed", 
     static_cast<uint32_t>(AttributeContext::Struct),
     false, 0, 0, 0,
     false, "", nullptr},
    
    {"deprecated", 
     static_cast<uint32_t>(AttributeContext::Func | AttributeContext::Var | AttributeContext::Struct),
     true, 0, 1,
     static_cast<uint32_t>(AttrArgKind::String),
     false, "", validateDeprecatedArgs},
    
    {"aot", 
     static_cast<uint32_t>(AttributeContext::Func | AttributeContext::Main),
     false, 0, 0, 0,
     false, "jit", validateMainOnlyArgs},
    
    {"jit", 
     static_cast<uint32_t>(AttributeContext::Func | AttributeContext::Main),
     false, 0, 0, 0,
     false, "aot", validateMainOnlyArgs},
    
    {"cold", 
     static_cast<uint32_t>(AttributeContext::Func),
     false, 0, 0, 0,
     false, "", nullptr},
    
    {"heap", 
     static_cast<uint32_t>(AttributeContext::Func),
     false, 0, 0, 0,
     false, "", nullptr},
};

// ─────────────────────────────────────────────────────────────────────────────
// AttributeRegistry implementation
// ─────────────────────────────────────────────────────────────────────────────

AttributeRegistry& AttributeRegistry::instance() {
    static AttributeRegistry registry;
    return registry;
}

AttributeRegistry::AttributeRegistry() {
    LUC_LOG_SEMANTIC_VERBOSE("AttributeRegistry: initializing with " 
                             << (sizeof(kAttributeTable)/sizeof(kAttributeTable[0])) 
                             << " attributes");
    
    for (const auto& e : kAttributeTable) {
        AttributeInfo info;
        info.name = e.name;
        info.validContexts = static_cast<AttributeContext>(e.contexts);
        info.takesArgs = e.takesArgs;
        info.minArgs = e.minArgs;
        info.maxArgs = e.maxArgs;
        info.allowedArgKinds = static_cast<AttrArgKind>(e.allowedArgKinds);
        info.requiresConst = e.requiresConst;
        info.exclusiveWith = e.exclusiveWith;
        info.validator = e.validator;
        registerAttribute(info);
        
        LUC_LOG_SEMANTIC_EXTREME("\tRegistered @" << e.name 
                                 << " (contexts=" << contextToString(info.validContexts) << ")");
    }
}

void AttributeRegistry::registerAttribute(const AttributeInfo& info) {
    attributes_[info.name] = info;
}

const AttributeInfo* AttributeRegistry::lookup(const std::string& name) const {
    auto it = attributes_.find(name);
    if (it != attributes_.end()) {
        LUC_LOG_SEMANTIC_EXTREME("AttributeRegistry::lookup: found @" << name);
        return &it->second;
    }
    LUC_LOG_SEMANTIC_EXTREME("AttributeRegistry::lookup: @" << name << " not found");
    return nullptr;
}

bool AttributeRegistry::isValidOn(const std::string& name, AttributeContext ctx) const {
    auto* info = lookup(name);
    if (!info) return false;
    
    bool valid = false;
    if (hasFlag(info->validContexts, AttributeContext::Main)) {
        valid = hasFlag(ctx, AttributeContext::Main);
    } else {
        valid = hasFlag(info->validContexts, ctx);
    }
    
    LUC_LOG_SEMANTIC_VERBOSE("isValidOn: @" << name << " on context [" 
                             << contextToString(ctx) << "] -> " << (valid ? "true" : "false"));
    return valid;
}

bool AttributeRegistry::isMainOnlyAttribute(const std::string& name) const {
    auto* info = lookup(name);
    bool result = info && hasFlag(info->validContexts, AttributeContext::Main);
    LUC_LOG_SEMANTIC_EXTREME("isMainOnlyAttribute: @" << name << " -> " << (result ? "true" : "false"));
    return result;
}

std::string AttributeRegistry::allNames() const {
    std::string result;
    for (const auto& [name, _] : attributes_) {
        if (!result.empty()) result += ", ";
        result += "@" + name;
    }
    return result;
}

bool AttributeRegistry::validateAttribute(const AttributeAST& attr, AttributeContext ctx,
                                           const std::string& declName, DeclKeyword declKw,
                                           DiagnosticEngine& dc) const {
    LUC_LOG_SEMANTIC_VERBOSE("validateAttribute: @" << attr.name 
                             << " on declaration '" << declName << "'");
    
    const AttributeInfo* info = lookup(attr.name);
    if (!info) {
        LUC_LOG_SEMANTIC("\tERROR: unknown attribute");
        dc.error(DiagnosticCategory::Semantic, attr.loc, DiagCode::E2010,
                 "unknown attribute '@" + attr.name + "'; known attributes: " + allNames());
        return false;
    }
    
    // Check context validity
    if (!isValidOn(attr.name, ctx)) {
        LUC_LOG_SEMANTIC("\tERROR: invalid context for @" << attr.name);
        if (isMainOnlyAttribute(attr.name)) {
            dc.error(DiagnosticCategory::Semantic, attr.loc, DiagCode::E3016,
                     "'@" + attr.name + "' is only valid on the 'main' entry point function");
        } else {
            dc.error(DiagnosticCategory::Semantic, attr.loc, DiagCode::E2010,
                     "'@" + attr.name + "' is not valid on this declaration");
        }
        return false;
    }
    
    // Check const requirement for @extern
    if (info->requiresConst && declKw != DeclKeyword::Const) {
        LUC_LOG_SEMANTIC("\tWARNING: @extern with 'let' (should be 'const')");
        dc.warning(DiagnosticCategory::Semantic, attr.loc, DiagCode::W3001,
                   "'@extern' should use 'const', not 'let' — "
                   "extern bindings are permanently resolved by the linker");
        // Continue validation - not a fatal error
    }
    
    // Check arguments presence
    if (!info->takesArgs && !attr.args.empty()) {
        LUC_LOG_SEMANTIC("\tERROR: @" << attr.name << " takes no arguments");
        dc.error(DiagnosticCategory::Semantic, attr.loc, DiagCode::E2011,
                 "'@" + attr.name + "' takes no arguments");
        return false;
    }
    
    // Check argument count
    if (info->takesArgs) {
        int argCount = static_cast<int>(attr.args.size());
        if (argCount < info->minArgs) {
            LUC_LOG_SEMANTIC("\tERROR: @" << attr.name << " requires at least " 
                             << info->minArgs << " args, got " << argCount);
            dc.error(DiagnosticCategory::Semantic, attr.loc, DiagCode::E2011,
                     "'@" + info->name + "' requires at least " + 
                     std::to_string(info->minArgs) + " argument(s), got " + 
                     std::to_string(argCount));
            return false;
        }
        if (info->maxArgs != -1 && argCount > info->maxArgs) {
            LUC_LOG_SEMANTIC("\tERROR: @" << attr.name << " takes at most " 
                             << info->maxArgs << " args, got " << argCount);
            dc.error(DiagnosticCategory::Semantic, attr.loc, DiagCode::E2011,
                     "'@" + info->name + "' takes at most " + 
                     std::to_string(info->maxArgs) + " argument(s), got " + 
                     std::to_string(argCount));
            return false;
        }
        
        // Check argument kinds
        for (const auto& arg : attr.args) {
            AttrArgKind argKind;
            switch (arg.argKind) {
                case AttributeArgAST::ArgKind::StringLit: 
                    argKind = AttrArgKind::String; 
                    break;
                case AttributeArgAST::ArgKind::IntLit: 
                    argKind = AttrArgKind::Int; 
                    break;
                case AttributeArgAST::ArgKind::BoolLit: 
                    argKind = AttrArgKind::Bool; 
                    break;
                case AttributeArgAST::ArgKind::TypeIdent: 
                    argKind = AttrArgKind::Type; 
                    break;
                default: 
                    argKind = AttrArgKind::None; 
                    break;
            }
            
            if (!hasFlag(info->allowedArgKinds, argKind)) {
                LUC_LOG_SEMANTIC("\tERROR: invalid argument type for @" << attr.name);
                dc.error(DiagnosticCategory::Semantic, arg.loc, DiagCode::E2011,
                         "invalid argument type for '@" + info->name + "'");
                return false;
            }
        }
    }
    
    // Run custom validator
    if (info->validator) {
        LUC_LOG_SEMANTIC_EXTREME("\trunning custom validator for @" << attr.name);
        if (!info->validator(attr.args, declName, dc, attr.loc)) {
            return false;
        }
    }
    
    LUC_LOG_SEMANTIC_VERBOSE("\tvalidateAttribute: @" << attr.name << " SUCCESS");
    return true;
}

bool AttributeRegistry::checkMutualExclusion(const std::string& name1, const std::string& name2,
                                               DiagnosticEngine& dc, const SourceLocation& loc) const {
    const AttributeInfo* info1 = lookup(name1);
    const AttributeInfo* info2 = lookup(name2);
    
    if (!info1 || !info2) return true;
    
    if (info1->exclusiveWith == name2 || info2->exclusiveWith == name1) {
        LUC_LOG_SEMANTIC("\tERROR: mutually exclusive attributes @" << name1 
                         << " and @" << name2);
        dc.error(DiagnosticCategory::Semantic, loc, DiagCode::E3015,
                 "'@" + name1 + "' and '@" + name2 + "' are mutually exclusive");
        return false;
    }
    
    return true;
}