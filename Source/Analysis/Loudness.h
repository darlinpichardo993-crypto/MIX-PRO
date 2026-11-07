#pragma once
#include "../DSP/Biquad.h"
#include <array>
namespace mp {
// ITU-R BS.1770-4 loudness with K-weighting, 400 ms blocks / 75% overlap, absolute (-70) and relative (-10 LU) gating
class LoudnessMeter {
public:
    void prepare(double sr) {
        sr_ = sr;
        { // stage 1: high shelf
            const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
            const double K = std::tan(kPi * f0 / sr), Vh = std::pow(10.0, G / 20.0), Vb = std::pow(Vh, 0.4996667741545416);
            const double a0 = 1.0 + K / Q + K * K; BiquadCoefs c;
            c.b0 = (Vh + Vb * K / Q + K * K) / a0; c.b1 = 2.0 * (K * K - Vh) / a0; c.b2 = (Vh - Vb * K / Q + K * K) / a0;
            c.a1 = 2.0 * (K * K - 1.0) / a0; c.a2 = (1.0 - K / Q + K * K) / a0; for (auto& b : s1_) b.set(c);
        }
        { // stage 2: RLB high-pass
            const double f0 = 38.13547087602444, Q = 0.5003270373238773, K = std::tan(kPi * f0 / sr);
            const double a0 = 1.0 + K / Q + K * K; BiquadCoefs c;
            c.b0 = 1; c.b1 = -2; c.b2 = 1; c.a1 = 2.0 * (K * K - 1.0) / a0; c.a2 = (1.0 - K / Q + K * K) / a0; for (auto& b : s2_) b.set(c);
        }
        subLen_ = std::max(1, (int)(sr * 0.1)); reset();
    }
    void reset() {
        for (auto& b : s1_) b.reset(); for (auto& b : s2_) b.reset();
        acc_ = 0; cnt_ = 0; ring_.fill(0.0); pos_ = 0; nSub_ = 0; hCount_.fill(0); hEnergy_.fill(0.0);
        mom_ = st_ = -120.f;
    }
    void process(const float* l, const float* r, int n) {
        for (int i = 0; i < n; ++i) {
            const float a = s2_[0].process(s1_[0].process(l[i])); acc_ += (double)a * a;
            if (r) { const float b = s2_[1].process(s1_[1].process(r[i])); acc_ += (double)b * b; }
            if (++cnt_ >= subLen_) { pushSub(acc_ / subLen_); acc_ = 0; cnt_ = 0; }
        }
    }
    float momentary() const { return mom_; }
    float shortTerm() const { return st_; }
    float integrated() const {
        double sumE = 0; long long cnt = 0; for (int i = 0; i < kBins; ++i) { sumE += hEnergy_[i]; cnt += hCount_[i]; }
        if (!cnt) return -120.f;
        const double relThr = toLufs(sumE / cnt) - 10.0; const int i0 = std::max(0, (int)std::ceil((relThr + 70.0) * 10.0));
        sumE = 0; cnt = 0; for (int i = i0; i < kBins; ++i) { sumE += hEnergy_[i]; cnt += hCount_[i]; }
        return cnt ? (float)toLufs(sumE / cnt) : -120.f;
    }
    // percentile (0..1) of gated 400 ms loudness distribution
    float percentile(float p) const {
        double sumE = 0; long long cnt = 0; for (int i = 0; i < kBins; ++i) { sumE += hEnergy_[i]; cnt += hCount_[i]; }
        if (!cnt) return -120.f;
        const int i0 = std::max(0, (int)std::ceil((toLufs(sumE / cnt) - 10.0 + 70.0) * 10.0));
        long long tot = 0; for (int i = i0; i < kBins; ++i) tot += hCount_[i]; if (!tot) return -120.f;
        const long long target = (long long)(p * (double)tot); long long run = 0;
        for (int i = i0; i < kBins; ++i) { run += hCount_[i]; if (run >= target) return (float)(i / 10.0 - 70.0); }
        return (float)(kBins / 10.0 - 70.0);
    }
    static double toLufs(double meanSquare) { return -0.691 + 10.0 * std::log10(std::max(meanSquare, 1e-12)); }
private:
    static constexpr int kBins = 801;
    void pushSub(double e) {
        ring_[pos_] = e; pos_ = (pos_ + 1) % 30; if (nSub_ < 30) ++nSub_;
        auto meanLast = [&](int k) { double s = 0; for (int i = 1; i <= k; ++i) s += ring_[(pos_ - i + 60) % 30]; return s / k; };
        if (nSub_ >= 4) {
            const double m = meanLast(4), L = toLufs(m); mom_ = (float)L;
            if (L >= -70.0) { const int idx = clampT((int)std::lround((L + 70.0) * 10.0), 0, kBins - 1); ++hCount_[idx]; hEnergy_[idx] += m; }
        }
        st_ = (float)toLufs(meanLast(std::min(nSub_, 30)));
    }
    double sr_ = 44100, acc_ = 0; int subLen_ = 4410, cnt_ = 0, pos_ = 0, nSub_ = 0; float mom_ = -120.f, st_ = -120.f;
    Biquad s1_[2], s2_[2]; std::array<double, 30> ring_{}; std::array<long long, kBins> hCount_{}; std::array<double, kBins> hEnergy_{};
};

// 4x oversampled true-peak detector (12-tap windowed-sinc interpolation)
class TruePeak {
public:
    TruePeak() { for (int p = 1; p <= 3; ++p) for (int i = 0; i < 12; ++i) {
        const double t = (i - 5) - p / 4.0, s = std::fabs(t) < 1e-9 ? 1.0 : std::sin(kPi * t) / (kPi * t);
        k_[p - 1][i] = (float)(s * 0.5 * (1.0 + std::cos(kPi * t / 6.0))); } reset(); }
    void reset() { for (auto& v : w_) v = 0; }
    float process(const float* x, int n) {
        float pk = 0;
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < 11; ++j) w_[j] = w_[j + 1]; w_[11] = x[i];
            pk = std::max(pk, std::fabs(w_[5]));
            for (int p = 0; p < 3; ++p) { float s = 0; for (int j = 0; j < 12; ++j) s += w_[j] * k_[p][j]; pk = std::max(pk, std::fabs(s)); }
        }
        return pk;
    }
private: float k_[3][12]; float w_[12];
};
}
