#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout MixProProcessor::createLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (int i = 0; i < mp::kNumParams; ++i) {
        const auto& p = mp::paramTable()[i]; const juce::ParameterID pid{ p.id, 1 };
        if (p.kind == 0) layout.add(std::make_unique<juce::AudioParameterFloat>(pid, p.name, juce::NormalisableRange<float>(p.mn, p.mx, 0.001f, p.skew), p.def));
        else if (p.kind == 1) layout.add(std::make_unique<juce::AudioParameterChoice>(pid, p.name, juce::StringArray::fromTokens(p.choices, "|", ""), (int)p.def));
        else layout.add(std::make_unique<juce::AudioParameterBool>(pid, p.name, p.def > 0.5f));
    }
    return layout;
}

MixProProcessor::MixProProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true).withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, &undo, "MIXPRO", createLayout()) {
    for (int i = 0; i < mp::kNumParams; ++i) raw_[i] = apvts.getRawParameterValue(mp::paramTable()[i].id);
    apvts.addParameterListener("mode", this);
    const auto g = dataDir().getChildFile("GenreProfiles.json");
    if (g.existsAsFile()) mp::GenreLibrary::instance().fromJson(g.loadFileAsString().toStdString());
    engine.prepare(44100.0);
}
MixProProcessor::~MixProProcessor() {
    apvts.removeParameterListener("mode", this);
    while (jobs_.load() > 0) juce::Thread::sleep(10);
}

bool MixProProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
    const auto o = l.getMainOutputChannelSet();
    return (o == juce::AudioChannelSet::mono() || o == juce::AudioChannelSet::stereo()) && l.getMainInputChannelSet() == o;
}
void MixProProcessor::prepareToPlay(double sr, int) {
    sr_ = sr; engine.prepare(sr); masterLatency_ = std::max(8, (int)(sr * 0.002));
    { const juce::SpinLock::ScopedLockType lk(refLock_); refDirty_ = true; }
    setLatencySamples(mp::kModeMaster == (int)param(mp::p_mode) ? masterLatency_ : 0);
}
void MixProProcessor::parameterChanged(const juce::String& id, float v) {
    if (id == "mode") setLatencySamples((int)std::lround(v) == mp::kModeMaster ? masterLatency_ : 0);
}
void MixProProcessor::processBlock(juce::AudioBuffer<float>& buf, juce::MidiBuffer&) {
    juce::ScopedNoDenormals nd;
    const int n = buf.getNumSamples(), nch = buf.getNumChannels();
    for (int c = getTotalNumInputChannels(); c < nch; ++c) buf.clear(c, 0, n);
    float v[mp::kNumParams]; for (int i = 0; i < mp::kNumParams; ++i) v[i] = raw_[i]->load();
    mp::ParamSet P; mp::fillParamSet(v, P);
    { const juce::SpinLock::ScopedTryLockType lk(refLock_);
      if (lk.isLocked() && refDirty_) { engine.setReference(ref_.bandsNormDb, ref_.lufs, refValid_); refDirty_ = false; } }
    double bpm = 0.0;
    if (auto* ph = getPlayHead()) if (auto pos = ph->getPosition()) if (auto b = pos->getBpm()) bpm = *b;
    engine.process(buf.getWritePointer(0), nch > 1 ? buf.getWritePointer(1) : nullptr, n, P, bpm);
}
juce::AudioProcessorEditor* MixProProcessor::createEditor() { return new MixProEditor(*this); }

// ---------------- state ----------------
void MixProProcessor::getStateInformation(juce::MemoryBlock& dest) {
    auto state = apvts.copyState(); state.setProperty("refJson", referenceJson(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary(*xml, dest);
}
void MixProProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size)) if (xml->hasTagName(apvts.state.getType())) {
        auto vt = juce::ValueTree::fromXml(*xml); const juce::String rj = vt.getProperty("refJson").toString();
        apvts.replaceState(vt); if (rj.isNotEmpty()) setReferenceFromJson(rj); else clearReference();
    }
}

