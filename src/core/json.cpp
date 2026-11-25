#include "core/json.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace fl {
namespace {

const Json& null_singleton() {
    static const Json kNull;
    return kNull;
}

void append_utf8(std::string& out, unsigned cp) {
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

void append_escaped(std::string& out, std::string_view s) {
    out.push_back('"');
    for (char c : s) {
        const unsigned char uc = static_cast<unsigned char>(c);
        switch (c) {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\b':
            out += "\\b";
            break;
        case '\f':
            out += "\\f";
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
            if (uc < 0x20) {
                char buf[8];
                std::snprintf(buf, sizeof(buf), "\\u%04x", uc);
                out += buf;
            } else {
                out.push_back(c);
            }
            break;
        }
    }
    out.push_back('"');
}

void append_number(std::string& out, double v) {
    if (!std::isfinite(v)) {
        out += "0";
        return;
    }
    if (v == std::floor(v) && std::fabs(v) < 1e15) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(v));
        out += buf;
        return;
    }
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%.15g", v);
    if (std::strtod(buf, nullptr) != v) {
        std::snprintf(buf, sizeof(buf), "%.17g", v);
    }
    out += buf;
}

class Parser {
public:
    Parser(std::string_view text, std::string* error)
        : begin_(text.data()), p_(text.data()), end_(text.data() + text.size()), error_(error) {
        // 跳过 UTF-8 BOM
        if (end_ - p_ >= 3 && static_cast<unsigned char>(p_[0]) == 0xEF
            && static_cast<unsigned char>(p_[1]) == 0xBB
            && static_cast<unsigned char>(p_[2]) == 0xBF) {
            p_ += 3;
        }
    }

    bool parse(Json* out) {
        skip_ws();
        if (!parse_value(out, 0)) {
            return false;
        }
        skip_ws();
        if (p_ != end_) {
            return fail("末尾存在多余字符");
        }
        return true;
    }

private:
    bool fail(const char* what) {
        if (error_ && error_->empty()) {
            *error_ = std::string(what) + " (offset "
                    + std::to_string(static_cast<std::size_t>(p_ - begin_)) + ")";
        }
        return false;
    }

    void skip_ws() {
        while (p_ != end_ && (*p_ == ' ' || *p_ == '\t' || *p_ == '\n' || *p_ == '\r')) {
            ++p_;
        }
    }

    bool literal(std::string_view word) {
        if (static_cast<std::size_t>(end_ - p_) < word.size()) {
            return false;
        }
        if (std::memcmp(p_, word.data(), word.size()) != 0) {
            return false;
        }
        p_ += word.size();
        return true;
    }

    bool parse_value(Json* out, int depth) {
        if (depth > 64) {
            return fail("嵌套过深");
        }
        if (p_ == end_) {
            return fail("意外的输入结束");
        }
        switch (*p_) {
        case '{':
            return parse_object(out, depth);
        case '[':
            return parse_array(out, depth);
        case '"': {
            std::string s;
            if (!parse_string(&s)) {
                return false;
            }
            *out = Json(std::move(s));
            return true;
        }
        case 't':
            if (!literal("true")) {
                return fail("无效的字面量");
            }
            *out = Json(true);
            return true;
        case 'f':
            if (!literal("false")) {
                return fail("无效的字面量");
            }
            *out = Json(false);
            return true;
        case 'n':
            if (!literal("null")) {
                return fail("无效的字面量");
            }
            *out = Json();
            return true;
        default:
            return parse_number(out);
        }
    }

    bool parse_number(Json* out) {
        char* stop     = nullptr;
        const double v = std::strtod(p_, &stop);
        if (stop == p_) {
            return fail("无效的数字");
        }
        p_   = stop;
        *out = Json(v);
        return true;
    }

    bool parse_hex4(unsigned* out) {
        if (end_ - p_ < 4) {
            return fail("\\u 转义不完整");
        }
        unsigned v = 0;
        for (int i = 0; i < 4; ++i) {
            const char c   = p_[i];
            v            <<= 4;
            if (c >= '0' && c <= '9') {
                v |= static_cast<unsigned>(c - '0');
            } else if (c >= 'a' && c <= 'f') {
                v |= static_cast<unsigned>(c - 'a' + 10);
            } else if (c >= 'A' && c <= 'F') {
                v |= static_cast<unsigned>(c - 'A' + 10);
            } else {
                return fail("无效的 \\u 转义");
            }
        }
        p_   += 4;
        *out  = v;
        return true;
    }

