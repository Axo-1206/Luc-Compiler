import re
import glob

def refactor_file(filepath):
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    original = content

    if '#include "BaseAST.hpp"' in content and '#include "../registry/InternedString.hpp"' not in content:
        content = re.sub(r'(#include "BaseAST\.hpp")', r'\1\n#include "../registry/InternedString.hpp"', content)

    # 1. Replace struct fields
    # std::string something;
    fields_to_intern = ['name', 'value', 'typeName', 'method', 'field', 'ident', 'intrinsicName', 'bindName', 'packageName', 'alias']
    for field in fields_to_intern:
        content = re.sub(r'std::string(\s+)' + field + r'(\s*[;=])', r'InternedString\1' + field + r'\2', content)
        
        # also handle std::optional<std::string> -> std::optional<InternedString>
        content = re.sub(r'std::optional<std::string>(\s+)' + field + r'(\s*[;=])', r'std::optional<InternedString>\1' + field + r'\2', content)

    # 2. std::vector<std::string> -> std::vector<InternedString>
    content = content.replace('std::vector<std::string>', 'std::vector<InternedString>')

    # 3. Replace constructor arguments taking std::string
    # e.g., explicit Node(std::string n)
    content = re.sub(r'\(std::string (\w+)\)', r'(InternedString \1)', content)
    content = re.sub(r'\(LiteralKind k, std::string v\)', r'(LiteralKind k, InternedString v)', content)
    content = re.sub(r'\(std::string (\w+), ExprPtr (\w+)\)', r'(InternedString \1, ExprPtr \2)', content)
    content = re.sub(r'\(AttributeArgKind k, std::string v\)', r'(AttributeArgKind k, InternedString v)', content)

    # 4. Remove std::move for these fields in constructor initialization lists
    # e.g. name(std::move(n)) -> name(n)
    # We'll just replace std::move for common single letter args: n, v
    content = re.sub(r'name\(std::move\((n|v)\)\)', r'name(\1)', content)
    content = re.sub(r'value\(std::move\((n|v)\)\)', r'value(\1)', content)
    content = re.sub(r'typeName\(std::move\((n|v)\)\)', r'typeName(\1)', content)
    content = re.sub(r'ident\(std::move\((n|v)\)\)', r'ident(\1)', content)
    
    # 5. Fix pass by const reference: const std::string&
    content = re.sub(r'const std::string&\s+(\w+)', r'InternedString \1', content)

    if content != original:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(content)
        print(f"Updated {filepath}")
    else:
        print(f"No changes for {filepath}")

for f in glob.glob('c:/Users/TaiAx/Desktop/luc/src/ast/*AST.hpp'):
    refactor_file(f)