// ---------------- reference ----------------
void MixProProcessor::setStatus(const juce::String& s) { const juce::ScopedLock sl(uiLock_); status_ = s; }
juce::String MixProProcessor::getStatus() const { const juce::ScopedLock sl(uiLock_); return status_; }
mp::Fingerprint MixProProcessor::getReference() const { const juce::SpinLock::ScopedLockType lk(refLock_); return ref_; }
std::vector<float> MixProProcessor::getWaveform() const { const juce::ScopedLock sl(uiLock_); return wave_; }
juce::String MixProProcessor::referenceJson() const {
    const juce::SpinLock::ScopedLockType lk(refLock_); return refValid_ ? juce::String(ref_.toJson()) : juce::String();
}
void MixProProcessor::commitReference(const mp::Fingerprint& fp, std::vector<float> wave) {
    { const juce::SpinLock::ScopedLockType lk(refLock_); ref_ = fp; refValid_ = fp.valid; refDirty_ = true; }
    { const juce::ScopedLock sl(uiLock_); wave_ = std::move(wave); }
    ++refVersion_;
}
void MixProProcessor::clearReference() { mp::Fingerprint f; commitReference(f, {}); }
bool MixProProcessor::setReferenceFromJson(const juce::String& json) {
    mp::Fingerprint f; if (!mp::Fingerprint::fromJson(json.toStdString(), f)) { setStatus("Invalid fingerprint JSON."); return false; }
    commitReference(f, {}); setStatus("Fingerprint loaded: " + juce::String(f.name)); return true;
}
void MixProProcessor::loadReferenceFile(const juce::File& file) {
    ++jobs_; setStatus("Analysing " + file.getFileName() + " ...");
    juce::Thread::launch([this, file] {
        juce::AudioFormatManager fm; fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> rd(fm.createReaderFor(file));
        if (rd == nullptr || rd->lengthInSamples < 1) setStatus("Could not read audio file (supported: WAV, AIFF, FLAC, OGG).");
        else {
            const int ch = (int)std::min<unsigned>(2, rd->numChannels); const int n = (int)std::min<juce::int64>(rd->lengthInSamples, (juce::int64)(rd->sampleRate * 600.0));
            juce::AudioBuffer<float> b(ch, n);
            rd->read(&b, 0, n, 0, true, ch > 1);
            mp::Fingerprint fp;
            if (mp::analyzeReference(b.getReadPointer(0), ch > 1 ? b.getReadPointer(1) : nullptr, (size_t)n, rd->sampleRate, file.getFileNameWithoutExtension().toStdString(), fp)) {
                std::vector<float> w(1200); const int per = std::max(1, n / 600);
                for (int i = 0; i < 600; ++i) { float mn = 0, mx = 0; for (int k = 0; k < per && i * per + k < n; ++k) { const float s = b.getSample(0, i * per + k); mn = std::min(mn, s); mx = std::max(mx, s); } w[2 * i] = mn; w[2 * i + 1] = mx; }
                commitReference(fp, std::move(w)); setStatus("Reference analysed: " + file.getFileName());
            } else setStatus("Audio too short to analyse (minimum 1 second).");
        }
        --jobs_;
    });
}

// ---------------- presets ----------------
const std::vector<mp::PresetData>& MixProProcessor::factoryList() { static const auto l = mp::factoryPresets(); return l; }
void MixProProcessor::applyPreset(const mp::PresetData& p) {
    float v[mp::kNumParams]; mp::defaultValues(v); p.apply(v); undo.beginNewTransaction();
    for (int i = 0; i < mp::kNumParams; ++i) {
        if (i == mp::p_bypass || i == mp::p_exportBits || i == mp::p_exportSr || i == mp::p_exportMono) continue;
        if (auto* prm = apvts.getParameter(mp::paramTable()[i].id)) prm->setValueNotifyingHost(prm->convertTo0to1(v[i]));
    }
}
juce::File MixProProcessor::dataDir() const {
    auto d = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("ORYMY").getChildFile("MIX PRO"); d.createDirectory(); return d;
}
juce::File MixProProcessor::userPresetDir() const { auto d = dataDir().getChildFile("Presets"); d.createDirectory(); return d; }
bool MixProProcessor::saveUserPreset(const juce::String& name) {
    const auto clean = juce::File::createLegalFileName(name.trim()); if (clean.isEmpty()) return false;
    float v[mp::kNumParams]; for (int i = 0; i < mp::kNumParams; ++i) v[i] = raw_[i]->load();
    const auto p = mp::PresetData::fromValues(clean.toStdString(), "USER", v);
    return userPresetDir().getChildFile(clean + ".mixpro").replaceWithText(juce::String(p.toJson()));
}
bool MixProProcessor::loadPresetFile(const juce::File& f) {
    mp::PresetData p; if (!mp::PresetData::fromJson(f.loadFileAsString().toStdString(), p)) return false; applyPreset(p); return true;
}
juce::Array<juce::File> MixProProcessor::listUserPresets() const {
    auto l = userPresetDir().findChildFiles(juce::File::findFiles, false, "*.mixpro");
    const auto shared = juce::File::getSpecialLocation(juce::File::commonApplicationDataDirectory).getChildFile("ORYMY").getChildFile("MIX PRO").getChildFile("Presets");
    if (shared.isDirectory()) l.addArray(shared.findChildFiles(juce::File::findFiles, true, "*.mixpro"));
    return l;
}
bool MixProProcessor::exportGenreProfiles(const juce::File& f) const { return f.replaceWithText(juce::String(mp::GenreLibrary::instance().toJson())); }
bool MixProProcessor::importGenreProfiles(const juce::File& f) {
    if (!mp::GenreLibrary::instance().fromJson(f.loadFileAsString().toStdString())) return false;
    dataDir().getChildFile("GenreProfiles.json").replaceWithText(juce::String(mp::GenreLibrary::instance().toJson())); return true;
}

