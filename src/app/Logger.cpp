#include "Logger.hpp"

namespace App {
    void Log(EditorAppState& s, const std::string& msg) {
        s.console.push_back(msg);
        if (s.console.size() > 5000) s.console.erase(s.console.begin(), s.console.begin() + 1000);
    }
}