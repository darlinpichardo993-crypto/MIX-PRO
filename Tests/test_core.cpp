// MIX PRO core test-suite (JUCE-free). Exit code != 0 on any failure.
#include "Engines/MixEngine.h"
#include "Engines/FactoryPresets.h"
#include "Analysis/ReferenceAnalyzer.h"
#include <cstdio>
#include <random>
using namespace mp;
static int g_fail = 0, g_pass = 0;
#define CHECK(c, msg) do { if (c) { ++g_pass; } else { ++g_fail; std::printf("  FAIL: %s  [%s:%d]\n", msg, __FILE__, __LINE__); } } while (0)
static std::vector<float> sine(double f, double amp, size_t n, double sr) { std::vector<float> v(n); for (size_t i = 0; i < n; ++i) v[i] = (float)(amp * std::sin(2 * kPi * f * i / sr)); return v; }
static std::vector<float> noise(size_t n, float amp, unsigned seed = 1) { std::mt19937 g(seed); std::normal_distribution<float> d(0, amp); std::vector<float> v(n); for (auto& x : v) x = d(g); return v; }
static double rmsDb(const std::vector<float>& v, size_t a = 0) { double s = 0; for (size_t i = a; i < v.size(); ++i) s += (double)v[i] * v[i]; return 10 * std::log10(s / (v.size() - a) + 1e-20); }
static bool finiteAll(const std::vector<float>& v) { for (float x : v) if (!std::isfinite(x)) return false; return true; }
#define TEST(name) std::printf("[test] %s\n", name);

static std::vector<float> runEngine(std::vector<float> in, const ParamSet& P, double sr, int block = 512, std::vector<float>* inR = nullptr, std::vector<float>* outR = nullptr) {
    MixEngine e; e.prepare(sr); std::vector<float> l = in, r = inR ? *inR : in;
    for (size_t p = 0; p < l.size(); p += block) { int n = (int)std::min<size_t>(block, l.size() - p); e.process(l.data() + p, r.data() + p, n, P, 120.0); }
    if (outR) *outR = r; return l;
}
static ParamSet manualFlat() { ParamSet P; P.mode = kModeManual; P.manHpf = 20; P.manRatio = 1; P.manMakeup = 0; return P; }

