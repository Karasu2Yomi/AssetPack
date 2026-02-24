#pragma once
#include <string>

namespace Platform {

    bool OpenFileDialog(std::string& outPath,
                        const wchar_t* title = L"Select a file",
                        const wchar_t* filterName = L"All files (*.*)",
                        const wchar_t* filterSpec = L"*.*");

    bool OpenFolderDialog(std::string& outPath,
                          const wchar_t* title = L"Select a folder");

}