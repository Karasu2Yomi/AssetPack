#ifdef _WIN32

#include "WinFileDialog.hpp"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shobjidl.h>   // IFileOpenDialog
#include <string>

namespace {

    static std::string WideToUtf8(const std::wstring& w) {
        if (w.empty()) return {};
        int size = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
        std::string out(size, '\0');
        WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), out.data(), size, nullptr, nullptr);
        return out;
    }

    static bool ShowDialog(std::string& outPath, FILEOPENDIALOGOPTIONS opts,
                           const wchar_t* title,
                           const COMDLG_FILTERSPEC* filters, UINT filterCount) {
        HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        bool coInited = SUCCEEDED(hr);

        IFileOpenDialog* dlg = nullptr;
        hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg));
        if (FAILED(hr) || !dlg) {
            if (coInited) CoUninitialize();
            return false;
        }

        dlg->SetTitle(title);

        FILEOPENDIALOGOPTIONS cur = 0;
        dlg->GetOptions(&cur);
        dlg->SetOptions(cur | opts);

        if (filters && filterCount > 0) {
            dlg->SetFileTypes(filterCount, filters);
            dlg->SetFileTypeIndex(1);
        }

        hr = dlg->Show(nullptr);
        if (FAILED(hr)) {
            dlg->Release();
            if (coInited) CoUninitialize();
            return false;
        }

        IShellItem* item = nullptr;
        hr = dlg->GetResult(&item);
        if (FAILED(hr) || !item) {
            dlg->Release();
            if (coInited) CoUninitialize();
            return false;
        }

        PWSTR pathW = nullptr;
        hr = item->GetDisplayName(SIGDN_FILESYSPATH, &pathW);
        item->Release();
        dlg->Release();

        if (FAILED(hr) || !pathW) {
            if (coInited) CoUninitialize();
            return false;
        }

        std::wstring w(pathW);
        CoTaskMemFree(pathW);

        outPath = WideToUtf8(w);

        if (coInited) CoUninitialize();
        return !outPath.empty();
    }
}

namespace Platform {

    bool OpenFileDialog(std::string& outPath,
                    const wchar_t* title,
                    const wchar_t* filterName,
                    const wchar_t* filterSpec) {
        COMDLG_FILTERSPEC spec[1] = { { filterName, filterSpec } };
        return ShowDialog(outPath,
                    FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST,
                    title,
                    spec, 1);
    }

    bool OpenFolderDialog(std::string& outPath, const wchar_t* title) {
        // FOS_PICKFOLDERS = folder picker
        return ShowDialog(outPath,
                    FOS_FORCEFILESYSTEM | FOS_PICKFOLDERS,
                    title,
                    nullptr, 0);
    }

} // namespace platform

#endif // _WIN32