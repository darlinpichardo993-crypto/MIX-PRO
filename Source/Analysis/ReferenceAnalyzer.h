#pragma once
#include "Fingerprint.h"
#include "Loudness.h"
namespace mp {
// Offline analysis of a reference. R may be nullptr (mono). Returns false if the material is too short.
inline float gainToDbSafe(float g) { return 20.0f * std::log10(std::max(g, 1.0e-9f)); }
inline bool analyzeReference(const float* L, const float* R, size_t n, double sr, const std::string& name, Fingerprint& fp) {
    if (!L || sr < 8000 || n < (size_t)(sr * 1.0)) return false;
    fp = Fingerprint(); fp.name = name; fp.sampleRate = sr; fp.durationSec = (double)n / sr;
    std::vector<float> mono(n); for (size_t i = 0; i < n; ++i) mono[i] = R ? 0.5f * (L[i] + R[i]) : L[i];

    SpectrumBands sb; sb.prepare(sr, 4096, 0.0); sb.push(mono.data(), (int)n);
    sb.bandsDb(fp.bandsDb); sb.bandsNorm(fp.bandsNormDb); fp.tonalRatio = sb.tonalRatio();
    {   const auto& P = sb.meanPower(); double tot = 0, wf = 0, lg = 0, ar = 0; int cnt = 0; const double bh = sb.binHz();
        const int kLo = std::max(1, (int)(20.0 / bh)), kHi = std::min((int)P.size() - 1, (int)(20000.0 / bh));
        for (int k = kLo; k <= kHi; ++k) { tot += P[k]; wf += P[k] * k * bh; lg += std::log(P[k] + 1e-20); ar += P[k]; ++cnt; }
        fp.centroidHz = tot > 0 ? (float)(wf / tot) : 0.f; double cum = 0; fp.rolloffHz = (float)(kHi * bh);
        for (int k = kLo; k <= kHi; ++k) { cum += P[k]; if (cum >= 0.85 * tot) { fp.rolloffHz = (float)(k * bh); break; } }
        fp.flatness = (cnt && ar > 0) ? (float)(std::exp(lg / cnt) / (ar / cnt)) : 0.f; }

    double sl = 0, sr2 = 0, slr = 0, sm = 0, ss = 0; float pk = 0;
    for (size_t i = 0; i < n; ++i) {
        const double l = L[i], r = R ? R[i] : L[i]; sl += l * l; sr2 += r * r; slr += l * r;
        const double m = 0.5 * (l + r), s = 0.5 * (l - r); sm += m * m; ss += s * s; pk = std::max(pk, std::max(std::fabs((float)l), std::fabs((float)r)));
    }
    const double ms = (sl + sr2) / (2.0 * n);
    fp.rmsDb = (float)(10 * std::log10(std::max(ms, 1e-12))); fp.peakDb = gainToDbSafe(pk);
    { TruePeak a, b; float tp = a.process(L, (int)std::min<size_t>(n, 1u << 30)); if (R) tp = std::max(tp, b.process(R, (int)std::min<size_t>(n, 1u << 30))); fp.truePeakDb = gainToDbSafe(tp); }
    fp.crestDb = fp.peakDb - fp.rmsDb;
    LoudnessMeter lm; lm.prepare(sr); lm.process(L, R, (int)n); fp.lufs = lm.integrated();
    fp.dynamicRangeDb = std::max(0.f, lm.percentile(0.95f) - lm.percentile(0.10f));

    if (R) {
        const double den = std::sqrt(sl * sr2); fp.correlation = den > 1e-12 ? (float)(slr / den) : 1.f;
        fp.stereoWidth = sm > 1e-12 ? (float)std::sqrt(ss / sm) : 0.f;
        fp.midSideDb = (float)(10 * std::log10(std::max(sm, 1e-12) / std::max(ss, 1e-12)));
        fp.lrBalanceDb = (float)(10 * std::log10(std::max(sl, 1e-12) / std::max(sr2, 1e-12)));
        fp.monoCompatDb = (float)(10 * std::log10(std::max(sm / n, 1e-12) / std::max(ms, 1e-12)));
    } else { fp.correlation = 1; fp.stereoWidth = 0; fp.midSideDb = 60; fp.monoCompatDb = 0; }

    // 10 ms RMS envelope
    const int hop = std::max(1, (int)(sr * 0.01)); const int ne = (int)(n / hop);
    std::vector<float> env(ne), envDb(ne);
    for (int i = 0; i < ne; ++i) { double e = 0; for (int k = 0; k < hop; ++k) { const float v = mono[(size_t)i * hop + k]; e += v * v; } env[i] = (float)std::sqrt(e / hop); envDb[i] = gainToDbSafe(env[i]); }
    int onsets = 0, last = -100; for (int i = 2; i < ne; ++i) if (envDb[i] - envDb[i - 2] > 6.f && envDb[i] > -50.f && i - last >= 5) { ++onsets; last = i; }
    fp.transientDensity = (float)(onsets / fp.durationSec);
    // decay slopes after local maxima
    std::vector<float> t60s;
    for (int i = 1; i + 6 < ne; ++i) if (envDb[i] > -40.f && envDb[i] >= envDb[i - 1] && envDb[i] > envDb[i + 1]) {
        int j = i; while (j + 1 < ne && envDb[j + 1] <= envDb[j] + 0.5f && envDb[i] - envDb[j + 1] < 20.f) ++j;
        const float drop = envDb[i] - envDb[j]; if (j - i >= 5 && drop >= 10.f) t60s.push_back(60.f / (drop / ((j - i) * 0.01f)));
    }
    if (!t60s.empty()) { std::sort(t60s.begin(), t60s.end()); fp.reverbT60 = t60s[t60s.size() / 2]; fp.reverbConfidence = std::min(1.f, t60s.size() / 20.f) * 0.7f; }
    // echo via envelope autocorrelation (10 ms resolution)
    if (ne > 100) { double mean = 0; for (float v : env) mean += v; mean /= ne; double r0 = 0; for (float v : env) r0 += (v - mean) * (v - mean);
        double best = 0; int bl = 0; for (int lag = 5; lag <= std::min(80, ne / 3); ++lag) { double s = 0; for (int i = 0; i + lag < ne; ++i) s += (env[i] - mean) * (env[i + lag] - mean); s /= std::max(r0, 1e-20); if (s > best) { best = s; bl = lag; } }
        fp.echoMs = bl * 10.f; fp.echoStrength = (float)std::max(0.0, best); }
    float conf = (float)clampT(fp.durationSec / 30.0, 0.15, 1.0); if (fp.rmsDb < -50.f) conf *= 0.3f; fp.confidence = conf;
    fp.valid = true; return true;
}
}