    bool parse_string(std::string* out) {
        if (p_ == end_ || *p_ != '"') {
            return fail("期望字符串");
        }
        ++p_;
        while (p_ != end_) {
            const char c = *p_++;
            if (c == '"') {
                return true;
            }
            if (c != '\\') {
                out->push_back(c);
                continue;
            }
            if (p_ == end_) {
                return fail("字符串转义不完整");
            }
            switch (*p_++) {
            case '"':
                out->push_back('"');
                break;
            case '\\':
                out->push_back('\\');
                break;
            case '/':
                out->push_back('/');
                break;
            case 'b':
                out->push_back('\b');
                break;
            case 'f':
                out->push_back('\f');
                break;
            case 'n':
                out->push_back('\n');
                break;
            case 'r':
                out->push_back('\r');
                break;
            case 't':
                out->push_back('\t');
                break;
            case 'u': {
                unsigned cp = 0;
                if (!parse_hex4(&cp)) {
                    return false;
                }
                if (cp >= 0xD800 && cp <= 0xDBFF) {
                    // 代理对
                    if (end_ - p_ >= 6 && p_[0] == '\\' && p_[1] == 'u') {
                        p_           += 2;
                        unsigned low  = 0;
                        if (!parse_hex4(&low)) {
                            return false;
                        }
                        if (low >= 0xDC00 && low <= 0xDFFF) {
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                        } else {
                            append_utf8(*out, cp);
                            cp = low;
                        }
                    }
                }
                append_utf8(*out, cp);
                break;
            }
            default:
                return fail("未知的转义字符");
            }
        }
        return fail("字符串未闭合");
    }

    bool parse_array(Json* out, int depth) {
        ++p_;  // '['
        *out = Json::array();
        skip_ws();
        if (p_ != end_ && *p_ == ']') {
            ++p_;
            return true;
        }
        while (true) {
            skip_ws();
            Json item;
            if (!parse_value(&item, depth + 1)) {
                return false;
            }
            out->push(std::move(item));
            skip_ws();
            if (p_ == end_) {
                return fail("数组未闭合");
            }
            if (*p_ == ',') {
                ++p_;
                continue;
            }
            if (*p_ == ']') {
                ++p_;
                return true;
            }
            return fail("数组中期望 ',' 或 ']'");
        }
    }

    bool parse_object(Json* out, int depth) {
        ++p_;  // '{'
        *out = Json::object();
        skip_ws();
        if (p_ != end_ && *p_ == '}') {
            ++p_;
            return true;
        }
        while (true) {
            skip_ws();
            std::string key;
            if (!parse_string(&key)) {
                return false;
            }
            skip_ws();
            if (p_ == end_ || *p_ != ':') {
                return fail("期望 ':'");
            }
            ++p_;
            skip_ws();
            Json value;
            if (!parse_value(&value, depth + 1)) {
                return false;
            }
            out->set(std::move(key), std::move(value));
            skip_ws();
            if (p_ == end_) {
                return fail("对象未闭合");
            }
            if (*p_ == ',') {
                ++p_;
                continue;
            }
            if (*p_ == '}') {
                ++p_;
                return true;
            }
            return fail("对象中期望 ',' 或 '}'");
        }
    }

    const char* begin_;
    const char* p_;
    const char* end_;
    std::string* error_;
};

}  // namespace

Json::Json() = default;
Json::Json(std::nullptr_t) {
}
Json::Json(bool v) : type_(Type::Bool), bool_(v) {
}
Json::Json(int v) : type_(Type::Number), num_(static_cast<double>(v)) {
}
Json::Json(unsigned v) : type_(Type::Number), num_(static_cast<double>(v)) {
}
Json::Json(long long v) : type_(Type::Number), num_(static_cast<double>(v)) {
}
Json::Json(double v) : type_(Type::Number), num_(v) {
}
Json::Json(const char* v) : type_(Type::String), str_(v ? v : "") {
}
Json::Json(std::string v) : type_(Type::String), str_(std::move(v)) {
}

Json Json::array() {
    Json j;
    j.type_ = Type::Array;
    return j;
}

Json Json::object() {
    Json j;
    j.type_ = Type::Object;
    return j;
}

const Json& Json::null() {
    return null_singleton();
}

bool Json::as_bool(bool fallback) const {
    return type_ == Type::Bool ? bool_ : fallback;
}

double Json::as_double(double fallback) const {
    return type_ == Type::Number ? num_ : fallback;
}

long long Json::as_int(long long fallback) const {
    return type_ == Type::Number ? static_cast<long long>(num_) : fallback;
}

const std::string& Json::as_string() const {
    static const std::string kEmpty;
    return type_ == Type::String ? str_ : kEmpty;
}

std::string Json::as_string(std::string fallback) const {
    return type_ == Type::String ? str_ : std::move(fallback);
}

bool Json::contains(std::string_view key) const {
    return type_ == Type::Object && obj_.find(key) != obj_.end();
}

const Json& Json::at(std::string_view key) const {
    if (type_ != Type::Object) {
        return null_singleton();
    }
    const auto it = obj_.find(key);
    return it == obj_.end() ? null_singleton() : it->second;
}

