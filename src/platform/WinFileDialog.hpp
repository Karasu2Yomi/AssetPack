#pragma once
#include <string>

namespace Platform {

    bool OpenFileDialog(std::string& outPath,
                        const wchar_t* title = L"ファイルを選択",
                        const wchar_t* filterName = L"すべてのファイル (*.*)",
                        const wchar_t* filterSpec = L"*.*");

    bool OpenFolderDialog(std::string& outPath,
                          const wchar_t* title = L"フォルダを選択");

}
