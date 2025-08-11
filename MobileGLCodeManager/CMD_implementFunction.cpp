#include <iostream>
#include <string>
#include <cstring>
#include <vector>
#include <sstream>
#include <fstream>
#include <unordered_map>
#include <functional>
#include <filesystem>
#include <regex>

const char* GL_IMPL_DIRECTORY_PATH = "MobileGL/MG_Impl/GLImpl";
const char* DEFINITIONS_FILE_PATH = "MobileGL/MG_Impl/GLImpl/Exporting/Definitions.cpp";
const char* CMAKELISTS_FILE_PATH = "CMakeLists.txt";

const char* INIT_HEADER_FILE_CONTENT = R"init(#pragma once
#include <Includes.h>

namespace MobileGL {
    namespace MG_Impl::GLImpl {
        /* @INSERTION_POINT:FUNCTION_DECLARATION@ */
    } // namespace MG_Impl::GLImpl
} // namespace MobileGL)init";

const char* INIT_SOURCE_CODE_FILE_CONTENT = R"init(

namespace MobileGL {
    namespace MG_Impl::GLImpl {
        /* @INSERTION_POINT:FUNCTION_IMPLEMENTATION@ */
    } // namespace MG_Impl::GLImpl
} // namespace MobileGL)init";

const char* INSERTION_POINT_FUNCTION_DECLARATION = "/* @INSERTION_POINT:FUNCTION_DECLARATION@ */";
const char* INSERTION_POINT_FUNCTION_IMPLEMENTATION = "/* @INSERTION_POINT:FUNCTION_IMPLEMENTATION@ */";
const char* INSERTION_POINT_SOURCE_FILE_GLIMPL = "# @INSERTION_POINT:SOURCE_FILE_GLIMPL@ #";


namespace fs = std::filesystem;

bool EnsureFileAndDirsExist(const std::string& pathStr) {
    fs::path filePath(pathStr);
    fs::path dirPath = filePath.parent_path();

    if (!dirPath.empty() && !fs::exists(dirPath)) {
        if (!fs::create_directories(dirPath)) {
            std::cerr << "Failed to create directories: " << dirPath << std::endl;
            return false;
        }
    }

    if (!fs::exists(filePath)) {
        std::ofstream ofs(filePath);
        if (!ofs) {
            std::cerr << "Failed to create file: " << filePath << std::endl;
            return false;
        }
    }

    return true;
}

bool IsFileExists(const std::string& pathStr) {
    fs::path filePath(pathStr);
    return fs::exists(filePath) && fs::is_regular_file(filePath);
}

std::string GetFileContent(const std::string& filename) {
    std::ifstream inFile(filename);
    if (!inFile.is_open()) {
        throw std::runtime_error("Cannot open file: " + filename);
    }
    std::string content;
    std::string line;
    while (std::getline(inFile, line)) {
        content += line + '\n';
    }
    inFile.close();
	return content;
}

void WriteToFile(const std::string& filename, const std::string& content) {
    std::ofstream outFile(filename);
    if (!outFile.is_open()) {
        throw std::runtime_error("Cannot write to file: " + filename);
    }
    outFile << content;
    outFile.close();
}

