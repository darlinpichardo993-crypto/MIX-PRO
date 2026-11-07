#pragma once
#include "Fft.h"
namespace mp {
constexpr int kBands = 16;
inline double bandEdge(int i) { return 20.0 * std::pow(1000.0, i / (double)kBands); }
inline double bandCenter(int b) { return std::sqrt(bandEdge(b) * bandEdge(b + 1)); }

// Windowed FFT power analyser: tau<=0 -> running mean (offline), tau>0 -> exponential smoothing (live)
class SpectrumBands {
public:
    void prepare(double sr, int fftSize = 4096, double tauSec = 0.0) {
        sr_ = sr; N_ = fftSize; hop_ = N_ / 2; tau_ = tauSec; fft_.init(N_);
        win_.resize(N_); sumW_ = 0; for (int i = 0; i < N_; ++i) { win_[i] = 0.5 - 0.5 * std::cos(2 * kPi * i / N_); sumW_ += win_[i]; }
        buf_.assign(N_, 0.f); re_.assign(N_, 0); im_.assign(N_, 0); p_.assign(N_ / 2 + 1, 0); power_.assign(N_ / 2 + 1, 0);
        alpha_ = tau_ > 0 ? 1.0 - std::exp(-(double)hop_ / (sr * tau_)) : 0.0;
        binHz_ = sr / N_;
        for (int b = 0; b < kBands; ++b) { lo_[b] = (int)std::ceil(bandEdge(b) / binHz_); hi_[b] = (int)std::ceil(bandEdge(b + 1) / binHz_) - 1; lo_[b] = std::max(lo_[b], 1); hi_[b] = std::min(hi_[b], N_ / 2); if (hi_[b] < lo_[b]) hi_[b] = lo_[b]; }
        reset();
    }
    void reset() { std::fill(buf_.begin(), buf_.end(), 0.f); std::fill(power_.begin(), power_.end(), 0.0); w_ = 0; since_ = 0; pushed_ = 0; frames_ = 0; tonalPk_ = tonalTot_ = 0; }
    int frames() const { return frames_; }
    double binHz() const { return binHz_; }
    int fftSize() const { return N_; }
    void push(const float* x, int n) {
        for (int i = 0; i < n; ++i) {
            buf_[w_] = x[i]; if (++w_ >= N_) w_ = 0; ++pushed_;
            if (++since_ >= hop_ && pushed_ >= (size_t)N_) { since_ = 0; frame(); }
        }
    }
    void bandsDb(float* out) const {
        for (int b = 0; b < kBands; ++b) { double s = 0; for (int k = lo_[b]; k <= hi_[b]; ++k) s += power_[k]; out[b] = (float)(10.0 * std::log10(std::max(s, 1e-12))); }
    }
    // shape of the spectrum: weighted-mean-removed (weights nullable)
    void bandsNorm(float* out, const float* wts = nullptr) const {
        bandsDb(out); double sw = 0, sm = 0;
        for (int b = 0; b < kBands; ++b) { const double w = wts ? wts[b] : 1.0; sw += w; sm += w * out[b]; }
        const float mean = sw > 0 ? (float)(sm / sw) : 0.f; for (int b = 0; b < kBands; ++b) out[b] -= mean;
    }
    const std::vector<double>& meanPower() const { return power_; }
    float tonalRatio() const { return tonalTot_ > 0 ? (float)(tonalPk_ / tonalTot_) : 0.f; }
    // log-spaced display values in dB (per-bin power, full-scale sine ~ 0 dB)
    void displayDb(float* out, int nPts, double fLo = 20.0, double fHi = 20000.0) const {
        for (int i = 0; i < nPts; ++i) {
            const double f = fLo * std::pow(fHi / fLo, i / (double)(nPts - 1)), pos = f / binHz_;
            const int k0 = clampT((int)pos, 1, N_ / 2 - 1); const double fr = clampT(pos - k0, 0.0, 1.0);
            const double p = power_[k0] * (1 - fr) + power_[k0 + 1] * fr;
            out[i] = (float)(10.0 * std::log10(std::max(p, 1e-12)));
        }
    }
private:
    void frame() {
        for (int i = 0; i < N_; ++i) { re_[i] = buf_[(w_ + i) % N_] * win_[i]; im_[i] = 0; }
        fft_.forward(re_.data(), im_.data());
        const double norm = 4.0 / (sumW_ * sumW_);
        for (int k = 0; k <= N_ / 2; ++k) p_[k] = (re_[k] * re_[k] + im_[k] * im_[k]) * norm;
        ++frames_;
        for (int k = 0; k <= N_ / 2; ++k) power_[k] += (tau_ > 0 ? alpha_ : 1.0 / frames_) * (p_[k] - power_[k]);
        if (tau_ <= 0) {   // tonality: share of power in clear spectral peaks
            double tot = 0, pk = 0;
            for (int k = 1; k < N_ / 2; ++k) tot += p_[k];
            for (int k = 8; k < N_ / 2 - 8; ++k) if (p_[k] > p_[k - 1] && p_[k] >= p_[k + 1]) {
                double m = 0; for (int j = -6; j <= 6; ++j) m += p_[k + j]; m /= 13.0;
                if (p_[k] > 4.0 * m) pk += p_[k - 1] + p_[k] + p_[k + 1];
            }
            tonalPk_ += pk; tonalTot_ += tot;
        }
    }
    double sr_ = 44100, tau_ = 0, alpha_ = 0, sumW_ = 0, binHz_ = 10, tonalPk_ = 0, tonalTot_ = 0;
    int N_ = 4096, hop_ = 2048, w_ = 0, since_ = 0, frames_ = 0, lo_[kBands], hi_[kBands]; size_t pushed_ = 0;
    FFT fft_; std::vector<double> win_, re_, im_, p_, power_; std::vector<float> buf_;
};
}
