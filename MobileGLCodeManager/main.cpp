#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <unordered_map>
#include <functional>

// forward declarations
void implementFunction(const std::string& functionName, const std::string& component);

static bool isProgramClosed = false;

using CommandHandler = std::function<void(const std::vector<std::string>& args)>;

std::unordered_map<std::string, CommandHandler> commandMap;

std::vector<std::string> split(const std::string& line) {
    std::istringstream iss(line);
    std::vector<std::string> tokens;
    std::string token;
    while (iss >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

void CMD_help(const std::vector<std::string>& args) {
    std::cout << "Available commands:" << std::endl;
    for (auto& kv : commandMap) {
        std::cout << "  " << kv.first << std::endl;
    }
}

void CMD_exit(const std::vector<std::string>& args) {
    std::cout << "Exiting program..." << std::endl;
    isProgramClosed = true;
}

void CMD_implementFunction(const std::vector<std::string>& args) {
    if (args.size() < 3) {
        std::cout << "Usage: implementFunction <function_name> <component>" << std::endl;
        return;
    }
    std::string functionName = args[1];
    std::string component = args[2];
    std::cout << "Implementing function: " << functionName << " with component: " << component << std::endl;
    implementFunction(functionName, component);
}

void registerCommands() {
    commandMap["help"] = CMD_help;
    commandMap["exit"] = CMD_exit;
    commandMap["impl"] = CMD_implementFunction;
}

void MainLoop() {
    std::string input;
    while (!isProgramClosed) {
        std::cout << ">>> ";
        if (!std::getline(std::cin, input)) {
            isProgramClosed = true;
            break;
        }
        auto tokens = split(input);
        if (tokens.empty()) continue;

        auto cmd = tokens[0];
        auto it = commandMap.find(cmd);
        if (it != commandMap.end()) {
            it->second(tokens);
        }
        else {
            std::cout << "Unknown command: " << cmd << std::endl;
        }
    }
}

int main() {
    std::cout << "MobileGL Code Manager" << std::endl;
    registerCommands();
    MainLoop();
    return 0;
}
