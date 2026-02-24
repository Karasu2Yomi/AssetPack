#include "PathUtil.hpp"

#include <system_error>
#include <fstream>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#endif

namespace Util {

    std::string ToUtf8(const std::filesystem::path& p) {
        const auto u8 = p.u8string(); // std::u8string
        return std::string(reinterpret_cast<const char*>(u8.data()), u8.size());
    }

    std::filesystem::path GetExeDir() {
        #ifdef _WIN32
        // UTF-16で取得
        wchar_t buf[MAX_PATH];
        DWORD len = GetModuleFileNameW(nullptr, buf, MAX_PATH);
        if (len == 0 || len == MAX_PATH) {
            // 最後の手段：カレント
            return std::filesystem::current_path();
        }
        std::filesystem::path exePath(buf);
        return exePath.parent_path();
        #else
        // 非Windowsならカレント（必要なら /proc/self/exe を実装）
        return std::filesystem::current_path();
        #endif
    }

    bool EnsureDir(const std::filesystem::path& baseDir,
                   const std::filesystem::path& dstDirRelative,
                   std::string* outErr) {
        std::error_code ec;
        auto dir = std::filesystem::weakly_canonical(baseDir / dstDirRelative, ec);
        if (ec) dir = baseDir / dstDirRelative;

        if (std::filesystem::exists(dir, ec)) {
            if (!std::filesystem::is_directory(dir, ec)) {
                if (outErr) {
                    *outErr = std::string("パスは存在しますがディレクトリではありません: ") + ToUtf8(dir);
                }
                return false;
            }
            return true;
        }
        if (!std::filesystem::create_directories(dir, ec)) {
            if (outErr) *outErr = std::string("ディレクトリ作成に失敗: ") + ToUtf8(dir) + " (" + ec.message() + ")";
            return false;
        }
        return true;
    }

    std::filesystem::path MakeUniquePath(const std::filesystem::path& dstPath) {
        std::error_code ec;
        if (!std::filesystem::exists(dstPath, ec)) return dstPath;

        auto stem = ToUtf8(dstPath.stem());
        auto ext  = ToUtf8(dstPath.extension());
        auto dir  = dstPath.parent_path();

        for (int i = 1; i < 10'000; ++i) {
            std::filesystem::path candidate = dir / (std::filesystem::path(stem + "_" + std::to_string(i) + ext));
            if (!std::filesystem::exists(candidate, ec)) return candidate;
        }
        // 異常系：最後に元を返す
        return dstPath;
    }

    static bool IsUnderBase(const std::filesystem::path& p, const std::filesystem::path& base) {
        std::error_code ec;
        auto cp   = std::filesystem::weakly_canonical(p, ec);
        if (ec) cp = std::filesystem::absolute(p);

        auto cb   = std::filesystem::weakly_canonical(base, ec);
        if (ec) cb = std::filesystem::absolute(base);

        // プレフィックス比較
        auto pit = cp.begin();
        auto bit = cb.begin();
        for (; bit != cb.end(); ++bit, ++pit) {
            if (pit == cp.end()) return false;
            if (*pit != *bit) return false;
        }
        return true;
    }

    std::optional<std::filesystem::path> TryMakeRelativeIfUnderBase(
        const std::filesystem::path& absolutePath,
        const std::filesystem::path& baseDir) {

        if (!absolutePath.is_absolute()) {
            // 既に相対ならそのまま返す（呼び出し側で判断）
            return absolutePath;
        }
        if (!IsUnderBase(absolutePath, baseDir)) return std::nullopt;

        std::error_code ec;
        auto rel = std::filesystem::relative(absolutePath, baseDir, ec);
        if (ec) return std::nullopt;
        return rel;
    }

    CopyResult CopyIntoBaseAndMakeRelative(const std::filesystem::path& srcFile,
                                      const std::filesystem::path& baseDir,
                                      const std::filesystem::path& dstDirRelative,
                                      bool overwrite) {
        CopyResult r;

        std::error_code ec;
        if (!std::filesystem::exists(srcFile, ec) || !std::filesystem::is_regular_file(srcFile, ec)) {
            r.message = std::string("ソースファイルが存在しない/通常ファイルではない: ") + ToUtf8(srcFile);
            return r;
        }

        // baseDir/dstDirRelative を用意
        std::string err;
        if (!EnsureDir(baseDir, dstDirRelative, &err)) {
            r.message = err;
            return r;
        }

        auto dstDirAbs = baseDir / dstDirRelative;
        auto dstPath = dstDirAbs / srcFile.filename();

        if (!overwrite) {
            dstPath = MakeUniquePath(dstPath);
        }

        std::filesystem::copy_options opt = overwrite
            ? std::filesystem::copy_options::overwrite_existing
            : std::filesystem::copy_options::none;

        std::filesystem::copy_file(srcFile, dstPath, opt, ec);
        if (ec) {
            r.message = std::string("コピー失敗: ") + ToUtf8(dstPath) + " (" + ec.message() + ")";
            return r;
        }

        auto relOpt = TryMakeRelativeIfUnderBase(dstPath, baseDir);
        if (!relOpt) {
            r.message = std::string("相対パス化に失敗（baseDir配下判定NG）: ") + ToUtf8(dstPath);
            return r;
        }

        r.ok = true;
        r.absoluteDst = dstPath;
        r.relativeDst = *relOpt; // 例: tilesets/basic.png
        return r;
    }

}