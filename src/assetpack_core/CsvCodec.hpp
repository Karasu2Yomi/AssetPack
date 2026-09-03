#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace AssetPackCore {

struct CsvRecord {
    std::size_t lineNumber = 0;
    std::vector<std::string> fields;
};

struct CsvDocument {
    std::vector<std::string> header;
    std::vector<CsvRecord> records;
};

struct CsvParseResult {
    std::optional<CsvDocument> document;
    std::string error;

    [[nodiscard]] bool Succeeded() const noexcept { return document.has_value(); }
    [[nodiscard]] explicit operator bool() const noexcept { return Succeeded(); }
};

namespace CsvCodec {

CsvParseResult Parse(std::string_view text);
std::string Serialize(const CsvDocument& document);

} // namespace CsvCodec

} // namespace AssetPackCore
