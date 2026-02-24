#pragma once

namespace Pipeline {
    struct IoError {
        bool ok = true;
        std::string message;
    };
}