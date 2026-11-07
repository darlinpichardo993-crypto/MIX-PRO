#pragma once
#include "../DSP/Common.h"
namespace mp {
class FFT {
public:
    explicit FFT(int n = 4096) { init(n); }
    void init(int n) {
        n_ = n; cos_.resize(n / 2); sin_.resize(n / 2); rev_.resize(n);
        int bits = 0; while ((1 << bits) < n) ++bits;
        for (int i = 0; i < n / 2; ++i) { cos_[i] = std::cos(2 * kPi * i / n); sin_[i] = std::sin(2 * kPi * i / n); }
        for (int i = 0; i < n; ++i) { int r = 0; for (int b = 0; b < bits; ++b) if (i & (1 << b)) r |= 1 << (bits - 1 - b); rev_[i] = r; }
    }
    int size() const { return n_; }
    void forward(double* re, double* im) const {
        for (int i = 0; i < n_; ++i) { const int j = rev_[i]; if (j > i) { std::swap(re[i], re[j]); std::swap(im[i], im[j]); } }
        for (int len = 2; len <= n_; len <<= 1) {
            const int half = len / 2, step = n_ / len;
            for (int i = 0; i < n_; i += len)
                for (int k = 0; k < half; ++k) {
                    const double wr = cos_[k * step], wi = -sin_[k * step];
                    const int a = i + k, b = a + half;
                    const double tr = re[b] * wr - im[b] * wi, ti = re[b] * wi + im[b] * wr;
                    re[b] = re[a] - tr; im[b] = im[a] - ti; re[a] += tr; im[a] += ti;
                }
        }
    }
private: int n_ = 0; std::vector<double> cos_, sin_; std::vector<int> rev_;
};
}
