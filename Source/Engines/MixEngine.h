#pragma once
#include <atomic>
#include <cstring>
#include "../DSP/Dynamics.h"
#include "../DSP/Effects.h"
#include "../Analysis/Spectrum.h"
#include "../Analysis/Loudness.h"
#include "Effective.h"
#include "MatchEngine.h"
namespace mp {

struct Meters {
    static constexpr int kSpec = 64, kScope = 256;
    std::atomic<float> inPeakDb{-120.f}, outPeakDb{-120.f}, compGrDb{0.f}, limGrDb{0.f}, momentary{-120.f}, shortTerm{-120.f}, integrated{-120.f},
        truePeakDb{-120.f}, correlation{0.f}, loudGainDb{0.f};
    std::atomic<float> specIn[kSpec], specOut[kSpec], eq[11], match[kBands], scopeL[kScope], scopeR[kScope];
    std::atomic<int> scopePos{0}, activeRole{0};
    Meters() { for (auto& a : specIn) a = -120.f; for (auto& a : specOut) a = -120.f; for (auto& a : eq) a = 0.f; for (auto& a : match) a = 0.f; for (auto& a : scopeL) a = 0.f; for (auto& a : scopeR) a = 0.f; }
};

class MixEngine {
public:
    static constexpr int kChunk = 256;
    Meters meters;

    void prepare(double sr) {
        sr_ = sr; for (int b = 0; b < kBands; ++b) centers_[b] = bandCenter(b);
        match_.prepare(sr); comp_.prepare(sr); glue_.prepare(sr); mb_.prepare(sr); trans_.prepare(sr); deess_.prepare(sr);
        delay_.prepare(sr); reverb_.prepare(sr); stereo_.prepare(sr); limiter_.prepare(sr);
        inSpec_.prepare(sr, 4096, 2.0); outSpec_.prepare(sr, 4096, 0.25);
        preMeter_.prepare(sr); postMeter_.prepare(sr); reset();
    }
    void reset() {
        init_ = false; for (int c = 0; c < 2; ++c) { hpf_[c].reset(); ls_[c].reset(); p1_[c].reset(); p2_[c].reset(); hs_[c].reset(); }
        match_.prepare(sr_); comp_.reset(); glue_.reset(); mb_.reset(); trans_.reset(); deess_.reset(); sat_.reset(); delay_.reset(); reverb_.reset(); stereo_.reset(); limiter_.reset();
        inSpec_.reset(); outSpec_.reset(); preMeter_.reset(); postMeter_.reset(); tpL_.reset(); tpR_.reset();
        inG_ = outG_ = 1.f; wet_ = 1.f; loudDb_ = 0.f; tpHold_ = 0.f; inLvl_ = 0.f; chunkCount_ = 0; corrLL_ = corrRR_ = corrLR_ = 0;
    }
    int latencySamples(const ParamSet& P) const { return P.mode == kModeMaster ? limiter_.latency() : 0; }
    // reference shape (normalised band dB) and loudness; called from audio thread or before processing
    void setReference(const float* bandsNormDb, float lufs, bool valid) {
        refValid_ = valid; refLufs_ = lufs; if (valid && bandsNormDb) std::memcpy(refNorm_, bandsNormDb, sizeof refNorm_);
    }
    float referenceLufs() const { return refLufs_; }

