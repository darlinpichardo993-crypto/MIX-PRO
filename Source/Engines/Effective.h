#pragma once
#include "ParamTable.h"
namespace mp {
#define MP_EFF_FIELDS(X) X(inGain) X(outGain) X(hpf) X(lsF) X(lsG) X(p1F) X(p1G) X(p1Q) X(p2F) X(p2G) X(p2Q) X(hsF) X(hsG) \
 X(thr) X(ratio) X(att) X(rel) X(knee) X(makeup) X(sat) X(tAtk) X(tSus) X(deEss) X(deEssF) X(width) X(balance) X(monoLow) \
 X(revMix) X(revSize) X(revDamp) X(preDelay) X(dlyMix) X(dlyMs) X(dlyFb) X(matchLo) X(matchHi) X(matchAmt) X(mbAmt) X(glue) X(ceiling) X(targetLufs)
enum MatchKind { kMatchNone = 0, kMatchBump = 1, kMatchCurve = 2 };
struct Effective {
#define X(f) float f = 0;
    MP_EFF_FIELDS(X)
#undef X
    bool master = false, matchLoud = false; int kind = kMatchNone; int src = 0; /* src: 1=reference, 2=era (curve targets) */ int role = 0;
};
inline void lerpEff(Effective& c, const Effective& t, float k) {
#define X(f) c.f += (t.f - c.f) * k;
    MP_EFF_FIELDS(X)
#undef X
    c.master = t.master; c.matchLoud = t.matchLoud; c.kind = t.kind; c.src = t.src; c.role = t.role;
}
inline void computeEffective(const ParamSet& P, double bpm, Effective& E) {
    const bool master = P.mode == kModeMaster, manual = P.mode == kModeManual;
    int role = clampT(P.role, 0, kNumRoles - 1);
    if (P.mode == kModeVocal && !isVocalRole(role)) role = LeadVocal;
    if (P.mode == kModeInstrument && isVocalRole(role)) role = RhythmGuitar;
    const RoleBase B = (master || manual) ? neutralBase() : roleTable()[role];
    const StyleScale S = manual ? StyleScale{1, 1, 1, 0} : styleScale(P.style);
    float lsG = B.lsG * S.eq, p1G = B.p1G * S.eq, p2G = B.p2G * S.eq, hsG = B.hsG * S.eq + S.highDb;
    float ratio = 1.f + (B.ratio - 1.f) * S.comp, thr = B.thr, sat = B.sat * S.sat, revMix = B.revMix, dlyMix = B.dlyMix, width = B.width;
    if (P.mode == kModeGenre || P.mode == kModeVocal || P.mode == kModeInstrument) {
        const auto& g = GenreLibrary::instance().get(P.genre);
        lsG += g.low; hsG += g.high; p2G += g.pres; ratio = 1.f + (ratio - 1.f) * g.comp; sat *= g.sat; revMix *= g.rev; width *= g.width;
    }
    // macros
    lsG += P.body * 4.f; p2G += P.presence * 4.f; hsG += P.presence * 1.5f; sat += P.character * 0.8f;
    thr -= P.dynamics * 8.f; ratio = std::max(1.f, ratio * (1.f + 0.4f * P.dynamics));
    revMix += P.ambience * 0.25f; dlyMix += P.ambience * 0.15f;
    // manual EQ offsets (all modes) and master tilt
    lsG += P.manLow - (master ? P.mTilt * 0.5f : 0.f); p1G += P.manLowMid; p2G += P.manHighMid; hsG += P.manHigh + (master ? P.mTilt * 0.5f : 0.f);
    E.inGain = P.inGain; E.outGain = P.outGain;
    E.hpf = manual ? P.manHpf : B.hpf;
    E.lsF = B.lsF; E.lsG = clampT(lsG, -15.f, 15.f); E.p1F = B.p1F; E.p1G = clampT(p1G, -15.f, 15.f); E.p1Q = B.p1Q;
    E.p2F = B.p2F; E.p2G = clampT(p2G, -15.f, 15.f); E.p2Q = B.p2Q; E.hsF = B.hsF; E.hsG = clampT(hsG, -15.f, 15.f);
    if (manual) { E.thr = P.manThr; E.ratio = P.manRatio; E.att = P.manAtt; E.rel = P.manRel; E.makeup = P.manMakeup; }
    else { E.thr = thr; E.ratio = ratio; E.att = B.att; E.rel = B.rel; E.makeup = (ratio > 1.01f ? B.makeup : 0.f); }
    E.knee = 6.f; E.sat = clampT(sat, 0.f, 1.f);
    E.tAtk = clampT(B.tAtk + P.attack, -1.f, 1.f); E.tSus = clampT(B.tSus + P.sustain, -1.f, 1.f);
    E.deEss = (P.mode == kModeInstrument && !(role == Guira)) ? 0.f : clampT(B.deEss + P.deEss, 0.f, 1.f); E.deEssF = role == Guira ? 6000.f : P.deEssFreq;
    E.width = clampT(width * P.width, 0.f, 2.f); E.balance = P.position; E.monoLow = master ? P.mMonoLow : B.monoLow;
    E.revMix = clampT(revMix, 0.f, 0.6f); E.revSize = P.reverbSize; E.revDamp = P.reverbDamp; E.preDelay = P.preDelay;
    static const float div[5] = {1.f, 0.5f, 0.75f, 0.25f, 2.f};
    E.dlyMs = (P.delaySync && bpm > 20.0) ? (float)(60000.0 / bpm * div[clampT(P.delayDiv, 0, 4)]) : P.delayMs; E.dlyFb = P.delayFb; E.dlyMix = clampT(dlyMix, 0.f, 0.5f);
    E.matchLo = B.matchLo; E.matchHi = B.matchHi; E.matchAmt = P.matchAmount;
    E.mbAmt = master ? P.mMb : 0.f; E.glue = master ? P.mGlue : 0.f; E.ceiling = P.mCeiling; E.targetLufs = P.mTargetLufs;
    E.master = master; E.matchLoud = master && P.mMatchLoud; E.role = role; E.src = 0;
    switch (P.mode) {
    case kModeAuto: case kModeVocal: case kModeInstrument: E.kind = kMatchBump; break;
    case kModeReference: E.kind = kMatchCurve; E.src = 1; break;
    case kModeEra: E.kind = kMatchCurve; E.src = 2; break;
    case kModeMaster: E.kind = kMatchCurve; E.src = 1; break;
    default: E.kind = kMatchNone; break;
    }
}
}
