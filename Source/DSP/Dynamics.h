#pragma once
#include "Biquad.h"
namespace mp {

class Compressor {
public:
    void prepare(double sr) { sr_ = sr; gr_ = 0; set(-18, 2, 10, 100, 6, 0); }
    void reset() { gr_ = 0; }
    void set(float thr, float ratio, float attMs, float relMs, float knee, float makeup) {
        thr_ = thr; ratio_ = std::max(1.0f, ratio); knee_ = std::max(0.0f, knee); makeup_ = makeup;
        att_ = (float)std::exp(-1.0 / (sr_ * std::max(attMs, 0.01f) * 0.001));
        rel_ = (float)std::exp(-1.0 / (sr_ * std::max(relMs, 1.0f) * 0.001));
    }
    // returns max gain reduction (dB, positive) in block. r may be null (mono)
    float process(float* l, float* r, int n) {
        float maxGr = 0; const float slope = 1.0f - 1.0f / ratio_;
        for (int i = 0; i < n; ++i) {
            float a = std::fabs(l[i]); if (r) a = std::max(a, std::fabs(r[i]));
            const float over = gainToDb(a) - thr_;
            float target;
            if (knee_ < 0.01f) target = over > 0 ? slope * over : 0.f;
            else if (2 * over < -knee_) target = 0.f;
            else if (2 * over > knee_) target = slope * over;
            else { const float x = over + knee_ * 0.5f; target = slope * x * x / (2 * knee_); }
            const float c = target > gr_ ? att_ : rel_;
            gr_ = flushDenormal(c * gr_ + (1 - c) * target);
            const float g = dbToGain(makeup_ - gr_);
            l[i] *= g; if (r) r[i] *= g;
            maxGr = std::max(maxGr, gr_);
        }
        return maxGr;
    }
private:
    double sr_ = 44100; float thr_ = -18, ratio_ = 2, knee_ = 6, makeup_ = 0, att_ = 0, rel_ = 0, gr_ = 0;
};

// Look-ahead peak limiter (min-window + moving average, hard safety clip)
class Limiter {
public:
    void prepare(double sr) {
        sr_ = sr; N_ = std::max(8, (int)(sr * 0.002));
        for (auto& d : dl_) d.assign(N_, 0.f);
        req_.assign(N_, 1.f); avg_.assign(N_, 1.f); reset(); setRelease(80);
    }
    int latency() const { return N_; }
    void reset() {
        for (auto& d : dl_) std::fill(d.begin(), d.end(), 0.f);
        std::fill(req_.begin(), req_.end(), 1.f); std::fill(avg_.begin(), avg_.end(), 1.f);
        pos_ = 0; sum_ = (double)N_; gRel_ = 1.f; minGain_ = 1.f;
    }
    void setRelease(float ms) { relC_ = (float)std::exp(-1.0 / (sr_ * std::max(ms, 1.f) * 0.001)); }
    void setCeilingDb(float db) { ceil_ = dbToGain(db); }
    // returns min gain (linear) reached in this block
    float process(float* l, float* r, int n) {
        float blockMin = 1.f;
        for (int i = 0; i < n; ++i) {
            float pk = std::fabs(l[i]); if (r) pk = std::max(pk, std::fabs(r[i]));
            req_[pos_] = pk > ceil_ ? ceil_ / pk : 1.f;
            float gMin = 1.f; for (int k = 0; k < N_; ++k) gMin = std::min(gMin, req_[k]);
            if (gMin < gRel_) gRel_ = gMin; else gRel_ = relC_ * gRel_ + (1 - relC_) * gMin;
            sum_ += gRel_ - avg_[pos_]; avg_[pos_] = gRel_;
            const float g = (float)(sum_ / N_);
            const float outL = dl_[0][pos_], outR = r ? dl_[1][pos_] : 0.f;
            dl_[0][pos_] = l[i]; if (r) dl_[1][pos_] = r[i];
            l[i] = clampT(outL * g, -ceil_, ceil_); if (r) r[i] = clampT(outR * g, -ceil_, ceil_);
            blockMin = std::min(blockMin, g);
            if (++pos_ >= N_) pos_ = 0;
        }
        return blockMin;
    }
private:
    double sr_ = 44100; int N_ = 88, pos_ = 0; float ceil_ = 0.891f, relC_ = 0.99f, gRel_ = 1.f, minGain_ = 1.f; double sum_ = 0;
    std::vector<float> dl_[2], req_, avg_;
};

// Split-band de-esser: reduces the band above `freq` when it dominates the full-band level
class DeEsser {
public:
    void prepare(double sr) { sr_ = sr; set(6500.f); reset();
        atk_ = (float)std::exp(-1.0 / (sr * 0.0005)); rel_ = (float)std::exp(-1.0 / (sr * 0.03));
        gAtk_ = (float)std::exp(-1.0 / (sr * 0.001)); gRel_ = (float)std::exp(-1.0 / (sr * 0.04)); }
    void reset() { hp_[0].reset(); hp_[1].reset(); eb_ = ef_ = 0; gr_ = 0; }
    void set(float freq) { const auto c = makeBiquad(FilterType::HighPass, sr_, freq, 0.7071); hp_[0].set(c); hp_[1].set(c); }
    float process(float* l, float* r, int n, float amount) {
        float maxGr = 0;
        for (int i = 0; i < n; ++i) {
            const float hl = hp_[0].process(l[i]); const float hr = r ? hp_[1].process(r[i]) : 0.f;
            const float band = std::max(std::fabs(hl), std::fabs(hr));
            float full = std::fabs(l[i]); if (r) full = std::max(full, std::fabs(r[i]));
            eb_ = (band > eb_ ? atk_ : rel_) * eb_ + (band > eb_ ? 1 - atk_ : 1 - rel_) * band;
            ef_ = (full > ef_ ? atk_ : rel_) * ef_ + (full > ef_ ? 1 - atk_ : 1 - rel_) * full;
            const float sib = gainToDb(eb_) - gainToDb(ef_);
            const float target = (ef_ > 1e-4f) ? clampT((sib + 12.f) * amount * 1.2f, 0.f, 12.f) : 0.f;
            const float c = target > gr_ ? gAtk_ : gRel_;
            gr_ = flushDenormal(c * gr_ + (1 - c) * target);
            const float k = 1.f - dbToGain(-gr_);
            l[i] -= hl * k; if (r) r[i] -= hr * k;
            maxGr = std::max(maxGr, gr_);
        }
        return maxGr;
    }
private:
    double sr_ = 44100; Biquad hp_[2]; float eb_ = 0, ef_ = 0, gr_ = 0, atk_ = 0, rel_ = 0, gAtk_ = 0, gRel_ = 0;
};

// Three-band LR4 multiband compressor with program-dependent thresholds
class MultibandComp {
public:
    static constexpr int kMax = 512;
    void prepare(double sr) {
        sr_ = sr;
        const double f1 = 150.0, f2 = 2500.0;
        lp1_.set(FilterType::LowPass, sr, f1); hp1_.set(FilterType::HighPass, sr, f1);
        lp2_.set(FilterType::LowPass, sr, f2); hp2_.set(FilterType::HighPass, sr, f2);
        apLp_.set(FilterType::LowPass, sr, f2); apHp_.set(FilterType::HighPass, sr, f2);
        for (auto& c : comp_) c.prepare(sr);
        for (auto& a : avgDb_) a = -60.f; reset();
    }
    void reset() { lp1_.reset(); hp1_.reset(); lp2_.reset(); hp2_.reset(); apLp_.reset(); apHp_.reset(); for (auto& c : comp_) c.reset(); }
    float process(float* l, float* r, int n, float amount) {
        if (amount < 0.001f || n > kMax) return 0.f;
        const int nch = r ? 2 : 1; float* io[2] = { l, r };
        for (int ch = 0; ch < nch; ++ch)
            for (int i = 0; i < n; ++i) {
                const float x = io[ch][i];
                const float low = lp1_.process(ch, x);
                const float lowAligned = apLp_.process(ch, low) + apHp_.process(ch, low);
                const float rest = hp1_.process(ch, x);
                band_[0][ch][i] = lowAligned; band_[1][ch][i] = lp2_.process(ch, rest); band_[2][ch][i] = hp2_.process(ch, rest);
            }
        float maxGr = 0; const float k = 1.f - std::exp(-(float)n / (float)(sr_ * 2.0));
        for (int b = 0; b < 3; ++b) {
            double e = 0; for (int ch = 0; ch < nch; ++ch) for (int i = 0; i < n; ++i) e += band_[b][ch][i] * band_[b][ch][i];
            const float lvl = 10.f * std::log10((float)(e / (n * nch)) + 1e-12f);
            if (lvl > -80.f) avgDb_[b] += (lvl - avgDb_[b]) * k;
            comp_[b].set(avgDb_[b] + 2.f - 4.f * amount, 1.f + 2.5f * amount, 15.f + 10.f * b, 150.f - 40.f * b, 6.f, 0.f);
            maxGr = std::max(maxGr, comp_[b].process(band_[b][0], nch > 1 ? band_[b][1] : nullptr, n));
        }
        for (int ch = 0; ch < nch; ++ch) for (int i = 0; i < n; ++i) io[ch][i] = band_[0][ch][i] + band_[1][ch][i] + band_[2][ch][i];
        return maxGr;
    }
private:
    double sr_ = 44100; LR4 lp1_, hp1_, lp2_, hp2_, apLp_, apHp_; Compressor comp_[3]; float avgDb_[3];
    float band_[3][2][kMax];
};
}
