#pragma once
#include "../DSP/Common.h"
#include "../Analysis/MiniJson.h"
#include <string>
namespace mp {
struct GenreProfile { std::string name, category; float low, high, pres, comp, sat, rev, width; };
constexpr int kNumGenres = 34;
class GenreLibrary {
public:
    static GenreLibrary& instance() { static GenreLibrary g; return g; }
    const GenreProfile& get(int i) const { return g_[clampT(i, 0, kNumGenres - 1)]; }
    GenreProfile& edit(int i) { return g_[clampT(i, 0, kNumGenres - 1)]; }
    void resetDefaults() { g_ = defaults(); }
    static const char* names() {
        return "Bachata|Merengue|Salsa|Bolero|Son|Reggaeton|Dembow|Latin Pop|Latin R&B|Latin Rock|Trap|Hip-Hop|Drill|R&B|Dancehall|Afrobeat|Afrobeats|Reggae|Pop|Rock|Indie|Alternative|Soul|Gospel|Funk|Jazz|Blues|Country|EDM|House|Techno|HyperPop|K-Pop|Orchestral";
    }
    std::string toJson() const {
        std::string j = "{\n"; for (int i = 0; i < kNumGenres; ++i) { const auto& p = g_[i];
            j += "  \"" + p.name + "\": [" + jsonNum(p.low) + ", " + jsonNum(p.high) + ", " + jsonNum(p.pres) + ", " + jsonNum(p.comp) + ", " + jsonNum(p.sat) + ", " + jsonNum(p.rev) + ", " + jsonNum(p.width) + "]" + (i + 1 < kNumGenres ? ",\n" : "\n"); }
        return j + "}\n";
    }
    bool fromJson(const std::string& s) {
        JsonObj o; if (!parseJson(s, o)) return false; bool any = false;
        for (auto& p : g_) { auto it = o.nums.find(p.name); if (it != o.nums.end() && it->second.size() == 7) { auto& v = it->second;
            p.low = (float)v[0]; p.high = (float)v[1]; p.pres = (float)v[2]; p.comp = (float)v[3]; p.sat = (float)v[4]; p.rev = (float)v[5]; p.width = (float)v[6]; any = true; } }
        return any;
    }
private:
    GenreLibrary() { g_ = defaults(); }
    static std::vector<GenreProfile> defaults() {
        const char* L = "LATIN"; const char* U = "URBAN"; const char* I = "INTERNATIONAL";
        return {
         {"Bachata",L,0.5f,1.0f,0.5f,1.0f,1.0f,1.0f,1.0f},{"Merengue",L,1.0f,1.0f,0.5f,1.2f,1.0f,0.8f,1.0f},{"Salsa",L,0.5f,0.5f,0.5f,1.0f,0.8f,0.9f,1.0f},
         {"Bolero",L,0.0f,-0.5f,0.0f,0.7f,0.6f,1.3f,0.9f},{"Son",L,0.0f,0.0f,0.5f,0.8f,0.7f,1.0f,1.0f},{"Reggaeton",L,2.0f,1.0f,0.0f,1.3f,1.1f,0.8f,0.9f},
         {"Dembow",L,2.0f,1.0f,0.0f,1.4f,1.2f,0.7f,0.9f},{"Latin Pop",L,1.0f,1.5f,1.0f,1.2f,0.8f,1.0f,1.1f},{"Latin R&B",L,1.5f,1.0f,0.5f,1.0f,0.8f,1.2f,1.1f},
         {"Latin Rock",L,1.0f,1.0f,1.0f,1.3f,1.2f,0.9f,1.1f},
         {"Trap",U,2.5f,1.5f,0.0f,1.4f,1.2f,0.8f,1.0f},{"Hip-Hop",U,2.0f,1.0f,0.5f,1.3f,1.0f,0.8f,0.9f},{"Drill",U,2.0f,1.0f,0.0f,1.4f,1.2f,0.7f,0.9f},
         {"R&B",U,1.5f,1.0f,0.5f,1.0f,0.8f,1.2f,1.1f},{"Dancehall",U,2.0f,1.0f,0.5f,1.2f,1.0f,0.9f,1.0f},{"Afrobeat",U,1.0f,0.5f,0.5f,1.0f,0.9f,0.9f,1.1f},
         {"Afrobeats",U,1.5f,1.0f,0.5f,1.1f,0.9f,0.9f,1.1f},{"Reggae",U,1.5f,0.0f,0.0f,0.9f,0.8f,1.1f,1.0f},
         {"Pop",I,1.0f,1.5f,1.0f,1.2f,0.8f,1.0f,1.2f},{"Rock",I,1.0f,1.0f,1.0f,1.3f,1.3f,0.8f,1.1f},{"Indie",I,0.5f,0.5f,0.5f,0.9f,1.0f,1.1f,1.2f},
         {"Alternative",I,1.0f,1.0f,0.5f,1.1f,1.2f,1.0f,1.2f},{"Soul",I,1.0f,0.5f,0.5f,0.9f,1.0f,1.2f,1.1f},{"Gospel",I,0.5f,1.0f,1.0f,1.0f,0.7f,1.3f,1.2f},
         {"Funk",I,1.0f,1.0f,0.5f,1.1f,1.0f,0.7f,1.0f},{"Jazz",I,0.0f,0.0f,0.0f,0.6f,0.5f,1.1f,1.1f},{"Blues",I,0.5f,0.0f,0.5f,0.8f,1.2f,1.0f,1.0f},
         {"Country",I,0.0f,1.0f,1.0f,1.0f,0.7f,1.0f,1.1f},{"EDM",I,2.0f,2.0f,0.0f,1.5f,1.0f,0.8f,1.3f},{"House",I,2.0f,1.5f,0.0f,1.4f,0.9f,0.8f,1.2f},
         {"Techno",I,2.0f,1.0f,0.0f,1.4f,1.1f,0.7f,1.2f},{"HyperPop",I,1.5f,2.5f,1.0f,1.6f,1.5f,0.8f,1.4f},{"K-Pop",I,1.5f,2.0f,1.0f,1.4f,0.9f,1.0f,1.3f},
         {"Orchestral",I,0.0f,0.5f,0.0f,0.5f,0.3f,1.5f,1.4f} };
    }
    std::vector<GenreProfile> g_;
};

// ERA starting-point shapes (heuristic tilt/body/presence/air curves; NOT measurements of any recording or studio)
inline float eraBell(float x, float c, float w) { const float d = (x - c) / w; return std::exp(-d * d); }
inline void eraShape(int era, float* out16, double centers[16]) {
    static const float tilt[3] = {-2.0f, -1.5f, -1.0f}, body[3] = {1.5f, 0.8f, 0.5f}, pres[3] = {0.0f, 1.5f, 2.0f}, air[3] = {-1.0f, 0.0f, 1.0f};
    for (int b = 0; b < 16; ++b) { const float o = (float)std::log2(centers[b] / 1000.0);
        out16[b] = tilt[era] * o + body[era] * eraBell(o, (float)std::log2(0.15), 1.0f) + pres[era] * eraBell(o, (float)std::log2(3.0), 0.8f) + air[era] * eraBell(o, (float)std::log2(10.0), 0.8f); }
}
}