Json& Json::operator[](std::string_view key) {
    if (type_ != Type::Object) {
        *this = Json::object();
    }
    return obj_[std::string(key)];
}

const Json& Json::operator[](std::string_view key) const {
    return at(key);
}

void Json::set(std::string key, Json value) {
    if (type_ != Type::Object) {
        *this = Json::object();
    }
    obj_[std::move(key)] = std::move(value);
}

bool Json::erase(std::string_view key) {
    if (type_ != Type::Object) {
        return false;
    }
    return obj_.erase(std::string(key)) > 0;
}

std::size_t Json::size() const {
    if (type_ == Type::Array) {
        return arr_.size();
    }
    if (type_ == Type::Object) {
        return obj_.size();
    }
    return 0;
}

const Json& Json::at_index(std::size_t index) const {
    if (type_ != Type::Array || index >= arr_.size()) {
        return null_singleton();
    }
    return arr_[index];
}

void Json::push(Json value) {
    if (type_ != Type::Array) {
        *this = Json::array();
    }
    arr_.push_back(std::move(value));
}

const Json& Json::get_path(std::string_view dotted_path) const {
    const Json* cur = this;
    while (!dotted_path.empty()) {
        const std::size_t dot       = dotted_path.find('.');
        const std::string_view head = dotted_path.substr(0, dot);
        cur                         = &cur->at(head);
        if (cur->is_null()) {
            return null_singleton();
        }
        if (dot == std::string_view::npos) {
            break;
        }
        dotted_path.remove_prefix(dot + 1);
    }
    return *cur;
}

void Json::merge_patch(const Json& patch) {
    if (!patch.is_object()) {
        *this = patch;
        return;
    }
    if (type_ != Type::Object) {
        *this = Json::object();
    }
    for (const auto& [key, value] : patch.members()) {
        if (value.is_null()) {
            obj_.erase(key);
            continue;
        }
        auto it = obj_.find(key);
        if (it == obj_.end()) {
            obj_.emplace(key, value);
        } else {
            it->second.merge_patch(value);
        }
    }
}

void Json::dump_to(std::string& out, int indent, int depth) const {
    const bool pretty = indent > 0;
    const std::string pad =
        pretty ? std::string(static_cast<std::size_t>(indent * (depth + 1)), ' ') : std::string();
    const std::string pad_end =
        pretty ? std::string(static_cast<std::size_t>(indent * depth), ' ') : std::string();

    switch (type_) {
    case Type::Null:
        out += "null";
        break;
    case Type::Bool:
        out += bool_ ? "true" : "false";
        break;
    case Type::Number:
        append_number(out, num_);
        break;
    case Type::String:
        append_escaped(out, str_);
        break;
    case Type::Array: {
        if (arr_.empty()) {
            out += "[]";
            break;
        }
        out.push_back('[');
        for (std::size_t i = 0; i < arr_.size(); ++i) {
            if (i) {
                out.push_back(',');
            }
            if (pretty) {
                out.push_back('\n');
                out += pad;
            }
            arr_[i].dump_to(out, indent, depth + 1);
        }
        if (pretty) {
            out.push_back('\n');
            out += pad_end;
        }
        out.push_back(']');
        break;
    }
    case Type::Object: {
        if (obj_.empty()) {
            out += "{}";
            break;
        }
        out.push_back('{');
        bool first = true;
        for (const auto& [key, value] : obj_) {
            if (!first) {
                out.push_back(',');
            }
            first = false;
            if (pretty) {
                out.push_back('\n');
                out += pad;
            }
            append_escaped(out, key);
            out.push_back(':');
            if (pretty) {
                out.push_back(' ');
            }
            value.dump_to(out, indent, depth + 1);
        }
        if (pretty) {
            out.push_back('\n');
            out += pad_end;
        }
        out.push_back('}');
        break;
    }
    }
}

std::string Json::dump(int indent) const {
    std::string out;
    out.reserve(256);
    dump_to(out, indent, 0);
    return out;
}

Json Json::parse(std::string_view text, std::string* error) {
    if (error) {
        error->clear();
    }
    Json result;
    Parser parser(text, error);
    if (!parser.parse(&result)) {
        return Json();
    }
    return result;
}

bool Json::operator==(const Json& other) const {
    if (type_ != other.type_) {
        return false;
    }
    switch (type_) {
    case Type::Null:
        return true;
    case Type::Bool:
        return bool_ == other.bool_;
    case Type::Number:
        return num_ == other.num_;
    case Type::String:
        return str_ == other.str_;
    case Type::Array:
        return arr_ == other.arr_;
    case Type::Object:
        return obj_ == other.obj_;
    }
    return false;
}

}  // namespace fl
