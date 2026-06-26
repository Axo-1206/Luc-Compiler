/**
 * @file ModuleResolver.cpp
 * @brief Implementation of module resolution and caching.
 */

#include "parser/ModuleResolver.hpp"
#include "core/ast/BaseAST.hpp"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <regex>

namespace parser {

// ─────────────────────────────────────────────────────────────────────────────
// Construction
// ─────────────────────────────────────────────────────────────────────────────

ModuleResolver::ModuleResolver(const std::filesystem::path& packageRoot, StringPool& pool)
    : packageRoot_(packageRoot)
    , pool_(pool) {
    // Ensure package root exists
    if (!std::filesystem::exists(packageRoot_)) {
        // Create if it doesn't exist (for tests)
        std::filesystem::create_directories(packageRoot_);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Path Resolution
// ─────────────────────────────────────────────────────────────────────────────

InternedString ModuleResolver::resolveUsePath(InternedString usePath) {
    // Check custom mappings first
    auto it = customMappings_.find(usePath);
    if (it != customMappings_.end()) {
        return it->second;
    }
    
    // Check cache
    auto cacheIt = usePathToFile_.find(usePath);
    if (cacheIt != usePathToFile_.end()) {
        return cacheIt->second;
    }
    
    // Convert use path to file path
    std::string_view useStr = pool_.lookup(usePath);
    std::string filePath;
    
    // Replace '.' with '/' for path separators
    for (char c : useStr) {
        if (c == '.') {
            filePath += '/';
        } else {
            filePath += c;
        }
    }
    
    // Try with .lucid extension
    std::string pathWithExt = filePath + ".lucid";
    
    // Check if file exists in search paths
    std::filesystem::path foundPath = findFileInSearchPaths(pathWithExt);
    if (!foundPath.empty()) {
        // Convert to relative path from package root
        std::string relative = std::filesystem::relative(foundPath, packageRoot_).string();
        // Normalize to forward slashes
        std::replace(relative.begin(), relative.end(), '\\', '/');
        InternedString result = pool_.intern(relative);
        usePathToFile_[usePath] = result;
        return result;
    }
    
    // Try with just the name (no .lucid) - maybe it's a file without extension
    std::filesystem::path foundPathNoExt = findFileInSearchPaths(filePath);
    if (!foundPathNoExt.empty()) {
        std::string relative = std::filesystem::relative(foundPathNoExt, packageRoot_).string();
        std::replace(relative.begin(), relative.end(), '\\', '/');
        InternedString result = pool_.intern(relative);
        usePathToFile_[usePath] = result;
        return result;
    }
    
    // Not found - return empty
    return InternedString();
}

std::filesystem::path ModuleResolver::getModuleFilePath(InternedString modulePath) const {
    std::string_view pathStr = pool_.lookup(modulePath);
    std::filesystem::path result = packageRoot_;
    
    // Split path by '/' and append each component
    std::string path = std::string(pathStr);
    size_t start = 0;
    size_t end = path.find('/');
    while (end != std::string::npos) {
        result /= path.substr(start, end - start);
        start = end + 1;
        end = path.find('/', start);
    }
    if (start < path.size()) {
        result /= path.substr(start);
    }
    
    return result;
}

void ModuleResolver::addSearchPath(const std::filesystem::path& path) {
    if (std::filesystem::exists(path)) {
        searchPaths_.push_back(path);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Module Caching
// ─────────────────────────────────────────────────────────────────────────────

bool ModuleResolver::isModuleParsed(InternedString modulePath) const {
    return parsedModules_.find(modulePath) != parsedModules_.end();
}

ProgramAST* ModuleResolver::getParsedModule(InternedString modulePath) const {
    auto it = parsedModules_.find(modulePath);
    if (it != parsedModules_.end()) {
        return it->second;
    }
    return nullptr;
}

void ModuleResolver::cacheModule(InternedString modulePath, ProgramAST* ast) {
    parsedModules_[modulePath] = ast;
}

// ─────────────────────────────────────────────────────────────────────────────
// Circular Import Detection
// ─────────────────────────────────────────────────────────────────────────────

bool ModuleResolver::isParsing(InternedString modulePath) const {
    for (InternedString path : parsingStack_) {
        if (path == modulePath) {
            return true;
        }
    }
    return false;
}

void ModuleResolver::pushParsing(InternedString modulePath) {
    parsingStack_.push_back(modulePath);
}

void ModuleResolver::popParsing() {
    if (!parsingStack_.empty()) {
        parsingStack_.pop_back();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// File Operations
// ─────────────────────────────────────────────────────────────────────────────

std::string ModuleResolver::readModuleSource(InternedString filePath) const {
    std::filesystem::path fullPath = getModuleFilePath(filePath);
    
    // Check if file exists
    if (!std::filesystem::exists(fullPath)) {
        return "";
    }
    
    // Read file
    std::ifstream file(fullPath);
    if (!file.is_open()) {
        return "";
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// ─────────────────────────────────────────────────────────────────────────────
// Module Registration
// ─────────────────────────────────────────────────────────────────────────────

void ModuleResolver::registerModuleMapping(InternedString usePath, InternedString filePath) {
    customMappings_[usePath] = filePath;
}

std::vector<InternedString> ModuleResolver::getParsedModulePaths() const {
    std::vector<InternedString> paths;
    paths.reserve(parsedModules_.size());
    for (const auto& [path, ast] : parsedModules_) {
        paths.push_back(path);
    }
    return paths;
}

// ─────────────────────────────────────────────────────────────────────────────
// Private Helpers
// ─────────────────────────────────────────────────────────────────────────────

InternedString ModuleResolver::normalizePath(std::string_view path) const {
    // Convert Windows backslashes to forward slashes
    std::string normalized;
    normalized.reserve(path.size());
    for (char c : path) {
        if (c == '\\') {
            normalized += '/';
        } else {
            normalized += c;
        }
    }
    return pool_.intern(normalized);
}

std::filesystem::path ModuleResolver::findFileInSearchPaths(const std::string& relativePath) const {
    // Check package root first
    std::filesystem::path rootPath = packageRoot_ / relativePath;
    if (std::filesystem::exists(rootPath)) {
        return rootPath;
    }
    
    // Check additional search paths
    for (const auto& searchPath : searchPaths_) {
        std::filesystem::path fullPath = searchPath / relativePath;
        if (std::filesystem::exists(fullPath)) {
            return fullPath;
        }
    }
    
    // Not found
    return {};
}

} // namespace parser