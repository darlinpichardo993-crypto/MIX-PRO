#pragma once
#include "Biquad.h"
namespace mp {

class Saturator {
public:
    void reset() { x1_[0] = x1_[1] = y1_[0] = y1_[1] = 0; }
    void process(float* l, float* r, int n, float drive) {
        if (drive <= 0.0005f) return;
        const float k = 1.f + drive * 8.f, bias = drive * 0.12f, comp = 0.25f / std::tanh(0.25f * k);
        const float dc = std::tanh(bias);
        float* io[2] = { l, r };
        for (int ch = 0; ch < (r ? 2 : 1); ++ch)
            for (int i = 0; i < n; ++i) {
                const float x = io[ch][i];
                const float y = (std::tanh(k * x + bias) - dc) * comp;
                const float o = y - x1_[ch] + 0.995f * y1_[ch];   // DC blocker
                x1_[ch] = y; y1_[ch] = flushDenormal(o);
                io[ch][i] = o;
            }
    }
private: float x1_[2] = {0, 0}, y1_[2] = {0, 0};
};

class TransientShaper {
public:
    void prepare(double sr) {
        fa_ = (float)std::exp(-1.0 / (sr * 0.001)); fr_ = (float)std::exp(-1.0 / (sr * 0.010));
        sa_ = (float)std::exp(-1.0 / (sr * 0.020)); sr_ = (float)std::exp(-1.0 / (sr * 0.120));
        ga_ = (float)std::exp(-1.0 / (sr * 0.0008)); gr_ = (float)std::exp(-1.0 / (sr * 0.005)); reset();
    }
    void reset() { ef_ = es_ = 0; g_ = 0; }
    void process(float* l, float* r, int n, float atk, float sus) {
        if (std::fabs(atk) < 0.01f && std::fabs(sus) < 0.01f) return;
        for (int i = 0; i < n; ++i) {
            float a = std::fabs(l[i]); if (r) a = std::max(a, std::fabs(r[i]));
            ef_ = (a > ef_ ? fa_ : fr_) * ef_ + (a > ef_ ? 1 - fa_ : 1 - fr_) * a;
            es_ = (a > es_ ? sa_ : sr_) * es_ + (a > es_ ? 1 - sa_ : 1 - sr_) * a;
            const float diff = (ef_ > 1e-5f) ? gainToDb(ef_) - gainToDb(es_) : 0.f;
            const float tgt = clampT(atk * 0.8f * std::max(diff, 0.f) + sus * 0.8f * std::max(-diff, 0.f), -12.f, 12.f);
            const float c = std::fabs(tgt) > std::fabs(g_) ? ga_ : gr_;
            g_ = flushDenormal(c * g_ + (1 - c) * tgt);
            const float gain = dbToGain(g_);
            l[i] *= gain; if (r) r[i] *= gain;
        }
    }
private: float fa_ = 0, fr_ = 0, sa_ = 0, sr_ = 0, ga_ = 0, gr_ = 0, ef_ = 0, es_ = 0, g_ = 0;
};

class StereoDelay {
public:
    void prepare(double sr) {
        sr_ = sr; size_ = (int)(sr * 2.0) + 8; for (auto& b : buf_) b.assign(size_, 0.f);
        smooth_ = (float)std::exp(-1.0 / (sr * 0.05)); reset();
    }
    void reset() { for (auto& b : buf_) std::fill(b.begin(), b.end(), 0.f); w_ = 0; lp_[0] = lp_[1] = 0; dCur_ = -1; }
    void process(float* l, float* r, int n, float timeMs, float fb, float mix) {
        if (mix < 0.001f) return;
        const float tgt = clampT((float)(timeMs * sr_ * 0.001), 2.f, (float)(size_ - 4));
        if (dCur_ < 0) dCur_ = tgt;
        const float lpC = (float)std::exp(-2.0 * kPi * 5000.0 / sr_);
        float* io[2] = { l, r };
        for (int i = 0; i < n; ++i) {
            dCur_ = smooth_ * dCur_ + (1 - smooth_) * tgt;
            float rp = (float)w_ - dCur_; if (rp < 0) rp += size_;
            const int i0 = (int)rp, i1 = (i0 + 1) % size_; const float fr = rp - i0;
            for (int ch = 0; ch < (r ? 2 : 1); ++ch) {
                const float d = buf_[ch][i0] * (1 - fr) + buf_[ch][i1] * fr;
                lp_[ch] = flushDenormal(d * (1 - lpC) + lp_[ch] * lpC);
                const float x = io[ch][i];
                buf_[ch][w_] = x + lp_[ch] * fb;
                io[ch][i] = x + d * mix;
            }
            if (++w_ >= size_) w_ = 0;
        }
    }
private: double sr_ = 44100; int size_ = 0, w_ = 0; float smooth_ = 0, dCur_ = -1, lp_[2] = {0, 0}; std::vector<float> buf_[2];
};

class Reverb {
    struct Comb { std::vector<float> b; int i = 0; float s = 0;
        float run(float in, float fb, float damp) { const float o = b[i]; s = flushDenormal(o * (1 - damp) + s * damp); b[i] = in + s * fb; if (++i >= (int)b.size()) i = 0; return o; } };
    struct AP { std::vector<float> b; int i = 0;
        float run(float in) { const float bo = b[i]; const float o = -in + bo; b[i] = in + bo * 0.5f; if (++i >= (int)b.size()) i = 0; return o; } };
public:
    void prepare(double sr) {
        static const int ct[8] = {1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617}, at[4] = {556, 441, 341, 225};
        const double sc = sr / 44100.0;
        for (int c = 0; c < 2; ++c) {
            for (int k = 0; k < 8; ++k) { combs_[c][k].b.assign((size_t)((ct[k] + (c ? 23 : 0)) * sc) + 1, 0.f); combs_[c][k].i = 0; combs_[c][k].s = 0; }
            for (int k = 0; k < 4; ++k) { aps_[c][k].b.assign((size_t)((at[k] + (c ? 23 : 0)) * sc) + 1, 0.f); aps_[c][k].i = 0; }
        }
        pdSize_ = (int)(sr * 0.12) + 4; for (auto& p : pd_) p.assign(pdSize_, 0.f); pw_ = 0; sr_ = sr;
    }
    void reset() { prepare(sr_); }
    void process(float* l, float* r, int n, float size, float damp, float preMs, float mix) {
        if (mix < 0.001f) return;
        const float fb = 0.70f + 0.28f * clampT(size, 0.f, 1.f), dm = 0.4f * clampT(damp, 0.f, 1.f);
        const int pd = clampT((int)(preMs * sr_ * 0.001), 0, pdSize_ - 1);
        for (int i = 0; i < n; ++i) {
            const float inL = l[i], inR = r ? r[i] : l[i];
            pd_[0][pw_] = (inL + inR) * 0.5f * 0.015f; int rp = pw_ - pd; if (rp < 0) rp += pdSize_;
            const float in = pd_[0][rp]; if (++pw_ >= pdSize_) pw_ = 0;
            float out[2] = {0, 0};
            for (int c = 0; c < 2; ++c) {
                for (int k = 0; k < 8; ++k) out[c] += combs_[c][k].run(in, fb, dm);
                for (int k = 0; k < 4; ++k) out[c] = aps_[c][k].run(out[c]);
            }
            l[i] = inL + out[0] * mix * 3.f; if (r) r[i] = inR + out[1] * mix * 3.f;
        }
    }
private: double sr_ = 44100; Comb combs_[2][8]; AP aps_[2][4]; std::vector<float> pd_[2]; int pdSize_ = 1, pw_ = 0;
};

class StereoProcessor {
public:
    void prepare(double sr) { sr_ = sr; reset(); }
    void reset() { hp_.reset(); }
    void process(float* l, float* r, int n, float width, float balance, float monoLowHz) {
        if (!r) return;
        const bool useLow = monoLowHz > 20.f; if (useLow) hp_.set(makeBiquad(FilterType::HighPass, sr_, monoLowHz, 0.7071));
        const float a = (clampT(balance, -1.f, 1.f) + 1.f) * (float)kPi * 0.25f;
        const float gl = std::cos(a) * 1.41421356f, gr = std::sin(a) * 1.41421356f;
        for (int i = 0; i < n; ++i) {
            const float m = (l[i] + r[i]) * 0.5f; float s = (l[i] - r[i]) * 0.5f;
            if (useLow) s = hp_.process(s);
            s *= width; l[i] = (m + s) * gl; r[i] = (m - s) * gr;
        }
    }
private: double sr_ = 44100; Biquad hp_;
};
}
