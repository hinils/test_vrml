#pragma once
#include <string>
#include <vector>
#include <stdexcept>
#include <cctype>
#include <cstring>

namespace vrml {
namespace detail {

// ─────────────────────────────────────────────────────────────────
//  Token
// ─────────────────────────────────────────────────────────────────
enum class TokType {
    Word,       // keyword, DEF name, field name
    String,     // "..."
    Number,     // integer or float literal
    LBrace,     // {
    RBrace,     // }
    LBracket,   // [
    RBracket,   // ]
    Comma,      // ,
    Dot,        // .  (VRML1 SFRotation uses spaces, rarely dots appear)
    Eof
};

struct Token {
    TokType     type;
    std::string value;
    int         line = 0;
};

// ─────────────────────────────────────────────────────────────────
//  Lexer
// ─────────────────────────────────────────────────────────────────
class Lexer {
public:
    explicit Lexer(const std::string& src)
        : src_(src), pos_(0), line_(1) {}

    Token next() {
        skipWhitespaceAndComments();
        if (pos_ >= src_.size()) return makeToken(TokType::Eof, "");

        char c = src_[pos_];

        // Single-char tokens
        if (c == '{') { ++pos_; return makeToken(TokType::LBrace,   "{"); }
        if (c == '}') { ++pos_; return makeToken(TokType::RBrace,   "}"); }
        if (c == '[') { ++pos_; return makeToken(TokType::LBracket, "["); }
        if (c == ']') { ++pos_; return makeToken(TokType::RBracket, "]"); }
        if (c == ',') { ++pos_; return makeToken(TokType::Comma,    ","); }
        if (c == '.') {
            // Could be "0.5" with leading dot OR a separator
            if (pos_+1 < src_.size() && std::isdigit((unsigned char)src_[pos_+1])) {
                return readNumber();
            }
            ++pos_; return makeToken(TokType::Dot, ".");
        }

        // Quoted string
        if (c == '"') return readString();

        // Number (including negative)
        if (std::isdigit((unsigned char)c) ||
            (c == '-' && pos_+1 < src_.size() &&
             (std::isdigit((unsigned char)src_[pos_+1]) || src_[pos_+1] == '.'))) {
            return readNumber();
        }
        if (c == '+' && pos_+1 < src_.size() &&
            std::isdigit((unsigned char)src_[pos_+1])) {
            return readNumber();
        }

        // Word / keyword / identifier
        if (std::isalpha((unsigned char)c) || c == '_' || c == '#') {
            // '#' at non-line-start shouldn't happen (comments stripped), but guard
            return readWord();
        }

        // Unknown character – skip
        ++pos_;
        return next();
    }

    // Peek without consuming
    Token peek() {
        size_t savedPos  = pos_;
        int    savedLine = line_;
        Token  t = next();
        pos_  = savedPos;
        line_ = savedLine;
        return t;
    }

    // Consume and return next token, throw if not expected type
    Token expect(TokType type, const char* hint = "") {
        Token t = next();
        if (t.type != type) {
            throw std::runtime_error(
                std::string("VRML parse error line ") + std::to_string(t.line) +
                ": expected " + hint + ", got '" + t.value + "'");
        }
        return t;
    }

    bool atEnd() {
        skipWhitespaceAndComments();
        return pos_ >= src_.size();
    }

    int currentLine() const { return line_; }

private:
    const std::string& src_;
    size_t pos_;
    int    line_;

    Token makeToken(TokType t, const std::string& v) {
        return Token{t, v, line_};
    }

    void skipWhitespaceAndComments() {
        while (pos_ < src_.size()) {
            char c = src_[pos_];
            if (c == '\n') { ++line_; ++pos_; }
            else if (std::isspace((unsigned char)c)) { ++pos_; }
            else if (c == '#') {
                // line comment
                while (pos_ < src_.size() && src_[pos_] != '\n') ++pos_;
            }
            else { break; }
        }
    }

    Token readNumber() {
        size_t start = pos_;
        if (src_[pos_] == '+' || src_[pos_] == '-') ++pos_;
        while (pos_ < src_.size() && std::isdigit((unsigned char)src_[pos_])) ++pos_;
        if (pos_ < src_.size() && src_[pos_] == '.') {
            ++pos_;
            while (pos_ < src_.size() && std::isdigit((unsigned char)src_[pos_])) ++pos_;
        }
        // exponent
        if (pos_ < src_.size() && (src_[pos_] == 'e' || src_[pos_] == 'E')) {
            ++pos_;
            if (pos_ < src_.size() && (src_[pos_] == '+' || src_[pos_] == '-')) ++pos_;
            while (pos_ < src_.size() && std::isdigit((unsigned char)src_[pos_])) ++pos_;
        }
        return makeToken(TokType::Number, src_.substr(start, pos_-start));
    }

    Token readString() {
        ++pos_; // skip opening "
        std::string val;
        while (pos_ < src_.size() && src_[pos_] != '"') {
            if (src_[pos_] == '\\' && pos_+1 < src_.size()) {
                ++pos_;
                char esc = src_[pos_++];
                switch (esc) {
                    case 'n':  val += '\n'; break;
                    case 't':  val += '\t'; break;
                    case '"':  val += '"';  break;
                    case '\\': val += '\\'; break;
                    default:   val += esc;  break;
                }
            } else {
                if (src_[pos_] == '\n') ++line_;
                val += src_[pos_++];
            }
        }
        if (pos_ < src_.size()) ++pos_; // closing "
        return makeToken(TokType::String, val);
    }

    Token readWord() {
        size_t start = pos_;
        while (pos_ < src_.size()) {
            char c = src_[pos_];
            if (std::isalnum((unsigned char)c) || c == '_' || c == '-' || c == ':')
                ++pos_;
            else
                break;
        }
        return makeToken(TokType::Word, src_.substr(start, pos_-start));
    }
};

} // namespace detail
} // namespace vrml
