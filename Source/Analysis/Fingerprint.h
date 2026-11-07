#pragma once
#include "MiniJson.h"
#include "Spectrum.h"
namespace mp {
struct Fingerprint {
    int version = 1; std::string name; double sampleRate = 0, durationSec = 0;
    float bandsDb[kBands] = {}, bandsNormDb[kBands] = {};
    float centroidHz = 0, rolloffHz = 0, flatness = 0, tonalRatio = 0;
    float rmsDb = -120, peakDb = -120, truePeakDb = -120, lufs = -120, crestDb = 0, dynamicRangeDb = 0;
    float transientDensity = 0, stereoWidth = 0, midSideDb = 0, lrBalanceDb = 0, correlation = 1, monoCompatDb = 0;
    float reverbT60 = 0, reverbConfidence = 0, echoMs = 0, echoStrength = 0, confidence = 0;
    bool valid = false;

    std::string toJson() const {
        std::string j = "{\n  \"format\": \"MIXPRO-FINGERPRINT\",\n  \"version\": " + std::to_string(version) + ",\n  \"name\": \"" + jsonEscape(name) + "\",\n";
        auto n = [&](const char* k, double v) { j += std::string("  \"") + k + "\": " + jsonNum(v) + ",\n"; };
        auto a = [&](const char* k, const float* v) { j += std::string("  \"") + k + "\": ["; for (int i = 0; i < kBands; ++i) j += (i ? ", " : "") + jsonNum(v[i]); j += "],\n"; };
        n("sampleRate", sampleRate); n("durationSec", durationSec); a("bandsDb", bandsDb); a("bandsNormDb", bandsNormDb);
        n("centroidHz", centroidHz); n("rolloffHz", rolloffHz); n("flatness", flatness); n("tonalRatio", tonalRatio);
        n("rmsDb", rmsDb); n("peakDb", peakDb); n("truePeakDb", truePeakDb); n("lufs", lufs); n("crestDb", crestDb); n("dynamicRangeDb", dynamicRangeDb);
        n("transientDensity", transientDensity); n("stereoWidth", stereoWidth); n("midSideDb", midSideDb); n("lrBalanceDb", lrBalanceDb);
        n("correlation", correlation); n("monoCompatDb", monoCompatDb); n("reverbT60", reverbT60); n("reverbConfidence", reverbConfidence);
        n("echoMs", echoMs); n("echoStrength", echoStrength); j += "  \"confidence\": " + jsonNum(confidence) + "\n}\n"; return j;
    }
    static bool fromJson(const std::string& s, Fingerprint& f) {
        JsonObj o; if (!parseJson(s, o) || o.str("format") != "MIXPRO-FINGERPRINT") return false;
        auto itA = o.nums.find("bandsDb"), itN = o.nums.find("bandsNormDb");
        if (itA == o.nums.end() || itN == o.nums.end() || (int)itA->second.size() != kBands || (int)itN->second.size() != kBands) return false;
        f = Fingerprint(); f.version = (int)o.num("version", 1); f.name = o.str("name"); f.sampleRate = o.num("sampleRate"); f.durationSec = o.num("durationSec");
        for (int i = 0; i < kBands; ++i) { f.bandsDb[i] = (float)itA->second[i]; f.bandsNormDb[i] = (float)itN->second[i]; }
#define G(x) f.x = (float)o.num(#x, f.x)
        G(centroidHz); G(rolloffHz); G(flatness); G(tonalRatio); G(rmsDb); G(peakDb); G(truePeakDb); G(lufs); G(crestDb); G(dynamicRangeDb);
        G(transientDensity); G(stereoWidth); G(midSideDb); G(lrBalanceDb); G(correlation); G(monoCompatDb); G(reverbT60); G(reverbConfidence);
        G(echoMs); G(echoStrength); G(confidence);
#undef G
        f.valid = true; return true;
    }
    // Human-readable interpretation derived strictly from measured values
    std::vector<std::string> interpret() const {
        std::vector<std::string> r; char b[256];
        double pl = 0, pm = 0, ph = 0; for (int i = 0; i < kBands; ++i) { const double p = std::pow(10.0, bandsDb[i] / 10.0); (i < 5 ? pl : i < 11 ? pm : ph) += p; }
        const double t = pl + pm + ph + 1e-20;
        std::snprintf(b, sizeof b, "Balance espectral: graves (<170 Hz) %.0f%%, medios %.0f%%, agudos (>2.3 kHz) %.0f%% de la energía.", 100 * pl / t, 100 * pm / t, 100 * ph / t); r.push_back(b);
        std::snprintf(b, sizeof b, "Brillo: centroide %.0f Hz, roll-off 85%% en %.0f Hz.", centroidHz, rolloffHz); r.push_back(b);
        std::snprintf(b, sizeof b, "Loudness integrado %.1f LUFS, RMS %.1f dBFS, pico %.1f dBFS, true peak %.1f dBTP.", lufs, rmsDb, peakDb, truePeakDb); r.push_back(b);
        std::snprintf(b, sizeof b, "Dinámica: crest %.1f dB, rango dinámico estimado %.1f LU (%s).", crestDb, dynamicRangeDb, crestDb < 8 ? "mezcla densa/comprimida" : crestDb < 14 ? "dinámica moderada" : "dinámica abierta"); r.push_back(b);
        std::snprintf(b, sizeof b, "Estéreo: correlación %.2f, balance M/S %.1f dB, L/R %.1f dB, compatibilidad mono %.1f dB.", correlation, midSideDb, lrBalanceDb, monoCompatDb); r.push_back(b);
        std::snprintf(b, sizeof b, "Transitorios: %.1f ataques/s. Espacio: decay estimado %.2f s (confianza %.0f%%); eco dominante %.0f ms (fuerza %.2f).", transientDensity, reverbT60, 100 * reverbConfidence, echoMs, echoStrength); r.push_back(b);
        if (truePeakDb > -1.0f) r.push_back("Recomendación: true peak por encima de -1 dBTP; riesgo de picos inter-muestra al codificar.");
        if (correlation < 0.3f) r.push_back("Recomendación: baja correlación estéreo; revisa compatibilidad mono.");
        if (monoCompatDb < -3.f) r.push_back("Recomendación: pérdida de nivel al sumar a mono mayor a 3 dB.");
        if (pl / t > 0.6) r.push_back("Recomendación: predominan los graves; considera controlar la zona <170 Hz."); 
        std::snprintf(b, sizeof b, "Nivel de confianza global: %.0f%% (depende de la duración y del nivel del material analizado).", 100 * confidence); r.push_back(b);
        return r;
    }
};
}
