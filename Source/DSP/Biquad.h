#pragma once
#include "Common.h"
#include <complex>
namespace mp {
enum class FilterType { LowPass, HighPass, Peak, LowShelf, HighShelf, AllPass };
struct BiquadCoefs { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };

inline BiquadCoefs makeBiquad(FilterType t, double sr, double f, double q, double gainDb = 0.0) {
    f = clampT(f, 10.0, sr * 0.49); q = std::max(q, 0.05);
    const double w0 = 2.0 * kPi * f / sr, cw = std::cos(w0), sw = std::sin(w0);
    const double alpha = sw / (2.0 * q), A = std::pow(10.0, gainDb / 40.0), sqA = std::sqrt(A);
    double b0 = 1, b1 = 0, b2 = 0, a0 = 1, a1 = 0, a2 = 0;
    switch (t) {
    case FilterType::LowPass:  b0 = (1 - cw) / 2; b1 = 1 - cw; b2 = b0; a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
    case FilterType::HighPass: b0 = (1 + cw) / 2; b1 = -(1 + cw); b2 = b0; a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
    case FilterType::Peak:     b0 = 1 + alpha * A; b1 = -2 * cw; b2 = 1 - alpha * A; a0 = 1 + alpha / A; a1 = -2 * cw; a2 = 1 - alpha / A; break;
    case FilterType::LowShelf:
        b0 = A * ((A + 1) - (A - 1) * cw + 2 * sqA * alpha); b1 = 2 * A * ((A - 1) - (A + 1) * cw);
        b2 = A * ((A + 1) - (A - 1) * cw - 2 * sqA * alpha); a0 = (A + 1) + (A - 1) * cw + 2 * sqA * alpha;
        a1 = -2 * ((A - 1) + (A + 1) * cw); a2 = (A + 1) + (A - 1) * cw - 2 * sqA * alpha; break;
    case FilterType::HighShelf:
        b0 = A * ((A + 1) + (A - 1) * cw + 2 * sqA * alpha); b1 = -2 * A * ((A - 1) + (A + 1) * cw);
        b2 = A * ((A + 1) + (A - 1) * cw - 2 * sqA * alpha); a0 = (A + 1) - (A - 1) * cw + 2 * sqA * alpha;
        a1 = 2 * ((A - 1) - (A + 1) * cw); a2 = (A + 1) - (A - 1) * cw - 2 * sqA * alpha; break;
    case FilterType::AllPass:  b0 = 1 - alpha; b1 = -2 * cw; b2 = 1 + alpha; a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
    }
    BiquadCoefs c; c.b0 = b0 / a0; c.b1 = b1 / a0; c.b2 = b2 / a0; c.a1 = a1 / a0; c.a2 = a2 / a0; return c;
}

struct Biquad {
    BiquadCoefs c; double z1 = 0, z2 = 0;
    void set(const BiquadCoefs& n) { c = n; }
    void reset() { z1 = z2 = 0; }
    inline float process(float x) {
        double y = c.b0 * x + z1;
        z1 = c.b1 * x - c.a1 * y + z2;
        z2 = c.b2 * x - c.a2 * y;
        if (!std::isfinite(y)) { z1 = z2 = 0; return 0.f; }
        return (float)y;
    }
};

inline double magnitudeDb(const BiquadCoefs& c, double f, double sr) {
    const double w = 2.0 * kPi * f / sr;
    const std::complex<double> e1(std::cos(w), -std::sin(w)), e2 = e1 * e1;
    const auto num = c.b0 + c.b1 * e1 + c.b2 * e2, den = 1.0 + c.a1 * e1 + c.a2 * e2;
    return 20.0 * std::log10(std::max(std::abs(num / den), 1e-12));
}

// Linkwitz-Riley 4th order = two cascaded Butterworth biquads
struct LR4 {
    Biquad a[2], b[2];
    void set(FilterType t, double sr, double f) {
        const auto c = makeBiquad(t, sr, f, 0.70710678);
        for (int i = 0; i < 2; ++i) { a[i].set(c); b[i].set(c); }
    }
    void reset() { for (int i = 0; i < 2; ++i) { a[i].reset(); b[i].reset(); } }
    inline float process(int ch, float x) { return b[ch].process(a[ch].process(x)); }
};
}
