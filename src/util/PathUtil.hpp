#pragma once
#include <string>
#include <filesystem>
#include <optional>

namespace Util {

    struct CopyResult {
        bool ok = false;
        std::filesystem::path absoluteDst;  // コピー先（絶対）
        std::filesystem::path relativeDst;  // baseDir からの相対（保存用）
        std::string message;               // 失敗時理由など
    };

    // exe のあるディレクトリを取得（Windows/MinGW対応）
    std::filesystem::path GetExeDir();

    // baseDir 配下に dstDirRelative を確実に作る（無ければ作成）
    bool EnsureDir(const std::filesystem::path& baseDir,
                   const std::filesystem::path& dstDirRelative,
                   std::string* outErr = nullptr);

    // "衝突しないファイル名" を作る（同名がある場合は _1,_2...）
    std::filesystem::path MakeUniquePath(const std::filesystem::path& dstPath);

    // ファイルを baseDir/dstDirRelative にコピーし、baseDir からの相対パスを返す
    CopyResult CopyIntoBaseAndMakeRelative(const std::filesystem::path& srcFile,
                                          const std::filesystem::path& baseDir,
                                          const std::filesystem::path& dstDirRelative,
                                          bool overwrite = false);

    // 既に baseDir 配下なら、その相対パスを返す（配下でなければ nullopt）
    std::optional<std::filesystem::path> TryMakeRelativeIfUnderBase(
        const std::filesystem::path& absolutePath,
        const std::filesystem::path& baseDir);

    std::string ToUtf8(const std::filesystem::path& p);
}