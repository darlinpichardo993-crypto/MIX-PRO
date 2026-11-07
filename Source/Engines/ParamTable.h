#pragma once
#include "Roles.h"
#include "Genres.h"
#include <cstring>
namespace mp {
// X(id, name, kind(0 float,1 choice,2 bool), min, max, default, skew, choices)
#define MP_MODES "AUTO MIX|REFERENCE MATCH|ERA MATCH|GENRE MIX|VOCAL MIX|INSTRUMENT MIX|MASTER MATCH|MANUAL ENGINEERING"
#define MP_PARAMS(X) \
 X(mode,"Mode",1,0,7,0,1,MP_MODES) X(role,"Instrument Role",1,0,24,0,1,"") X(genre,"Genre",1,0,33,0,1,"") X(style,"Mix Style",1,0,5,2,1,"") \
 X(eraPos,"Era (01-02-03)",0,0,2,1,1,"") X(amount,"Amount",0,0,1,1,1,"") X(inGain,"Input Gain",0,-24,24,0,1,"") X(outGain,"Output Gain",0,-24,24,0,1,"") X(bypass,"Bypass",2,0,1,0,1,"") \
 X(matchAmount,"Match Amount",0,0,1,0.7f,1,"") X(matchRef,"Reference Weight",0,0,1,1,1,"") X(matchFreeze,"Freeze Match",2,0,1,0,1,"") \
 X(body,"Body",0,-1,1,0,1,"") X(presence,"Presence",0,-1,1,0,1,"") X(attack,"Pick Attack",0,-1,1,0,1,"") X(sustain,"Sustain",0,-1,1,0,1,"") \
 X(character,"Character",0,0,1,0,1,"") X(dynamics,"Dynamic Control",0,-1,1,0,1,"") X(ambience,"Ambience",0,0,1,0,1,"") X(position,"Stereo Position",0,-1,1,0,1,"") X(width,"Width",0,0,2,1,1,"") \
 X(manHpf,"Manual HPF",0,20,400,20,0.4f,"") X(manLow,"EQ Low",0,-12,12,0,1,"") X(manLowMid,"EQ Low-Mid",0,-12,12,0,1,"") X(manHighMid,"EQ High-Mid",0,-12,12,0,1,"") X(manHigh,"EQ High",0,-12,12,0,1,"") \
 X(manThr,"Comp Threshold",0,-48,0,-18,1,"") X(manRatio,"Comp Ratio",0,1,20,2,0.5f,"") X(manAtt,"Comp Attack",0,0.1f,200,15,0.4f,"") X(manRel,"Comp Release",0,10,1000,120,0.4f,"") X(manMakeup,"Comp Makeup",0,0,18,0,1,"") \
 X(deEss,"De-Ess",0,0,1,0,1,"") X(deEssFreq,"De-Ess Freq",0,3000,12000,6500,0.6f,"") \
 X(reverbSize,"Reverb Size",0,0,1,0.5f,1,"") X(reverbDamp,"Reverb Damping",0,0,1,0.5f,1,"") X(preDelay,"Pre-Delay",0,0,100,10,1,"") \
 X(delayMs,"Delay Time",0,20,1500,250,0.5f,"") X(delayFb,"Delay Feedback",0,0,0.9f,0.3f,1,"") X(delaySync,"Delay Host Sync",2,0,1,0,1,"") X(delayDiv,"Delay Division",1,0,4,1,1,"1/4|1/8|1/8 dotted|1/16|1/2") \
 X(mTargetLufs,"Target LUFS",0,-24,-4,-14,1,"") X(mCeiling,"Ceiling dBTP",0,-3,0,-1,1,"") X(mMb,"Multiband",0,0,1,0.5f,1,"") X(mGlue,"Glue Comp",0,0,1,0.3f,1,"") \
 X(mMonoLow,"Mono Low Freq",0,0,300,0,1,"") X(mTilt,"Tilt",0,-4,4,0,1,"") X(mMatchLoud,"Match Loudness",2,0,1,0,1,"") \
 X(exportBits,"Export Bit Depth",1,0,2,1,1,"16-bit|24-bit|32-bit float") X(exportSr,"Export Sample Rate",1,0,4,0,1,"Same as source|44100|48000|88200|96000") X(exportMono,"Export Mono",2,0,1,0,1,"")

enum ParamIndex {
#define X(id, ...) p_##id,
    MP_PARAMS(X)
#undef X
    kNumParams };

struct ParamInfo { const char* id; const char* name; int kind; float mn, mx, def, skew; const char* choices; };
inline const char* choicesFor(const char* id, const char* given) {
    if (!std::strcmp(id, "role")) return roleNames();
    if (!std::strcmp(id, "genre")) return GenreLibrary::names();
    if (!std::strcmp(id, "style")) return styleNames();
    return given;
}
inline const ParamInfo* paramTable() {
    static const ParamInfo t[kNumParams] = {
#define X(id, name, kind, mn, mx, def, skew, ch) { #id, name, kind, (float)(mn), (float)(mx), (float)(def), (float)(skew), choicesFor(#id, ch) },
        MP_PARAMS(X)
#undef X
    };
    return t;
}
inline int findParam(const std::string& id) { for (int i = 0; i < kNumParams; ++i) if (id == paramTable()[i].id) return i; return -1; }
inline void defaultValues(float* v) { for (int i = 0; i < kNumParams; ++i) v[i] = paramTable()[i].def; }

struct ParamSet {
    int mode = 0, role = 0, genre = 0, style = 2; float eraPos = 1.f, amount = 1.f, inGain = 0, outGain = 0; bool bypass = false;
    float matchAmount = 0.7f, matchRef = 1.f; bool matchFreeze = false;
    float body = 0, presence = 0, attack = 0, sustain = 0, character = 0, dynamics = 0, ambience = 0, position = 0, width = 1;
    float manHpf = 20, manLow = 0, manLowMid = 0, manHighMid = 0, manHigh = 0, manThr = -18, manRatio = 2, manAtt = 15, manRel = 120, manMakeup = 0;
    float deEss = 0, deEssFreq = 6500, reverbSize = 0.5f, reverbDamp = 0.5f, preDelay = 10, delayMs = 250, delayFb = 0.3f; bool delaySync = false; int delayDiv = 1;
    float mTargetLufs = -14, mCeiling = -1, mMb = 0.5f, mGlue = 0.3f, mMonoLow = 0, mTilt = 0; bool mMatchLoud = false;
    int exportBits = 1, exportSr = 0; bool exportMono = false;
};
inline void fillParamSet(const float* v, ParamSet& P) {
#define I(x) P.x = (int)std::lround(v[p_##x]);
#define F(x) P.x = v[p_##x];
#define B(x) P.x = v[p_##x] > 0.5f;
    I(mode) I(role) I(genre) I(style) F(eraPos) F(amount) F(inGain) F(outGain) B(bypass) F(matchAmount) F(matchRef) B(matchFreeze)
    F(body) F(presence) F(attack) F(sustain) F(character) F(dynamics) F(ambience) F(position) F(width)
    F(manHpf) F(manLow) F(manLowMid) F(manHighMid) F(manHigh) F(manThr) F(manRatio) F(manAtt) F(manRel) F(manMakeup)
    F(deEss) F(deEssFreq) F(reverbSize) F(reverbDamp) F(preDelay) F(delayMs) F(delayFb) B(delaySync) I(delayDiv)
    F(mTargetLufs) F(mCeiling) F(mMb) F(mGlue) F(mMonoLow) F(mTilt) B(mMatchLoud) I(exportBits) I(exportSr) B(exportMono)
#undef I
#undef F
#undef B
}

// ---- Presets (JSON, shared between instances/sessions) ----
struct PresetData {
    std::string name, category; std::vector<std::pair<std::string, float>> values;
    void apply(float* v) const { for (auto& kv : values) { const int i = findParam(kv.first); if (i >= 0) v[i] = clampT(kv.second, paramTable()[i].mn, paramTable()[i].mx); } }
    std::string toJson() const {
        std::string j = "{\n  \"format\": \"MIXPRO-PRESET\",\n  \"name\": \"" + jsonEscape(name) + "\",\n  \"category\": \"" + jsonEscape(category) + "\"";
        for (auto& kv : values) j += ",\n  \"" + kv.first + "\": " + jsonNum(kv.second); return j + "\n}\n";
    }
    static bool fromJson(const std::string& s, PresetData& p) {
        JsonObj o; if (!parseJson(s, o) || o.str("format") != "MIXPRO-PRESET") return false;
        p = PresetData(); p.name = o.str("name"); p.category = o.str("category");
        for (auto& kv : o.nums) if (findParam(kv.first) >= 0 && !kv.second.empty()) p.values.push_back({kv.first, (float)kv.second[0]}); return true;
    }
    static PresetData fromValues(const std::string& n, const std::string& cat, const float* v) {
        PresetData p; p.name = n; p.category = cat; for (int i = 0; i < kNumParams; ++i) p.values.push_back({paramTable()[i].id, v[i]}); return p;
    }
};
}
