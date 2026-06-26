/**
 * @file ParseSession.cpp
 * @brief Implementation of the parsing session.
 */

#include "parser/ParseSession.hpp"
#include "parser/Parser.hpp"
#include "core/diagnostics/Diagnostic.hpp"
#include "core/diagnostics/DiagnosticCodes.hpp"

#include <fstream>
#include <sstream>
#include <filesystem>

namespace parser {

// ─────────────────────────────────────────────────────────────────────────────
// Construction
// ─────────────────────────────────────────────────────────────────────────────

ParseSession::ParseSession(const std::filesystem::path& packageRoot)
    : moduleResolver_(packageRoot, pool_) {
    // Add standard library paths if they exist
    moduleResolver_.addSearchPath(packageRoot / "lib");
    moduleResolver_.addSearchPath(packageRoot / "std");
}

// ─────────────────────────────────────────────────────────────────────────────
// File Parsing
// ─────────────────────────────────────────────────────────────────────────────

ProgramAST* ParseSession::parseFile(const std::string& filePath, 
                                    const std::string& source) {
    InternedString path = pool_.intern(filePath);
    
    // Check if already parsed
    auto it = parsedFiles_.find(path);
    if (it != parsedFiles_.end()) {
        return it->second;
    }
    
    // Check for circular import
    for (InternedString p : parsingStack_) {
        if (p == path) {
            reportError("Circular import detected: " + filePath, filePath);
            return nullptr;
        }
    }
    
    // Parse the file
    parsingStack_.push_back(path);
    ProgramAST* ast = parseFileInternal(path, source);
    parsingStack_.pop_back();
    
    if (ast) {
        parsedFiles_[path] = ast;
        parsedFileOrder_.push_back(path);
    }
    
    return ast;
}

void ParseSession::parseAll() {
    // Parse all files in the package root
    std::filesystem::path root = moduleResolver_.getPackageRoot();
    
    // Recursively find all .lucid files
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        if (entry.is_regular_file() && entry.path().extension() == ".lucid") {
            files.push_back(entry.path());
        }
    }
    
    // Parse each file
    for (const auto& file : files) {
        std::string relative = std::filesystem::relative(file, root).string();
        parseFile(relative);
    }
}

ProgramAST* ParseSession::getParsedFile(const std::string& filePath) const {
    InternedString path = pool_.intern(filePath);
    auto it = parsedFiles_.find(path);
    if (it != parsedFiles_.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<ProgramAST*> ParseSession::getAllParsedFiles() const {
    std::vector<ProgramAST*> result;
    result.reserve(parsedFiles_.size());
    for (const auto& [path, ast] : parsedFiles_) {
        result.push_back(ast);
    }
    return result;
}

std::vector<std::string> ParseSession::getAllParsedFilePaths() const {
    std::vector<std::string> result;
    result.reserve(parsedFileOrder_.size());
    for (InternedString path : parsedFileOrder_) {
        result.push_back(std::string(pool_.lookup(path)));
    }
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// Module Resolution
// ─────────────────────────────────────────────────────────────────────────────

ProgramAST* ParseSession::importModule(const std::string& usePath) {
    InternedString usePathStr = pool_.intern(usePath);
    
    // Resolve use path to file path
    InternedString filePath = moduleResolver_.resolveUsePath(usePathStr);
    if (!filePath.isValid()) {
        reportError("Module not found: " + usePath);
        return nullptr;
    }
    
    // Check if already parsed
    ProgramAST* existing = moduleResolver_.getParsedModule(filePath);
    if (existing) {
        return existing;
    }
    
    // Parse the module
    std::string pathStr = std::string(pool_.lookup(filePath));
    ProgramAST* ast = parseFile(pathStr);
    
    if (ast) {
        moduleResolver_.cacheModule(filePath, ast);
        return ast;
    }
    
    return nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
// Internal Methods
// ─────────────────────────────────────────────────────────────────────────────

ProgramAST* ParseSession::parseFileInternal(InternedString filePath, 
                                            const std::string& source) {
    std::string sourceCode = source;
    
    // Read from disk if source not provided
    if (sourceCode.empty()) {
        std::filesystem::path fullPath = moduleResolver_.getModuleFilePath(filePath);
        sourceCode = readFile(fullPath);
        if (sourceCode.empty()) {
            reportError("Failed to read file: " + std::string(pool_.lookup(filePath)));
            return nullptr;
        }
    }
    
    // Parse the file
    ProgramAST* ast = parser::parseFile(
        std::string(pool_.lookup(filePath)),
        sourceCode,
        pool_,
        arena_
    );
    
    return ast;
}

std::string ParseSession::readFile(const std::filesystem::path& path) const {
    std::ifstream file(path);
    if (!file.is_open()) {
        return "";
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void ParseSession::reportError(const std::string& message, const std::string& file) {
    Diagnostic diag;
    diag.severity = DiagnosticSeverity::Error;
    diag.category = DiagnosticCategory::General;
    diag.code = DiagCode::E1001;
    diag.file = pool_.intern(file);
    errors_.push_back(diag);
}

} // namespace parser