void SetFunctionStub(const std::string& filename, const std::string& func_name, bool is_stub) {
    std::ifstream inFile(filename);
    if (!inFile.is_open()) {
        throw std::runtime_error("Cannot open file: " + filename);
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(inFile, line)) {
        lines.push_back(line);
    }
    inFile.close();

    std::string pattern_str =
        "(DECLARE_GL_FUNCTION_(?:STUB_)?HEAD\\([^)]*\\b" +
        func_name +
        "\\b[^)]*\\)\\s*DECLARE_GL_FUNCTION_(?:STUB_)?(?:END|END_NO_RETURN)[^)]*\\([^)]*\\b" +
        func_name +
        "\\b[^)]*\\))";

    std::regex func_pattern(pattern_str);

    for (auto& line : lines) {
        if (std::regex_search(line, func_pattern)) {
            if (is_stub) {
                size_t pos;
                if ((pos = line.find("DECLARE_GL_FUNCTION_HEAD")) != std::string::npos) {
                    line.replace(pos, strlen("DECLARE_GL_FUNCTION_HEAD"), "DECLARE_GL_FUNCTION_STUB_HEAD");
                }
                if ((pos = line.find("DECLARE_GL_FUNCTION_END")) != std::string::npos) {
                    line.replace(pos, strlen("DECLARE_GL_FUNCTION_END"), "DECLARE_GL_FUNCTION_STUB_END");
                }
            }
            else {
                size_t pos;
                if ((pos = line.find("DECLARE_GL_FUNCTION_STUB_HEAD")) != std::string::npos) {
                    line.replace(pos, strlen("DECLARE_GL_FUNCTION_STUB_HEAD"), "DECLARE_GL_FUNCTION_HEAD");
                }
                if ((pos = line.find("DECLARE_GL_FUNCTION_STUB_END")) != std::string::npos) {
                    line.replace(pos, strlen("DECLARE_GL_FUNCTION_STUB_END"), "DECLARE_GL_FUNCTION_END");
                }
            }
        }
    }

    std::ofstream outFile(filename);
    if (!outFile.is_open()) {
        throw std::runtime_error("Cannot write to file: " + filename);
    }
    for (const auto& l : lines) {
        outFile << l << '\n';
    }
    outFile.close();
}

void MakeSureSourceInCMakeListsFile(const std::string& component) {
    if (!IsFileExists(CMAKELISTS_FILE_PATH)) {
        std::cerr << "CMakeLists file does not exist: " << CMAKELISTS_FILE_PATH << std::endl;
        return;
    }

    std::string cmakeContent = GetFileContent(CMAKELISTS_FILE_PATH);

    size_t insertPos = cmakeContent.find(INSERTION_POINT_SOURCE_FILE_GLIMPL);
    if (insertPos == std::string::npos) {
        throw std::runtime_error("Insertion point not found in CMakeLists file");
    }

    std::string sourcePath = "MobileGL/MG_Impl/GLImpl/" + component + "/GL_" + component + ".cpp";

    if (cmakeContent.find(sourcePath) != std::string::npos) {
        return;
    }

    size_t lineStart = cmakeContent.rfind('\n', insertPos) + 1;
    std::string indent = cmakeContent.substr(lineStart, insertPos - lineStart);

    cmakeContent.insert(insertPos + strlen(INSERTION_POINT_SOURCE_FILE_GLIMPL),
        "\n" + indent + sourcePath);

    WriteToFile(CMAKELISTS_FILE_PATH, cmakeContent);

    std::cout << "Added source to CMakeLists '" << sourcePath << "'" << std::endl;
}

void replaceFirstLine(std::string& content, const std::string& newFirstLine) {
    size_t pos = content.find('\n');
    if (pos == std::string::npos) {
        content = newFirstLine;
    } else {
        content = newFirstLine + content.substr(pos);
    }
}

