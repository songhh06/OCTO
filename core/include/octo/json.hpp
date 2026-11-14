#pragma once
#include <array>
#include <charconv>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <variant>
#include <vector>

namespace octo::json {
inline std::filesystem::path path_utf8(const std::string &s) {
    return std::filesystem::path(std::u8string(s.begin(), s.end()));
}
struct Number {
    std::string token;
};
struct Value {
    using Array = std::vector<Value>;
    using Object = std::map<std::string, Value>;
    std::variant<std::monostate, bool, Number, std::string, Array, Object> data;
    Value() = default;
    Value(bool v) : data(v) {}
    Value(std::string v) : data(std::move(v)) {}
    Value(const char *v) : data(std::string(v)) {}
    Value(Array v) : data(std::move(v)) {}
    Value(Object v) : data(std::move(v)) {}
    Value(double v) {
        if (!std::isfinite(v))
            throw std::invalid_argument("nonfinite JSON number");
        std::ostringstream s;
        s.imbue(std::locale::classic());
        s << std::setprecision(17) << v;
        data = Number{s.str()};
    }
    static Value integer(std::uint64_t v) {
        Value n;
        n.data = Number{std::to_string(v)};
        return n;
    }
    const Object &object() const { return std::get<Object>(data); }
    const Array &array() const { return std::get<Array>(data); }
    const std::string &string() const { return std::get<std::string>(data); }
    bool boolean() const { return std::get<bool>(data); }
    double number() const {
        const auto &s = std::get<Number>(data).token;
        double d{};
        auto [p, e] = std::from_chars(s.data(), s.data() + s.size(), d);
        if (e != std::errc{} || p != s.data() + s.size() || !std::isfinite(d))
            throw std::invalid_argument("invalid finite JSON number");
        return d;
    }
    std::uint64_t u64() const {
        const auto &s = std::get<Number>(data).token;
        std::uint64_t n{};
        auto [p, e] = std::from_chars(s.data(), s.data() + s.size(), n);
        if (e == std::errc{} && p == s.data() + s.size())
            return n;
        double d = number();
        if (d < 0 || d > 9007199254740991.0 || d != std::floor(d))
            throw std::invalid_argument("expected exact unsigned integer");
        return static_cast<std::uint64_t>(d);
    }
    const Value &at(const std::string &key) const { return object().at(key); }
    bool has(const std::string &key) const { return object().contains(key); }
    double get(const std::string &key, double fallback) const {
        return has(key) ? at(key).number() : fallback;
    }
    bool flag(const std::string &key, bool fallback) const {
        return has(key) ? at(key).boolean() : fallback;
    }
};
inline void utf8(std::string &s, unsigned n) {
    if (n <= 0x7f)
        s += static_cast<char>(n);
    else if (n <= 0x7ff) {
        s += static_cast<char>(0xc0 | (n >> 6));
        s += static_cast<char>(0x80 | (n & 63));
    } else if (n <= 0xffff) {
        s += static_cast<char>(0xe0 | (n >> 12));
        s += static_cast<char>(0x80 | ((n >> 6) & 63));
        s += static_cast<char>(0x80 | (n & 63));
    } else {
        s += static_cast<char>(0xf0 | (n >> 18));
        s += static_cast<char>(0x80 | ((n >> 12) & 63));
        s += static_cast<char>(0x80 | ((n >> 6) & 63));
        s += static_cast<char>(0x80 | (n & 63));
    }
}
class Parser {
    const std::string &s;
    std::size_t p = 0;
    unsigned depth = 0;
    char peek() const { return p < s.size() ? s[p] : '\0'; }
    void ws() {
        while (peek() == ' ' || peek() == '\n' || peek() == '\r' || peek() == '\t')
            ++p;
    }
    void expect(char c) {
        if (peek() != c)
            throw std::invalid_argument("JSON token at byte " + std::to_string(p));
        ++p;
    }
    unsigned hex() {
        unsigned v = 0;
        for (unsigned j = 0; j < 4; ++j) {
            char c = peek();
            ++p;
            v *= 16;
            if (c >= '0' && c <= '9')
                v += c - '0';
            else if (c >= 'a' && c <= 'f')
                v += c - 'a' + 10;
            else if (c >= 'A' && c <= 'F')
                v += c - 'A' + 10;
            else
                throw std::invalid_argument("invalid unicode escape");
        }
        return v;
    }
    std::string text() {
        expect('"');
        std::string out;
        while (peek() != '"') {
            if (p >= s.size() || static_cast<unsigned char>(peek()) < 32)
                throw std::invalid_argument("invalid JSON string");
            char c = s[p++];
            if (c != '\\') {
                out += c;
                continue;
            }
            char e = peek();
            ++p;
            switch (e) {
            case '"':
            case '\\':
            case '/':
                out += e;
                break;
            case 'b':
                out += '\b';
                break;
            case 'f':
                out += '\f';
                break;
            case 'n':
                out += '\n';
                break;
            case 'r':
                out += '\r';
                break;
            case 't':
                out += '\t';
                break;
            case 'u': {
                unsigned v = hex();
                if (v >= 0xd800 && v <= 0xdbff) {
                    expect('\\');
                    expect('u');
                    unsigned low = hex();
                    if (low < 0xdc00 || low > 0xdfff)
                        throw std::invalid_argument("invalid surrogate");
                    v = 0x10000 + ((v - 0xd800) << 10) + low - 0xdc00;
                } else if (v >= 0xdc00 && v <= 0xdfff)
                    throw std::invalid_argument("invalid surrogate");
                utf8(out, v);
                break;
            }
            default:
                throw std::invalid_argument("invalid JSON escape");
            }
        }
        ++p;
        return out;
    }
    Value value() {
        ws();
        if (++depth > 128)
            throw std::invalid_argument("JSON nesting limit");
        Value out;
        if (peek() == '"')
            out = Value(text());
        else if (peek() == '{') {
            ++p;
            Value::Object map;
            ws();
            if (peek() != '}')
                for (;;) {
                    ws();
                    auto key = text();
                    ws();
                    expect(':');
                    if (map.contains(key))
                        throw std::invalid_argument("duplicate JSON key");
                    map.emplace(key, value());
                    ws();
                    if (peek() != ',')
                        break;
                    ++p;
                }
            expect('}');
            out = Value(std::move(map));
        } else if (peek() == '[') {
            ++p;
            Value::Array a;
            ws();
            if (peek() != ']')
                for (;;) {
                    a.push_back(value());
                    ws();
                    if (peek() != ',')
                        break;
                    ++p;
                }
            expect(']');
            out = Value(std::move(a));
        } else if (s.compare(p, 4, "true") == 0) {
            p += 4;
            out = Value(true);
        } else if (s.compare(p, 5, "false") == 0) {
            p += 5;
            out = Value(false);
        } else if (s.compare(p, 4, "null") == 0) {
            p += 4;
            out = Value();
        } else {
            auto begin = p;
            if (peek() == '-')
                ++p;
            if (peek() == '0')
                ++p;
            else {
                if (peek() < '1' || peek() > '9')
                    throw std::invalid_argument("JSON number expected");
                while (peek() >= '0' && peek() <= '9')
                    ++p;
            }
            if (peek() == '.') {
                ++p;
                auto first = p;
                while (peek() >= '0' && peek() <= '9')
                    ++p;
                if (first == p)
                    throw std::invalid_argument("JSON fraction expected");
            }
            if (peek() == 'e' || peek() == 'E') {
                ++p;
                if (peek() == '-' || peek() == '+')
                    ++p;
                auto first = p;
                while (peek() >= '0' && peek() <= '9')
                    ++p;
                if (first == p)
                    throw std::invalid_argument("JSON exponent expected");
            }
            out.data = Number{s.substr(begin, p - begin)};
            (void)out.number();
        }
        --depth;
        return out;
    }