    void process(float* l, float* r, int n, const ParamSet& P, double bpm) {
        for (int pos = 0; pos < n;) { const int c = std::min(kChunk, n - pos); chunk(l + pos, r ? r + pos : nullptr, c, P, bpm); pos += c; }
    }

private:
    void ramp(float* l, float* r, int n, float g0, float g1) {
        for (int i = 0; i < n; ++i) { const float g = g0 + (g1 - g0) * (float)(i + 1) / n; l[i] *= g; if (r) r[i] *= g; }
    }
    void chunk(float* l, float* r, int n, const ParamSet& P, double bpm) {
        Effective t; computeEffective(P, bpm, t);
        if (!init_) { cur_ = t; init_ = true; wet_ = (P.bypass ? 0.f : (t.master ? 1.f : P.amount)); inG_ = dbToGain(t.inGain); outG_ = dbToGain(t.outGain); }
        else lerpEff(cur_, t, 1.f - std::exp(-(float)n / (float)(sr_ * 0.04)));
        const Effective& E = cur_; const bool st = r != nullptr;
        std::memcpy(dryL_, l, sizeof(float) * n); if (st) std::memcpy(dryR_, r, sizeof(float) * n);

        const float ig = dbToGain(E.inGain); ramp(l, r, n, inG_, ig); inG_ = ig;
        float pk = 0, e = 0; for (int i = 0; i < n; ++i) { pk = std::max(pk, std::fabs(l[i])); e += l[i] * l[i]; if (st) { pk = std::max(pk, std::fabs(r[i])); e += r[i] * r[i]; } }
        const float decay = std::exp(-(float)n / (float)(sr_ * 0.3));
        meters.inPeakDb = std::max(gainToDb(pk), meters.inPeakDb.load() - 20.f * (1.f - decay) * 3.f);
        inLvl_ += (10.f * std::log10(e / (n * (st ? 2 : 1)) + 1e-12f) - inLvl_) * 0.1f;
        for (int i = 0; i < n; ++i) mono_[i] = st ? 0.5f * (l[i] + r[i]) : l[i];
        inSpec_.push(mono_, n);

        // ---- adaptive spectral matching ----
        float wts[kBands], inN[kBands], tgt[kBands] = {0};
        bandWeights(E.matchLo, E.matchHi, wts); inSpec_.bandsNorm(inN, wts); normalizeW(inN, wts);
        int kind = E.kind; bool haveTarget = true;
        if (kind == kMatchCurve) {
            if (E.src == 1) { if (refValid_) std::memcpy(tgt, refNorm_, sizeof tgt); else if (E.master) kind = kMatchNone; else kind = kMatchBump; }
            else {
                eraShape((int)clampT(std::floor(P.eraPos), 0.f, 1.f), eraA_, centers_); eraShape((int)clampT(std::floor(P.eraPos) + 1.f, 1.f, 2.f), eraB_, centers_);
                const float f = clampT(P.eraPos - std::floor(clampT(P.eraPos, 0.f, 1.999f)), 0.f, 1.f), fr = P.eraPos >= 2.f ? 1.f : f;
                for (int b = 0; b < kBands; ++b) tgt[b] = eraA_[b] * (1 - fr) + eraB_[b] * fr;
                normalizeW(tgt, wts);
                if (refValid_ && P.matchRef > 0.f) { float rn[kBands]; std::memcpy(rn, refNorm_, sizeof rn); normalizeW(rn, wts); for (int b = 0; b < kBands; ++b) tgt[b] = tgt[b] * (1 - P.matchRef) + rn[b] * P.matchRef; }
            }
            if (kind == kMatchCurve) normalizeW(tgt, wts);
        }
        (void)haveTarget;
        const bool ready = inSpec_.frames() >= 12 && inLvl_ > -65.f;
        const float amt = (kind == kMatchCurve && E.src == 2 && !(refValid_ && P.matchRef > 0.f)) ? E.matchAmt * 0.6f : E.matchAmt;
        match_.update(inN, tgt, kind, amt, wts, n, P.matchFreeze, ready);

        // ---- processing chain ----
        if (E.hpf > 21.f) { const auto c = makeBiquad(FilterType::HighPass, sr_, E.hpf, 0.7071); hpf_[0].set(c); hpf_[1].set(c);
            for (int i = 0; i < n; ++i) l[i] = hpf_[0].process(l[i]); if (st) for (int i = 0; i < n; ++i) r[i] = hpf_[1].process(r[i]); }
        auto eqBand = [&](Biquad* b, FilterType ty, float f, float q, float g) {
            if (std::fabs(g) < 0.02f) return; const auto c = makeBiquad(ty, sr_, f, q, g); b[0].set(c); b[1].set(c);
            for (int i = 0; i < n; ++i) l[i] = b[0].process(l[i]); if (st) for (int i = 0; i < n; ++i) r[i] = b[1].process(r[i]); };
        eqBand(ls_, FilterType::LowShelf, E.lsF, 0.7071f, E.lsG); eqBand(p1_, FilterType::Peak, E.p1F, E.p1Q, E.p1G);
        eqBand(p2_, FilterType::Peak, E.p2F, E.p2Q, E.p2G); eqBand(hs_, FilterType::HighShelf, E.hsF, 0.7071f, E.hsG);
        match_.process(l, r, n);

        float gr = 0;
        if (E.ratio > 1.01f || E.makeup != 0.f) { comp_.set(E.thr, E.ratio, E.att, E.rel, E.knee, E.makeup); gr = comp_.process(l, r, n); }
        if (E.mbAmt > 0.001f) gr = std::max(gr, mb_.process(l, r, n, E.mbAmt));
        if (E.glue > 0.001f) { glue_.set(-14.f - 6.f * E.glue, 1.2f + 1.8f * E.glue, 30.f, 200.f, 6.f, 0.f); gr = std::max(gr, glue_.process(l, r, n)); }
        trans_.process(l, r, n, E.tAtk, E.tSus);
        sat_.process(l, r, n, E.sat);
        if (E.deEss > 0.001f) { deess_.set(E.deEssF); deess_.process(l, r, n, E.deEss); }
        stereo_.process(l, r, n, E.width, E.balance, E.monoLow);
        delay_.process(l, r, n, E.dlyMs, E.dlyFb, E.dlyMix);
        reverb_.process(l, r, n, E.revSize, E.revDamp, E.preDelay, E.revMix);

        // ---- loudness match + output stage ----
        preMeter_.process(l, r, n);
        float lg0 = loudDb_;
        if (E.matchLoud) { const float stl = preMeter_.shortTerm();
            if (stl > -60.f) { const float want = clampT(E.targetLufs - stl, -12.f, 18.f); loudDb_ += clampT(want - loudDb_, -3.f * n / (float)sr_, 3.f * n / (float)sr_); } }
        else loudDb_ += clampT(0.f - loudDb_, -6.f * n / (float)sr_, 6.f * n / (float)sr_);
        const float og = dbToGain(E.outGain);
        ramp(l, r, n, dbToGain(lg0) * outG_, dbToGain(loudDb_) * og); outG_ = og;
        float limGr = 0;
        if (E.master) { limiter_.setCeilingDb(E.ceiling); limGr = -gainToDb(limiter_.process(l, r, n)); }

        // ---- sanitise, dry/wet ----
        const float wt = P.bypass ? 0.f : (E.master ? 1.f : P.amount);
        for (int i = 0; i < n; ++i) {
            if (!std::isfinite(l[i])) l[i] = 0.f; if (st && !std::isfinite(r[i])) r[i] = 0.f;
            const float w = wet_ + (wt - wet_) * (float)(i + 1) / n;
            l[i] = dryL_[i] * (1.f - w) + l[i] * w; if (st) r[i] = dryR_[i] * (1.f - w) + r[i] * w;
        }
        wet_ = wt;

        // ---- metering ----
        postMeter_.process(l, r, n);
        float opk = 0; double sll = 0, srr = 0, slr = 0;
        for (int i = 0; i < n; ++i) { const float a = l[i], b = st ? r[i] : l[i]; opk = std::max(opk, std::max(std::fabs(a), std::fabs(b))); sll += a * a; srr += b * b; slr += a * b; }
        for (int i = 0; i < n; ++i) mono_[i] = st ? 0.5f * (l[i] + r[i]) : l[i];
        outSpec_.push(mono_, n);
        float tp = tpL_.process(l, n); if (st) tp = std::max(tp, tpR_.process(r, n));
        tpHold_ = std::max(tp, tpHold_ * std::exp(-(float)n / (float)(sr_ * 3.0)));
        meters.outPeakDb = std::max(gainToDb(opk), meters.outPeakDb.load() - 20.f * (1.f - decay) * 3.f);
        meters.truePeakDb = gainToDb(tpHold_); meters.compGrDb = gr; meters.limGrDb = limGr;
        meters.momentary = postMeter_.momentary(); meters.shortTerm = postMeter_.shortTerm(); meters.integrated = postMeter_.integrated(); meters.loudGainDb = loudDb_;
        const float cd = std::exp(-(float)n / (float)(sr_ * 0.5)); corrLL_ = corrLL_ * cd + (float)sll; corrRR_ = corrRR_ * cd + (float)srr; corrLR_ = corrLR_ * cd + (float)slr;
        meters.correlation = st ? (float)(corrLR_ / std::sqrt(std::max(corrLL_ * corrRR_, 1e-20f))) : 1.f;
        for (int k = 0; k < 8; ++k) { const int idx = (meters.scopePos.load() + k) % Meters::kScope, s = k * n / 8; meters.scopeL[idx] = l[s]; meters.scopeR[idx] = st ? r[s] : l[s]; }
        meters.scopePos = (meters.scopePos.load() + 8) % Meters::kScope;
        if ((++chunkCount_ & 3) == 0) {
            float d[Meters::kSpec]; inSpec_.displayDb(d, Meters::kSpec); for (int i = 0; i < Meters::kSpec; ++i) meters.specIn[i] = d[i];
            outSpec_.displayDb(d, Meters::kSpec); for (int i = 0; i < Meters::kSpec; ++i) meters.specOut[i] = d[i];
            const float eqv[11] = {E.lsF, E.lsG, E.p1F, E.p1G, E.p1Q, E.p2F, E.p2G, E.p2Q, E.hsF, E.hsG, E.hpf}; for (int i = 0; i < 11; ++i) meters.eq[i] = eqv[i];
            const float* mg = match_.gains(); for (int b = 0; b < kBands; ++b) meters.match[b] = mg[b]; meters.activeRole = E.role;
        }
    }

    double sr_ = 44100, centers_[kBands]; bool init_ = false, refValid_ = false; float refNorm_[kBands] = {0}, refLufs_ = -14.f;
    Effective cur_; float inG_ = 1.f, outG_ = 1.f, wet_ = 1.f, loudDb_ = 0.f, tpHold_ = 0.f, inLvl_ = 0.f, eraA_[kBands], eraB_[kBands];
    float corrLL_ = 0, corrRR_ = 0, corrLR_ = 0; unsigned chunkCount_ = 0;
    Biquad hpf_[2], ls_[2], p1_[2], p2_[2], hs_[2]; MatchEngine match_; Compressor comp_, glue_; MultibandComp mb_; TransientShaper trans_; Saturator sat_;
    DeEsser deess_; StereoDelay delay_; Reverb reverb_; StereoProcessor stereo_; Limiter limiter_;
    SpectrumBands inSpec_, outSpec_; LoudnessMeter preMeter_, postMeter_; TruePeak tpL_, tpR_;
    float dryL_[kChunk], dryR_[kChunk], mono_[kChunk];
};
}