int main() {
    const double sr = 48000; const size_t N = (size_t)(sr * 2);
    TEST("EQ: peak filter gain at centre frequency")
    { Biquad b; b.set(makeBiquad(FilterType::Peak, sr, 1000, 1.0, 6.0)); auto x = sine(1000, 0.1, N, sr); for (auto& s : x) s = b.process(s);
      CHECK(std::fabs(rmsDb(x, N / 2) - rmsDb(sine(1000, 0.1, N, sr), N / 2) - 6.0) < 0.3, "peak +6 dB at 1 kHz");
      CHECK(std::fabs(magnitudeDb(makeBiquad(FilterType::Peak, sr, 1000, 1.0, 6.0), 1000, sr) - 6.0) < 0.01, "analytic response matches"); }
    TEST("Filters: highpass / lowpass attenuation")
    { Biquad h; h.set(makeBiquad(FilterType::HighPass, sr, 1000, 0.7071)); auto x = sine(100, 0.5, N, sr); for (auto& s : x) s = h.process(s);
      CHECK(rmsDb(x, N / 2) < rmsDb(sine(100, 0.5, N, sr), N / 2) - 30, "HPF 1k attenuates 100 Hz > 30 dB");
      Biquad lp; lp.set(makeBiquad(FilterType::LowPass, sr, 1000, 0.7071)); auto y = sine(10000, 0.5, N, sr); for (auto& s : y) s = lp.process(s);
      CHECK(rmsDb(y, N / 2) < rmsDb(sine(10000, 0.5, N, sr), N / 2) - 30, "LPF 1k attenuates 10 kHz > 30 dB"); }
    TEST("Compression: static gain reduction")
    { Compressor c; c.prepare(sr); c.set(-20, 4, 1, 50, 0, 0); auto x = sine(1000, 0.5, N, sr); float gr = c.process(x.data(), nullptr, (int)N); (void)gr;
      const double outDb = 20 * std::log10(0.5 / std::sqrt(2.0) * 0 + 1) + rmsDb(x, N / 2), inDb = rmsDb(sine(1000, 0.5, N, sr), N / 2);
      const double expected = (inDb + 3.01 + 20) * 0.75; (void)expected; // peak level -6.02 -> over thr 13.98 dB -> GR 10.49 dB
      CHECK(std::fabs((inDb - outDb) - 10.49) < 1.0, "GR ~10.5 dB for 4:1, 14 dB over threshold"); }
    TEST("Saturation: harmonics added, output bounded")
    { Saturator s; auto x = sine(1000, 0.5, N, sr), y = x; s.process(y.data(), nullptr, (int)N, 1.0f);
      double d0 = 0, d1 = 0; for (size_t i = N / 2; i < N; ++i) { const double e = y[i] - x[i]; d1 += e * e; d0 += x[i] * x[i]; }
      CHECK(d1 / d0 > 0.001, "saturator alters waveform"); float mx = 0; for (float v : y) mx = std::max(mx, std::fabs(v)); CHECK(mx < 1.5f && finiteAll(y), "bounded & finite"); }
    TEST("Transient shaping: attack boosts peaks")
    { std::vector<float> x(N, 0.f); for (size_t k = 0; k < N; k += 12000) for (int i = 0; i < 2000; ++i) x[k + i] = 0.5f * std::exp(-i / 600.f) * std::sin(2 * (float)kPi * 200 * i / (float)sr);
      auto a = x, b = x; TransientShaper t1, t2; t1.prepare(sr); t2.prepare(sr); t1.process(a.data(), nullptr, (int)N, 1.f, 0.f); t2.process(b.data(), nullptr, (int)N, -1.f, 0.f);
      float pa = 0, pb = 0; for (size_t i = 0; i < N; ++i) { pa = std::max(pa, std::fabs(a[i])); pb = std::max(pb, std::fabs(b[i])); }
      CHECK(pa > pb * 1.1f, "attack +1 yields higher peak than attack -1"); }
    TEST("Delay: echo position and level")
    { StereoDelay d; d.prepare(sr); std::vector<float> l(24000, 0.f), r(24000, 0.f); l[0] = r[0] = 1.f; d.process(l.data(), r.data(), 24000, 100.f, 0.f, 0.5f);
      int pk = 0; float m = 0; for (int i = 10; i < 24000; ++i) if (std::fabs(l[i]) > m) { m = std::fabs(l[i]); pk = i; }
      CHECK(std::abs(pk - 4800) <= 2, "echo at 100 ms"); CHECK(m > 0.3f && m < 0.6f, "echo level ~ mix"); }
    TEST("Reverb: tail present, finite, mix=0 transparent")
    { Reverb rv; rv.prepare(sr); std::vector<float> l(48000, 0.f), r(48000, 0.f); l[0] = r[0] = 1.f; rv.process(l.data(), r.data(), 48000, 0.7f, 0.3f, 0, 0.5f);
      double tail = 0; for (int i = 4800; i < 48000; ++i) tail += l[i] * l[i]; CHECK(tail > 1e-6 && finiteAll(l), "reverb tail exists");
      std::vector<float> a(1000, 0.3f), b = a; rv.process(a.data(), nullptr, 1000, 0.5f, 0.5f, 0, 0.f); CHECK(a == b, "mix 0 leaves signal untouched"); }
    TEST("Stereo: width 0 collapses to mono; mono-low keeps lows centred")
    { auto L = noise(N, 0.1f, 1), R = noise(N, 0.1f, 2); StereoProcessor s; s.prepare(sr); s.process(L.data(), R.data(), (int)N, 0.f, 0.f, 0.f);
      double d = 0; for (size_t i = 0; i < N; ++i) d += std::fabs(L[i] - R[i]); CHECK(d < 1e-3, "width 0 => L==R");
      auto lo = sine(60, 0.5, N, sr), lo2 = lo; for (auto& v : lo2) v = -v; s.prepare(sr); s.process(lo.data(), lo2.data(), (int)N, 1.f, 0.f, 150.f);
      CHECK(rmsDb(lo, N / 2) < rmsDb(sine(60, 0.5, N, sr), N / 2) - 10.0, "side content below 150 Hz is removed (mono low)"); }
    TEST("Loudness: BS.1770 reference level")
    { auto s = sine(1000, 0.1, (size_t)(sr * 6), sr); LoudnessMeter m; m.prepare(sr); m.process(s.data(), s.data(), (int)s.size());
      CHECK(std::fabs(m.integrated() - (-20.0)) < 0.3, "stereo 1 kHz -20 dBFS => -20 LUFS");
      TruePeak tp; auto hi = sine(11025, 0.9, 4800, 44100); float p = tp.process(hi.data(), (int)hi.size()); CHECK(p > 0.88f && p < 1.2f, "true peak sane"); }
    TEST("Spectral analysis: centroid and flatness")
    { auto s = sine(1000, 0.3, (size_t)(sr * 3), sr); SpectrumBands sb; sb.prepare(sr, 4096, 0.0); sb.push(s.data(), (int)s.size());
      Fingerprint f; analyzeReference(s.data(), s.data(), s.size(), sr, "sine", f); CHECK(std::fabs(f.centroidHz - 1000) < 100, "centroid ~1 kHz");
      auto n = noise((size_t)(sr * 3), 0.2f); Fingerprint g; analyzeReference(n.data(), n.data(), n.size(), sr, "noise", g); CHECK(g.flatness > f.flatness * 10, "noise flatter than sine");
      CHECK(f.tonalRatio > g.tonalRatio, "sine more tonal than noise"); }
    TEST("Fingerprint JSON round-trip")
    { auto n = noise((size_t)(sr * 4), 0.2f), m = noise((size_t)(sr * 4), 0.2f, 9); Fingerprint f, g; analyzeReference(n.data(), m.data(), n.size(), sr, "rt \"quote\"", f);
      CHECK(Fingerprint::fromJson(f.toJson(), g), "parse"); CHECK(g.name == f.name && std::fabs(g.lufs - f.lufs) < 1e-3 && std::fabs(g.bandsNormDb[7] - f.bandsNormDb[7]) < 1e-3, "values preserved");
      CHECK(!f.interpret().empty(), "interpretation produced"); CHECK(!Fingerprint::fromJson("{\"x\":1}", g), "rejects foreign json"); }
    TEST("Presets: save / load / apply / state restoration")
    { float v[kNumParams], w[kNumParams]; defaultValues(v); auto fp = factoryPresets(); CHECK(fp.size() >= 30, "factory presets present");
      for (auto& p : fp) { defaultValues(v); p.apply(v); PresetData q; CHECK(PresetData::fromJson(p.toJson(), q) && q.values.size() == p.values.size(), "preset json round-trip"); }
      defaultValues(v); fp[0].apply(v); auto full = PresetData::fromValues("snap", "USER", v); PresetData back; CHECK(PresetData::fromJson(full.toJson(), back), "full state parse"); defaultValues(w); back.apply(w);
      bool same = true; for (int i = 0; i < kNumParams; ++i) same &= std::fabs(v[i] - w[i]) < 1e-4f; CHECK(same, "state restored identically");
      ParamSet P; fillParamSet(v, P); CHECK(P.mode == kModeEra, "preset sets mode"); }
    TEST("Genre library: edit, save, load")
    { auto& g = GenreLibrary::instance(); g.resetDefaults(); g.edit(0).low = 3.3f; auto j = g.toJson(); g.resetDefaults(); CHECK(std::fabs(g.get(0).low - 0.5f) < 1e-5, "reset"); CHECK(g.fromJson(j) && std::fabs(g.get(0).low - 3.3f) < 1e-3, "restored from json"); g.resetDefaults(); }
    TEST("Bypass preserves original signal exactly")
    { ParamSet P; P.mode = kModeAuto; P.bypass = true; auto x = noise(N, 0.2f); auto y = runEngine(x, P, sr); CHECK(x == y, "bypass bit-identical"); }
    TEST("Parameters change the audio")
    { auto x = noise(N, 0.1f); ParamSet A; A.mode = kModeInstrument; A.role = LeadGuitar; ParamSet B = A; B.presence = 1.f; B.character = 0.8f; ParamSet C = A; C.role = Bass;
      auto ya = runEngine(x, A, sr), yb = runEngine(x, B, sr), yc = runEngine(x, C, sr); double d1 = 0, d2 = 0; for (size_t i = 0; i < N; ++i) { d1 += std::fabs(ya[i] - yb[i]); d2 += std::fabs(ya[i] - yc[i]); }
      CHECK(d1 > 1.0, "macros alter output"); CHECK(d2 > 1.0, "role alters output");
      ParamSet M = manualFlat(); M.manHigh = 9.f; auto ym = runEngine(x, M, sr); CHECK(rmsDb(ym) > rmsDb(x) + 1.0, "manual high shelf boosts noise"); }
    TEST("Master engine: loudness target and ceiling")
    { auto x = noise((size_t)(sr * 25), 0.05f); auto xs = sine(220, 0.02, x.size(), sr); for (size_t i = 0; i < x.size(); ++i) x[i] += xs[i];
      ParamSet P; P.mode = kModeMaster; P.mMatchLoud = true; P.mTargetLufs = -14; P.mCeiling = -1; P.mMb = 0.3f; P.mGlue = 0.2f;
      MixEngine e; e.prepare(sr); std::vector<float> l = x, r = x; for (size_t p = 0; p < l.size(); p += 512) e.process(l.data() + p, r.data() + p, (int)std::min<size_t>(512, l.size() - p), P, 120);
      float pk = 0; for (size_t i = 0; i < l.size(); ++i) pk = std::max(pk, std::fabs(l[i])); CHECK(pk <= dbToGain(-1.f) + 1e-4f, "never exceeds ceiling");
      LoudnessMeter m; m.prepare(sr); m.process(l.data() + l.size() / 2, r.data() + l.size() / 2, (int)(l.size() / 2)); CHECK(std::fabs(m.integrated() + 14.f) < 2.5f, "reaches user loudness target (+-2.5 LU)"); 
      CHECK(e.latencySamples(P) > 0, "latency reported in master mode"); }
    TEST("Reference matching moves spectrum toward the reference")
    { const size_t n = (size_t)(sr * 30); auto w = noise(n, 0.1f, 5); std::vector<float> dark = w, ref = w; double z = 0; for (auto& s : dark) { z += 0.15 * (s - z); s = (float)z * 3.f; }
      double a = 0, b = 0, c = 0; for (auto& s : ref) { a = 0.99765 * a + s * 0.0990460; b = 0.96300 * b + s * 0.2965164; c = 0.57 * c + s * 1.0526913; s = (float)(a + b + c + s * 0.1848) * 0.2f; } // pink-ish
      Fingerprint rf; analyzeReference(ref.data(), ref.data(), n, sr, "ref", rf);
      ParamSet P; P.mode = kModeReference; P.matchAmount = 1.f; P.role = Synths; MixEngine e; e.prepare(sr); e.setReference(rf.bandsNormDb, rf.lufs, true);
      std::vector<float> l = dark; for (size_t p = 0; p < n; p += 512) e.process(l.data() + p, nullptr, (int)std::min<size_t>(512, n - p), P, 120);
      Fingerprint fi, fo; analyzeReference(dark.data() + n / 2, nullptr, n / 2, sr, "in", fi); analyzeReference(l.data() + n / 2, nullptr, n / 2, sr, "out", fo);
      float wts[kBands]; bandWeights(roleTable()[Synths].matchLo, roleTable()[Synths].matchHi, wts); float ri[kBands], ro[kBands], rr[kBands]; std::memcpy(ri, fi.bandsNormDb, sizeof ri); std::memcpy(ro, fo.bandsNormDb, sizeof ro); std::memcpy(rr, rf.bandsNormDb, sizeof rr);
      normalizeW(ri, wts); normalizeW(ro, wts); normalizeW(rr, wts); double di = 0, dout = 0; for (int k = 0; k < kBands; ++k) { di += wts[k] * std::fabs(ri[k] - rr[k]); dout += wts[k] * std::fabs(ro[k] - rr[k]); }
      std::printf("   spectral distance to reference: before %.2f dB, after %.2f dB\n", di / kBands, dout / kBands); CHECK(dout < di * 0.8, "distance to reference reduced by >20%"); }
    TEST("Stability: NaN/Inf, all modes/roles, sample rates and block sizes")
    { const double rates[] = {22050, 44100, 48000, 88200, 96000, 192000}; const int blocks[] = {1, 7, 64, 480, 1024, 4096}; bool ok = true;
      for (double sr2 : rates) for (int blk : blocks) { ParamSet P; P.mode = (int)(blk + (int)sr2) % kNumModes; P.role = (int)(sr2 / 100) % kNumRoles; P.character = 1; P.ambience = 1; P.dynamics = 1; P.presence = 1; P.deEss = 1; P.mMb = 1; P.mGlue = 1; P.mMatchLoud = true;
          auto x = noise((size_t)(sr2 * 0.4), 0.8f, 3); auto y = runEngine(x, P, sr2, blk); ok &= finiteAll(y); }
      CHECK(ok, "finite output everywhere");
      for (int m = 0; m < kNumModes; ++m) for (int r = 0; r < kNumRoles; ++r) { ParamSet P; P.mode = m; P.role = r; P.genre = (m * 7 + r) % kNumGenres; auto x = noise(4800, 0.9f, m + r); auto y = runEngine(x, P, 48000, 300); ok &= finiteAll(y); }
      CHECK(ok, "all mode x role combinations finite");
      std::vector<float> sil(48000, 0.f); ParamSet P; P.mode = kModeAuto; CHECK(finiteAll(runEngine(sil, P, sr)), "silence safe"); }
    std::printf("\n%d checks passed, %d failed\n", g_pass, g_fail); return g_fail ? 1 : 0;
}
