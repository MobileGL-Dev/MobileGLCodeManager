#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <unordered_map>
#include <functional>
#include <algorithm>

#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#endif

// forward declarations
void implementFunction(const std::string& functionName, const std::string& component);

static bool isProgramClosed = false;

using CommandHandler = std::function<void(const std::vector<std::string>& args)>;

std::unordered_map<std::string, CommandHandler> commandMap;

// --- utils ---
std::vector<std::string> split(const std::string& line) {
    std::istringstream iss(line);
    std::vector<std::string> tokens;
    std::string token;
    while (iss >> token) tokens.push_back(token);
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

// --- Simple line editor with arrow keys and history support ---
class LineEditor {
public:
    LineEditor() = default;

    // Returns false on EOF/terminate
    bool readLine(const std::string& prompt, std::string& outLine) {
#ifdef _WIN32
        return readLineWindows(prompt, outLine);
#else
        return readLinePosix(prompt, outLine);
#endif
    }

private:
    std::vector<std::string> history;
    int historyIndex = -1; // -1 means no history selection

#ifdef _WIN32
    int getch_nonblocking() {
        // blocking _getch
        return _getch();
    }

    bool readLineWindows(const std::string& prompt, std::string& outLine) {
        std::string buffer;
        size_t cursor = 0;
        int prevDisplayLen = 0;

        std::cout << prompt;
        std::cout.flush();

        while (!isProgramClosed) {
            int c = getch_nonblocking();
            if (c == 0 || c == 224) {
                // special key, read next
                int c2 = getch_nonblocking();
                if (c2 == 72) { // up
                    if (!history.empty()) {
                        if (historyIndex < 0) historyIndex = (int)history.size() - 1;
                        else historyIndex = std::max(0, historyIndex - 1);
                        buffer = history[historyIndex];
                        cursor = buffer.size();
                        redraw(prompt, buffer, cursor, prevDisplayLen);
                    }
                } else if (c2 == 80) { // down
                    if (!history.empty()) {
                        if (historyIndex < 0) {
                            // nothing
                        } else {
                            historyIndex = std::min((int)history.size() - 1, historyIndex + 1);
                            if (historyIndex >= 0 && historyIndex < (int)history.size()) {
                                buffer = history[historyIndex];
                            } else {
                                buffer.clear();
                            }
                            cursor = buffer.size();
                            if (historyIndex >= (int)history.size()) historyIndex = -1;
                            redraw(prompt, buffer, cursor, prevDisplayLen);
                        }
                    }
                } else if (c2 == 75) { // left
                    if (cursor > 0) {
                        --cursor;
                        moveCursorLeft(1);
                    }
                } else if (c2 == 77) { // right
                    if (cursor < buffer.size()) {
                        ++cursor;
                        moveCursorRight(1);
                    }
                } else {
                    // ignore others
                }
            } else if (c == 13) {
                // Enter
                std::cout << std::endl;
                if (!buffer.empty()) {
                    history.push_back(buffer);
                }
                historyIndex = -1;
                outLine = buffer;
                return true;
            } else if (c == 3) {
                // Ctrl-C
                std::cout << "^C" << std::endl;
                buffer.clear();
                cursor = 0;
                redraw(prompt, buffer, cursor, prevDisplayLen);
                // do not exit program; just clear
            } else if (c == 8) {
                // Backspace
                if (cursor > 0) {
                    buffer.erase(cursor - 1, 1);
                    --cursor;
                    redraw(prompt, buffer, cursor, prevDisplayLen);
                }
            } else if (c == 0x1a) {
                // Ctrl-Z maybe EOF in some consoles; treat as EOF
                std::cout << std::endl;
                isProgramClosed = true;
                return false;
            } else if (c >= 32 && c <= 126) {
                // printable
                buffer.insert(buffer.begin() + cursor, (char)c);
                ++cursor;
                redraw(prompt, buffer, cursor, prevDisplayLen);
            } else {
                // ignore
            }
        } // while
        return false;
    }

    void moveCursorLeft(int n) {
        while (n--) std::cout << '\b';
        std::cout.flush();
    }
    void moveCursorRight(int n) {
        // print the character under cursor by reprinting remainder then moving back
        // simpler: output the characters to move right one-by-one if possible
        // But safe approach: just redraw line (handled by caller normally). For small moves we can:
        // This implementation will just re-render full line when needed; here do nothing.
        (void)n;
    }

    void redraw(const std::string& prompt, const std::string& buffer, size_t cursor, int &prevDisplayLen) {
        // carriage return, print prompt + buffer, pad with spaces to clear leftover, then reposition cursor
        std::cout << '\r' << prompt << buffer;
        int curDisplayLen = (int)(prompt.size() + buffer.size());
        if (prevDisplayLen > curDisplayLen) {
            // clear leftover
            for (int i = 0; i < prevDisplayLen - curDisplayLen; ++i) std::cout << ' ';
            // move back again
            std::cout << '\r' << prompt << buffer;
        }
        prevDisplayLen = curDisplayLen;
        // move cursor to the logical position
        // move back from end to cursor position
        int moveBack = (int)(buffer.size() - cursor);
        for (int i = 0; i < moveBack; ++i) std::cout << '\b';
        std::cout.flush();
    }

#else // POSIX

    struct TermState {
        termios orig;
        bool valid = false;
    };

    bool enableRawMode(TermState &st) {
        if (!isatty(STDIN_FILENO)) return false;
        if (tcgetattr(STDIN_FILENO, &st.orig) == -1) return false;
        termios raw = st.orig;
        raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
        raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
        raw.c_oflag &= ~(OPOST);
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0;
        if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) return false;
        st.valid = true;
        return true;
    }

    void disableRawMode(TermState &st) {
        if (st.valid) {
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &st.orig);
            st.valid = false;
        }
    }

    int readByte() {
        unsigned char c;
        ssize_t n = read(STDIN_FILENO, &c, 1);
        if (n <= 0) return -1;
        return c;
    }

    bool readLinePosix(const std::string& prompt, std::string& outLine) {
        TermState st;
        bool rawOk = enableRawMode(st);
        std::string buffer;
        size_t cursor = 0;
        int prevDisplayLen = 0;

        std::cout << prompt;
        std::cout.flush();

        while (!isProgramClosed) {
            int c = readByte();
            if (c == -1) {
                disableRawMode(st);
                isProgramClosed = true;
                return false;
            }
            if (c == '\r' || c == '\n') {
                std::cout << std::endl;
                if (!buffer.empty()) history.push_back(buffer);
                historyIndex = -1;
                outLine = buffer;
                disableRawMode(st);
                return true;
            } else if (c == 127 || c == 8) { // backspace (127 on many terminals)
                if (cursor > 0) {
                    buffer.erase(cursor - 1, 1);
                    --cursor;
                    redraw(prompt, buffer, cursor, prevDisplayLen);
                }
            } else if (c == 3) { // Ctrl-C
                std::cout << "^C" << std::endl;
                buffer.clear();
                cursor = 0;
                redraw(prompt, buffer, cursor, prevDisplayLen);
            } else if (c == 4) { // Ctrl-D -> treat as EOF
                std::cout << std::endl;
                disableRawMode(st);
                isProgramClosed = true;
                return false;
            } else if (c == 27) {
                // Escape sequence: read two more (maybe more)
                int c1 = readByte();
                if (c1 == -1) { disableRawMode(st); return false; }
                if (c1 == '[') {
                    int c2 = readByte();
                    if (c2 == -1) { disableRawMode(st); return false; }
                    if (c2 == 'A') { // up
                        if (!history.empty()) {
                            if (historyIndex < 0) historyIndex = (int)history.size() - 1;
                            else historyIndex = std::max(0, historyIndex - 1);
                            buffer = history[historyIndex];
                            cursor = buffer.size();
                            redraw(prompt, buffer, cursor, prevDisplayLen);
                        }
                    } else if (c2 == 'B') { // down
                        if (history.empty()) return false;

                        if (historyIndex >= 0) {
                            historyIndex++;
                            if (historyIndex >= (int)history.size()) {
                                historyIndex = -1;
                                buffer.clear();
                            } else {
                                buffer = history[historyIndex];
                            }
                            cursor = buffer.size();
                            redraw(prompt, buffer, cursor, prevDisplayLen);
                        }
                    } else if (c2 == 'C') { // right
                        if (cursor < buffer.size()) {
                            ++cursor;
                            redraw(prompt, buffer, cursor, prevDisplayLen);
                        }
                    } else if (c2 == 'D') { // left
                        if (cursor > 0) {
                            --cursor;
                            redraw(prompt, buffer, cursor, prevDisplayLen);
                        }
                    } else {
                        // other CSI sequences ignored
                    }
                } else {
                    // other escapes ignored
                }
            } else if (c >= 32 && c <= 126) {
                buffer.insert(buffer.begin() + cursor, (char)c);
                ++cursor;
                redraw(prompt, buffer, cursor, prevDisplayLen);
            } else {
                // ignore other controls
            }
        } // while

        disableRawMode(st);
        return false;
    }

    void redraw(const std::string& prompt, const std::string& buffer, size_t cursor, int &prevDisplayLen) {
        // Move to start of line, print prompt+buffer, clear leftover, then move cursor to position
        std::cout << '\r' << prompt << buffer;
        int curDisplayLen = (int)(prompt.size() + buffer.size());
        if (prevDisplayLen > curDisplayLen) {
            for (int i = 0; i < prevDisplayLen - curDisplayLen; ++i) std::cout << ' ';
            std::cout << '\r' << prompt << buffer;
        }
        prevDisplayLen = curDisplayLen;
        // move back from end to cursor position
        int moveBack = (int)(buffer.size() - cursor);
        for (int i = 0; i < moveBack; ++i) std::cout << '\b';
        std::cout.flush();
    }
#endif
};

// --- End LineEditor ---

void MainLoop() {
    LineEditor editor;
    std::string input;
    const std::string prompt = ">>> ";
    while (!isProgramClosed) {
        bool ok = editor.readLine(prompt, input);
        if (!ok) break; // EOF or program closed
        auto tokens = split(input);
        if (tokens.empty()) continue;

        auto cmd = tokens[0];
        auto it = commandMap.find(cmd);
        if (it != commandMap.end()) {
            it->second(tokens);
        } else {
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
