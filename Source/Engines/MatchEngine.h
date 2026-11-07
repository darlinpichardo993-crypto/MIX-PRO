#pragma once
#include "../DSP/Biquad.h"
#include "../Analysis/Spectrum.h"
namespace mp {
inline void bandWeights(float lo, float hi, float* w) {
    for (int b = 0; b < kBands; ++b) { const double fc = bandCenter(b);
        w[b] = fc < lo ? (float)std::max(0.0, 1.0 - std::log2(lo / fc)) : fc > hi ? (float)std::max(0.0, 1.0 - std::log2(fc / hi)) : 1.f; }
}
inline void normalizeW(float* a, const float* w) {
    double sw = 0, sm = 0; for (int b = 0; b < kBands; ++b) { sw += w[b]; sm += w[b] * a[b]; }
    const float m = sw > 0 ? (float)(sm / sw) : 0.f; for (int b = 0; b < kBands; ++b) a[b] -= m;
}
// 16 peaking filters whose gains are derived from (input spectrum -> target spectrum) or from resonance/accumulation detection
class MatchEngine {
public:
    void prepare(double sr) { sr_ = sr; for (int b = 0; b < kBands; ++b) { gain_[b] = applied_[b] = 0; for (int c = 0; c < 2; ++c) { f_[c][b].reset(); f_[c][b].set(BiquadCoefs()); } } }
    void update(const float* in, const float* target, int kind, float amount, const float* w, int n, bool freeze, bool ready) {
        float d[kBands] = {0}, s[kBands];
        const bool active = kind != 0 && ready;
        if (active) {
            if (kind == 1) {
                for (int b = 0; b < kBands; ++b) { float sum = 0; int c = 0;
                    for (int dd = -2; dd <= 2; ++dd) { const int j = b + dd; if (dd && j >= 0 && j < kBands) { sum += in[j]; ++c; } }
                    const float bump = in[b] - sum / c; d[b] = -clampT((bump - 2.5f) * 0.7f, 0.f, 4.5f) * amount * w[b]; }
            } else for (int b = 0; b < kBands; ++b) d[b] = clampT((target[b] - in[b]) * amount * w[b], -9.f, 9.f);
        }
        for (int b = 0; b < kBands; ++b) { const float a = d[b > 0 ? b - 1 : b], c = d[b < kBands - 1 ? b + 1 : b]; s[b] = 0.25f * a + 0.5f * d[b] + 0.25f * c; }
        if (kind == 0 || (!freeze && ready)) {
            const float rate = 8.f * (float)n / (float)sr_;
            for (int b = 0; b < kBands; ++b) gain_[b] += clampT(s[b] - gain_[b], -rate, rate);
        }
        for (int b = 0; b < kBands; ++b) if (std::fabs(gain_[b] - applied_[b]) > 0.02f) {
            applied_[b] = gain_[b]; const auto c = makeBiquad(FilterType::Peak, sr_, bandCenter(b), 1.8, gain_[b]);
            f_[0][b].set(c); f_[1][b].set(c); }
    }
    void process(float* l, float* r, int n) {
        for (int b = 0; b < kBands; ++b) if (std::fabs(applied_[b]) > 0.05f) {
            for (int i = 0; i < n; ++i) l[i] = f_[0][b].process(l[i]);
            if (r) for (int i = 0; i < n; ++i) r[i] = f_[1][b].process(r[i]);
        }
    }
    const float* gains() const { return applied_; }
private: double sr_ = 44100; float gain_[kBands], applied_[kBands]; Biquad f_[2][kBands];
};
}