  public:
    explicit Parser(const std::string &text) : s(text) {
        if (s.starts_with("\xef\xbb\xbf"))
            p = 3;
    }
    Value parse() {
        auto v = value();
        ws();
        if (p != s.size())
            throw std::invalid_argument("trailing JSON data");
        return v;
    }
};
inline std::string quote(const std::string &s) {
    std::string out = "\"";
    for (unsigned char c : s) {
        switch (c) {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            if (c < 32) {
                const char *h = "0123456789abcdef";
                out += "\\u00";
                out += h[c >> 4];
                out += h[c & 15];
            } else
                out += static_cast<char>(c);
        }
    }
    return out + '"';
}
inline std::string dump(const Value &v) {
    if (std::holds_alternative<std::monostate>(v.data))
        return "null";
    if (auto p = std::get_if<bool>(&v.data))
        return *p ? "true" : "false";
    if (auto p = std::get_if<Number>(&v.data))
        return p->token;
    if (auto p = std::get_if<std::string>(&v.data))
        return quote(*p);
    std::string out;
    if (auto p = std::get_if<Value::Array>(&v.data)) {
        out = "[";
        for (const auto &x : *p) {
            if (out.size() > 1)
                out += ',';
            out += dump(x);
        }
        return out + ']';
    }
    out = "{";
    for (const auto &[k, x] : v.object()) {
        if (out.size() > 1)
            out += ',';
        out += quote(k) + ':' + dump(x);
    }
    return out + '}';
}
inline std::string read(const std::filesystem::path &path) {
    std::ifstream f(path, std::ios::binary);
    if (!f)
        throw std::runtime_error("cannot read file");
    constexpr std::size_t limit = 50 * 1024 * 1024;
    std::string text;
    std::array<char, 8192> buffer{};
    while (f) {
        f.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        auto count = static_cast<std::size_t>(f.gcount());
        if (count > limit - text.size())
            throw std::invalid_argument("JSON file exceeds 50 MiB");
        text.append(buffer.data(), count);
    }
    if (!f.eof())
        throw std::runtime_error("JSON read failed");
    return text;
}
inline Value load(const std::filesystem::path &p) { return Parser(read(p)).parse(); }
inline void save(const std::filesystem::path &p, const Value &v) {
    std::ofstream f(p, std::ios::binary);
    if (!f)
        throw std::runtime_error("cannot write JSON file");
    f << dump(v) << '\n';
    if (!f)
        throw std::runtime_error("JSON write failed");
}
} // namespace octo::json