// ---------------- A/B ----------------
void MixProProcessor::toggleAB() {
    ab_[abIdx_] = apvts.copyState(); abIdx_ ^= 1;
    if (!ab_[abIdx_].isValid()) ab_[abIdx_] = apvts.copyState();
    apvts.replaceState(ab_[abIdx_].createCopy());
}

// ---------------- offline render / export ----------------
void MixProProcessor::renderFile(const juce::File& inFile, const juce::File& outFile) {
    float v[mp::kNumParams]; for (int i = 0; i < mp::kNumParams; ++i) v[i] = raw_[i]->load();
    mp::ParamSet P; mp::fillParamSet(v, P); const auto fp = getReference(); const bool hasRef = fp.valid;
    ++jobs_; setStatus("Rendering " + inFile.getFileName() + " ...");
    juce::Thread::launch([this, inFile, outFile, P, fp, hasRef] {
        juce::AudioFormatManager fm; fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> rd(fm.createReaderFor(inFile));
        if (rd == nullptr) { setStatus("Render failed: cannot read input."); --jobs_; return; }
        const int ch = (int)std::min<unsigned>(2, rd->numChannels), n = (int)rd->lengthInSamples; const double sr = rd->sampleRate;
        mp::MixEngine eng; eng.prepare(sr); eng.setReference(fp.bandsNormDb, fp.lufs, hasRef);
        const int lat = eng.latencySamples(P); const int total = n + lat;
        juce::AudioBuffer<float> b(2, total); b.clear(); rd->read(&b, 0, n, 0, true, ch > 1);
        if (ch == 1) b.copyFrom(1, 0, b, 0, 0, n);
        for (int pos = 0; pos < total; pos += 2048) { const int c = std::min(2048, total - pos); eng.process(b.getWritePointer(0) + pos, ch > 1 ? b.getWritePointer(1) + pos : nullptr, c, P, 0.0); }
        juce::AudioBuffer<float> o(ch, n); for (int c = 0; c < ch; ++c) o.copyFrom(c, 0, b, c, lat, n);
        static const double rates[5] = {0, 44100, 48000, 88200, 96000}; const double outSr = P.exportSr == 0 ? sr : rates[juce::jlimit(0, 4, P.exportSr)];
        if (std::abs(outSr - sr) > 1.0) {
            const int m = (int)(n * outSr / sr); juce::AudioBuffer<float> rs(ch, m);
            for (int c = 0; c < ch; ++c) { juce::LagrangeInterpolator li; li.process(sr / outSr, o.getReadPointer(c), rs.getWritePointer(c), m); }
            o.makeCopyOf(rs);
        }
        int outCh = ch; if (P.exportMono && ch == 2) { for (int i = 0; i < o.getNumSamples(); ++i) o.setSample(0, i, 0.5f * (o.getSample(0, i) + o.getSample(1, i))); outCh = 1; }
        const int bits = P.exportBits == 0 ? 16 : P.exportBits == 1 ? 24 : 32;
        outFile.deleteFile(); std::unique_ptr<juce::FileOutputStream> os(outFile.createOutputStream());
        juce::WavAudioFormat wav; std::unique_ptr<juce::AudioFormatWriter> wr;
        if (os != nullptr) { wr.reset(wav.createWriterFor(os.get(), outSr, (unsigned)outCh, bits, {}, 0)); if (wr != nullptr) os.release(); }
        if (wr == nullptr) setStatus("Render failed: cannot create output file.");
        else { wr->writeFromAudioSampleBuffer(o, 0, o.getNumSamples()); wr.reset(); setStatus("Rendered: " + outFile.getFullPathName()); }
        --jobs_;
    });
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MixProProcessor(); }