void WriteSourceAndHeaderFiles(const std::string& functionName, const std::string& component) {
    std::string filePathPrefix = std::string(GL_IMPL_DIRECTORY_PATH) + "/" + component + "/GL_" + component;

    EnsureFileAndDirsExist(filePathPrefix + ".h");
    std::string headerfileContent = GetFileContent(filePathPrefix + ".h");
    if (headerfileContent.empty()) {
        WriteToFile(filePathPrefix + ".h", INIT_HEADER_FILE_CONTENT);
        headerfileContent = INIT_HEADER_FILE_CONTENT;
    }

    EnsureFileAndDirsExist(filePathPrefix + ".cpp");
    std::string sourcefileContent = GetFileContent(filePathPrefix + ".cpp");
    if (sourcefileContent.empty()) {
        std::string realContent = INIT_SOURCE_CODE_FILE_CONTENT;
        replaceFirstLine(realContent, "#include \"GL_" + component + ".h\"");
        WriteToFile(filePathPrefix + ".cpp", realContent);
        sourcefileContent = realContent;
    }

    MakeSureSourceInCMakeListsFile(component);

    std::string definitionsContent = GetFileContent(DEFINITIONS_FILE_PATH);
    std::istringstream defStream(definitionsContent);
    std::string line;
    std::string functionSignature;

    std::regex funcLinePattern("DECLARE_GL_FUNCTION_HEAD\\(([^)]*\\b" + functionName + "\\b[^)]*)\\)");

    while (std::getline(defStream, line)) {
        std::smatch match;
        if (std::regex_search(line, match, funcLinePattern)) {
            functionSignature = match[1].str();
            break;
        }
    }

    if (functionSignature.empty()) {
        throw std::runtime_error("Function not found in definitions: " + functionName);
    }

    std::istringstream sigStream(functionSignature);
    std::vector<std::string> parts;
    std::string part;

    while (std::getline(sigStream, part, ',')) {
        parts.push_back(part);
    }

    if (parts.size() < 2) {
        throw std::runtime_error("Invalid function signature: " + functionSignature);
    }

    std::string returnType = parts[0];
    std::string funcName = parts[1];
    funcName.erase(0, 1);

    std::string params;
    for (size_t i = 2; i < parts.size(); ++i) {
        if (!params.empty()) params += ", ";
        parts[i].erase(0, 1);
        params += parts[i];
    }

    std::string funcDeclaration = returnType + " " + funcName + "(" + params + ");";

    size_t headerPos = headerfileContent.find(INSERTION_POINT_FUNCTION_DECLARATION);
    if (headerPos == std::string::npos) {
        throw std::runtime_error("Insertion point not found in header '" + filePathPrefix + ".h'");
    }

    size_t lineStart = headerfileContent.rfind('\n', headerPos) + 1;
    std::string indent = headerfileContent.substr(lineStart, headerPos - lineStart);

    headerfileContent.insert(headerPos + strlen(INSERTION_POINT_FUNCTION_DECLARATION),
        "\n" + indent + funcDeclaration);

    size_t sourcePos = sourcefileContent.find(INSERTION_POINT_FUNCTION_IMPLEMENTATION);
    if (sourcePos == std::string::npos) {
        throw std::runtime_error("Insertion point not found in source' " + filePathPrefix + ".cpp'");
    }

    lineStart = sourcefileContent.rfind('\n', sourcePos) + 1;
    indent = sourcefileContent.substr(lineStart, sourcePos - lineStart);

    std::string rawDef =
        returnType + " " + funcName + "(" + params + ") {\n"
        "    // TODO: implement\n"
        "}\n";

    std::string funcDefinition = indent +
        std::regex_replace(rawDef, std::regex("\n"), "\n" + indent);

    sourcefileContent.insert(
        sourcePos + strlen(INSERTION_POINT_FUNCTION_IMPLEMENTATION),
        "\n" + funcDefinition
    );

    WriteToFile(filePathPrefix + ".h", headerfileContent);
    WriteToFile(filePathPrefix + ".cpp", sourcefileContent);
}

void implementFunction(const std::string& functionName, const std::string& component) {
    if (!IsFileExists(DEFINITIONS_FILE_PATH)) {
        std::cerr << "Definitions file does not exist '" << DEFINITIONS_FILE_PATH << "'" << std::endl;
        return;
    }

    try {
        SetFunctionStub(DEFINITIONS_FILE_PATH, functionName, false);

        WriteSourceAndHeaderFiles(functionName, component);

        std::cout << "Successfully implemented function '" << functionName
            << "' in component '" << component << "'" << std::endl;
    }
    catch (const std::runtime_error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    catch (...) {
        std::cerr << "Unknown error implementing function '" << functionName << "'" << std::endl;
    }
}
