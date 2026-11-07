#pragma once
#include <map>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <cmath>
namespace mp {
// Minimal JSON subset: flat object, values = number | string | array of numbers
struct JsonObj {
    std::map<std::string, std::vector<double>> nums;
    std::map<std::string, std::string> strs;
    double num(const std::string& k, double d = 0) const { auto it = nums.find(k); return (it != nums.end() && !it->second.empty()) ? it->second[0] : d; }
    bool has(const std::string& k) const { return nums.count(k) || strs.count(k); }
    std::string str(const std::string& k, const std::string& d = "") const { auto it = strs.find(k); return it != strs.end() ? it->second : d; }
};
inline std::string jsonEscape(const std::string& s) {
    std::string o; for (char c : s) { if (c == '"' || c == '\\') { o += '\\'; o += c; } else if (c == '\n') o += "\\n"; else if ((unsigned char)c >= 32) o += c; } return o;
}
inline std::string jsonNum(double v) { if (!std::isfinite(v)) v = 0; char b[48]; std::snprintf(b, sizeof b, "%.9g", v); return b; }
inline bool parseJson(const std::string& s, JsonObj& out) {
    size_t i = 0; out = JsonObj();
    auto ws = [&] { while (i < s.size() && std::isspace((unsigned char)s[i])) ++i; };
    auto readStr = [&](std::string& r) -> bool {
        if (i >= s.size() || s[i] != '"') return false; ++i; r.clear();
        while (i < s.size() && s[i] != '"') { if (s[i] == '\\' && i + 1 < s.size()) { ++i; r += (s[i] == 'n' ? '\n' : s[i]); } else r += s[i]; ++i; }
        if (i >= s.size()) return false; ++i; return true; };
    auto readNum = [&](double& v) -> bool { const char* b = s.c_str() + i; char* e = nullptr; v = std::strtod(b, &e); if (e == b) return false; i += (size_t)(e - b); return true; };
    ws(); if (i >= s.size() || s[i] != '{') return false; ++i; ws();
    if (i < s.size() && s[i] == '}') return true;
    while (i < s.size()) {
        ws(); std::string key; if (!readStr(key)) return false; ws(); if (i >= s.size() || s[i] != ':') return false; ++i; ws();
        if (i < s.size() && s[i] == '"') { std::string v; if (!readStr(v)) return false; out.strs[key] = v; }
        else if (i < s.size() && s[i] == '[') {
            ++i; std::vector<double> arr; ws();
            while (i < s.size() && s[i] != ']') { double v; ws(); if (!readNum(v)) return false; arr.push_back(v); ws(); if (i < s.size() && s[i] == ',') ++i; ws(); }
            if (i >= s.size()) return false; ++i; out.nums[key] = arr;
        } else { double v; if (!readNum(v)) return false; out.nums[key] = { v }; }
        ws(); if (i < s.size() && s[i] == ',') { ++i; continue; }
        if (i < s.size() && s[i] == '}') return true; return false;
    }
    return false;
}
}
