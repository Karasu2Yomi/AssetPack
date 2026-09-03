#include "CsvCodec.hpp"

#include <cstddef>
#include <stdexcept>
#include <sstream>
#include <string>

namespace {

[[nodiscard]] bool HasUtf8Bom(const std::string_view text) noexcept {
    return text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEFu &&
           static_cast<unsigned char>(text[1]) == 0xBBu &&
           static_cast<unsigned char>(text[2]) == 0xBFu;
}

class CsvSyntaxError final : public std::runtime_error {
public:
    CsvSyntaxError(std::size_t line, std::size_t column, const std::string& detail)
        : std::runtime_error(MakeMessage(line, column, detail)) {}

private:
    static std::string MakeMessage(std::size_t line,
                                   std::size_t column,
                                   const std::string& detail) {
        std::ostringstream stream;
        stream << line << "行目、" << column << "列目: " << detail;
        return stream.str();
    }
};

enum class ParseState {
    Start,
    Unquoted,
    Quoted,
    AfterQuoted,
};

[[nodiscard]] bool NeedEscape(const std::string& field) {
    if (field.empty()) {
        return false;
    }
    for (const char ch : field) {
        if (ch == ',' || ch == '"' || ch == '\n' || ch == '\r') {
            return true;
        }
    }
    return false;
}

[[nodiscard]] std::string EscapeField(const std::string& field) {
    if (!NeedEscape(field)) {
        return field;
    }
    std::string escaped = "\"";
    for (const char ch : field) {
        if (ch == '"') {
            escaped.push_back('"');
        }
        escaped.push_back(ch);
    }
    escaped.push_back('"');
    return escaped;
}

} // namespace

namespace AssetPackCore::CsvCodec {

CsvParseResult Parse(const std::string_view text) {
    try {
        if (text.empty()) {
            return {std::nullopt, "1行目、1列目: CSVが空です。"};
        }

        std::string_view safe = text;
        if (HasUtf8Bom(safe)) {
            safe.remove_prefix(3);
        }

        std::vector<CsvRecord> records;
        std::vector<std::string> fields;
        std::string field;
        std::size_t index = 0;
        std::size_t line = 1;
        std::size_t column = 1;
        std::size_t recordLine = 1;
        std::size_t quotedLine = 0;
        std::size_t quotedColumn = 0;
        ParseState state = ParseState::Start;
        bool recordOpen = false;

        const auto consume = [&](bool advanced) {
            if (!advanced) return;
            ++index;
            ++column;
        };

        const auto consumeNewline = [&]() {
            if (safe[index] == '\r') {
                if (index + 1 >= safe.size() || safe[index + 1] != '\n') {
                    throw CsvSyntaxError(line, column,
                                         "改行コードのCRの直後にはLFが必要です。");
                }
                index += 2;
            } else {
                ++index;
            }
            ++line;
            column = 1;
        };

        const auto appendField = [&]() {
            fields.push_back(std::move(field));
            field.clear();
        };

        const auto appendRecord = [&]() {
            appendField();
            records.push_back({recordLine, std::move(fields)});
            fields.clear();
            state = ParseState::Start;
            recordOpen = false;
        };

        while (index < safe.size()) {
            const char ch = safe[index];
            const bool newline = ch == '\n' || ch == '\r';
            if (state == ParseState::Quoted) {
                if (ch == '"') {
                    if (index + 1 < safe.size() && safe[index + 1] == '"') {
                        field.push_back('"');
                        index += 2;
                        column += 2;
                        continue;
                    }
                    state = ParseState::AfterQuoted;
                    consume(true);
                    continue;
                }
                if (newline) {
                    field.push_back('\n');
                    consumeNewline();
                    continue;
                }
                field.push_back(ch);
                consume(true);
                continue;
            }

            if (newline) {
                appendRecord();
                consumeNewline();
                recordLine = line;
                continue;
            }

            if (state == ParseState::Start) {
                recordOpen = true;
                if (ch == '"') {
                    quotedLine = line;
                    quotedColumn = column;
                    state = ParseState::Quoted;
                    consume(true);
                } else if (ch == ',') {
                    appendField();
                    consume(true);
                } else {
                    field.push_back(ch);
                    state = ParseState::Unquoted;
                    consume(true);
                }
                continue;
            }

            if (state == ParseState::Unquoted) {
                if (ch == '"') {
                    throw CsvSyntaxError(line, column, "引用符で囲む項目は、先頭から引用符で始めてください。");
                }
                if (ch == ',') {
                    appendField();
                    state = ParseState::Start;
                    consume(true);
                } else {
                    field.push_back(ch);
                    consume(true);
                }
                continue;
            }

            if (ch == ',') {
                appendField();
                state = ParseState::Start;
                consume(true);
                continue;
            }
            throw CsvSyntaxError(line, column, "閉じ引用符の後に使用できない文字があります。");
        }

        if (state == ParseState::Quoted) {
            throw CsvSyntaxError(quotedLine, quotedColumn, "引用符で囲まれた項目が閉じられていません。");
        }
        if (recordOpen) {
            appendRecord();
        }

        if (records.empty()) {
            throw CsvSyntaxError(1, 1, "CSVにヘッダー行がありません。");
        }

        CsvDocument out;
        out.header = std::move(records.front().fields);
        const std::size_t expected = out.header.size();
        out.records.reserve(records.size() - 1);
        for (std::size_t i = 1; i < records.size(); ++i) {
            CsvRecord& rec = records[i];
            if (rec.fields.size() != expected) {
                throw CsvSyntaxError(rec.lineNumber, 1,
                                     "項目数が" + std::to_string(rec.fields.size()) +
                                         "個です。" + std::to_string(expected) + "個必要です。");
            }
            out.records.push_back(std::move(rec));
        }
        return {std::move(out), {}};
    } catch (const CsvSyntaxError& ex) {
        return {std::nullopt, ex.what()};
    } catch (const std::exception&) {
        return {std::nullopt, "CSVの解析中に予期しないエラーが発生しました。"};
    }
}

std::string Serialize(const CsvDocument& document) {
    std::string out;

    const auto emitLine = [&](const std::vector<std::string>& values) {
        for (std::size_t i = 0; i < values.size(); ++i) {
            if (i > 0) {
                out.push_back(',');
            }
            out += EscapeField(values[i]);
        }
        out += "\r\n";
    };

    emitLine(document.header);
    for (const auto& rec : document.records) {
        emitLine(rec.fields);
    }
    return out;
}

} // namespace AssetPackCore::CsvCodec
