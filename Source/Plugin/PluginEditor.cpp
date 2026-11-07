#include "PluginEditor.h"
using namespace ui;

// ======================= VisualPanel =======================
static float freqToX(double f, float x0, float w) { return x0 + w * (float)(std::log(f / 20.0) / std::log(1000.0)); }

void VisualPanel::paint(juce::Graphics& g) {
    auto b = getLocalBounds().toFloat(); g.setColour(panel); g.fillRoundedRectangle(b, 6.f);
    auto spec = b.removeFromLeft(b.getWidth() * 0.58f).reduced(8.f); auto rest = b.reduced(8.f);
    auto meters = rest.removeFromLeft(rest.getWidth() * 0.5f); drawSpectrum(g, spec); drawMeters(g, meters); drawScope(g, rest);
}
void VisualPanel::drawSpectrum(juce::Graphics& g, juce::Rectangle<float> r) {
    auto chain = r.removeFromBottom(30.f);
    g.setColour(juce::Colour(0xff101012)); g.fillRoundedRectangle(r, 4.f);
    g.setColour(line); for (double f : {50., 100., 200., 500., 1000., 2000., 5000., 10000.}) { const float x = freqToX(f, r.getX(), r.getWidth()); g.drawVerticalLine((int)x, r.getY(), r.getBottom()); }
    g.setColour(dim); g.setFont(juce::FontOptions(10.f));
    for (double f : {100., 1000., 10000.}) g.drawText(f >= 1000 ? juce::String((int)(f / 1000)) + "k" : juce::String((int)f), (int)freqToX(f, r.getX(), r.getWidth()) + 2, (int)r.getBottom() - 12, 30, 10, juce::Justification::left);
    auto& m = proc.engine.meters;
    auto curve = [&](std::atomic<float>* a, juce::Colour c, float th, bool fill) {
        juce::Path p; for (int i = 0; i < mp::Meters::kSpec; ++i) {
            const float db = juce::jlimit(-120.f, -30.f, a[i].load()); const float x = r.getX() + r.getWidth() * i / (float)(mp::Meters::kSpec - 1), y = r.getBottom() - (db + 120.f) / 90.f * r.getHeight();
            if (i == 0) p.startNewSubPath(x, y); else p.lineTo(x, y); }
        if (fill) { juce::Path f(p); f.lineTo(r.getRight(), r.getBottom()); f.lineTo(r.getX(), r.getBottom()); f.closeSubPath(); g.setColour(c.withAlpha(0.10f)); g.fillPath(f); }
        g.setColour(c); g.strokePath(p, juce::PathStrokeType(th)); };
    curve(m.specIn, dim, 1.f, false); curve(m.specOut, text, 1.5f, true);
    // EQ response (base EQ + match filters), centre line = 0 dB, +-18 dB
    const double sr = proc.getSampleRate() > 0 ? proc.getSampleRate() : 44100.0; const float e0 = m.eq[0], e1 = m.eq[1];
    juce::Path eq; const float cy = r.getCentreY();
    for (int i = 0; i < 160; ++i) {
        const double f = 20.0 * std::pow(1000.0, i / 159.0); double d = 0;
        if (m.eq[10] > 21.f) d += 2.0 * mp::magnitudeDb(mp::makeBiquad(mp::FilterType::HighPass, sr, m.eq[10], 0.7071), f, sr) * 0.5;
        d += mp::magnitudeDb(mp::makeBiquad(mp::FilterType::LowShelf, sr, e0, 0.7071, e1), f, sr) + mp::magnitudeDb(mp::makeBiquad(mp::FilterType::Peak, sr, m.eq[2], m.eq[4], m.eq[3]), f, sr)
           + mp::magnitudeDb(mp::makeBiquad(mp::FilterType::Peak, sr, m.eq[5], m.eq[7], m.eq[6]), f, sr) + mp::magnitudeDb(mp::makeBiquad(mp::FilterType::HighShelf, sr, m.eq[8], 0.7071, m.eq[9]), f, sr);
        for (int k = 0; k < mp::kBands; ++k) { const float gk = m.match[k].load(); if (std::fabs(gk) > 0.05f) d += mp::magnitudeDb(mp::makeBiquad(mp::FilterType::Peak, sr, mp::bandCenter(k), 1.8, gk), f, sr); }
        const float x = r.getX() + r.getWidth() * i / 159.f, y = cy - juce::jlimit(-18.f, 18.f, (float)d) / 18.f * r.getHeight() * 0.45f;
        if (i == 0) eq.startNewSubPath(x, y); else eq.lineTo(x, y); }
    g.setColour(accent); g.strokePath(eq, juce::PathStrokeType(1.6f));
    g.setColour(dim); g.setFont(juce::FontOptions(10.f)); g.drawText("SPECTRUM (in grey / out white)  ·  EQ + MATCH CURVE (amber, ±18 dB)", r.toNearestInt().removeFromTop(14).reduced(6, 0), juce::Justification::left);
    g.setColour(text); g.setFont(juce::FontOptions(11.f)); g.drawFittedText(chainText(), chain.toNearestInt(), juce::Justification::centredLeft, 2);
}
juce::String VisualPanel::chainText() const {
    float v[mp::kNumParams]; for (int i = 0; i < mp::kNumParams; ++i) v[i] = proc.param(i);
    mp::ParamSet P; mp::fillParamSet(v, P); mp::Effective E; mp::computeEffective(P, 120.0, E); auto& m = proc.engine.meters;
    juce::String s = "CHAIN: IN"; if (E.hpf > 21.f) s << " > HPF";
    if (std::fabs(E.lsG) + std::fabs(E.p1G) + std::fabs(E.p2G) + std::fabs(E.hsG) > 0.1f) s << " > EQ";
    if (E.kind != mp::kMatchNone) s << (E.kind == mp::kMatchBump ? " > AUTO-EQ" : " > MATCH");
    if (E.ratio > 1.01f) s << " > COMP(" << juce::String(m.compGrDb.load(), 1) << " dB)"; if (E.mbAmt > 0.001f) s << " > MULTIBAND"; if (E.glue > 0.001f) s << " > GLUE";
    if (std::fabs(E.tAtk) + std::fabs(E.tSus) > 0.01f) s << " > TRANSIENT"; if (E.sat > 0.001f) s << " > SAT"; if (E.deEss > 0.001f) s << " > DE-ESS";
    if (std::fabs(E.width - 1.f) > 0.01f || std::fabs(E.balance) > 0.01f || E.monoLow > 20.f) s << " > STEREO"; if (E.dlyMix > 0.001f) s << " > DELAY"; if (E.revMix > 0.001f) s << " > REVERB";
    if (E.matchLoud) s << " > LOUDNESS(" << juce::String(m.loudGainDb.load(), 1) << " dB)"; if (E.master) s << " > LIMITER(-" << juce::String(m.limGrDb.load(), 1) << " dB)";
    return s << " > OUT   [role: " << juce::String(mp::roleTable()[E.role].name) << "]";
}
void VisualPanel::drawMeters(juce::Graphics& g, juce::Rectangle<float> r) {
    auto& m = proc.engine.meters; g.setColour(juce::Colour(0xff101012)); g.fillRoundedRectangle(r, 4.f);
    auto bars = r.reduced(8.f); auto txt = bars.removeFromRight(bars.getWidth() * 0.5f); const float bw = bars.getWidth() / 4.f;
    auto bar = [&](int i, const char* name, float v, float lo, float hi, bool down) {
        auto c = juce::Rectangle<float>(bars.getX() + i * bw + 4, bars.getY() + 4, bw - 10, bars.getHeight() - 24); g.setColour(line); g.fillRect(c);
        const float t = juce::jlimit(0.f, 1.f, (v - lo) / (hi - lo)); g.setColour(down ? accent : text);
        if (down) g.fillRect(c.withHeight(c.getHeight() * t)); else g.fillRect(c.withTop(c.getBottom() - c.getHeight() * t));
        g.setColour(dim); g.setFont(juce::FontOptions(10.f)); g.drawText(name, (int)c.getX() - 4, (int)c.getBottom() + 2, (int)c.getWidth() + 8, 12, juce::Justification::centred); };
    bar(0, "IN", m.inPeakDb, -60.f, 0.f, false); bar(1, "OUT", m.outPeakDb, -60.f, 0.f, false); bar(2, "GR", m.compGrDb, 0.f, 18.f, true); bar(3, "LIM", m.limGrDb, 0.f, 12.f, true);
    g.setColour(text); g.setFont(juce::FontOptions(12.f)); auto t = txt.reduced(4.f);
    auto line1 = [&](const juce::String& k, const juce::String& v) { auto row = t.removeFromTop(20.f); g.setColour(dim); g.drawText(k, row.toNearestInt(), juce::Justification::left); g.setColour(text); g.drawText(v, row.toNearestInt(), juce::Justification::right); };
    auto f1 = [](float x) { return x <= -100.f ? juce::String("-inf") : juce::String(x, 1); };
    line1("LUFS M", f1(m.momentary)); line1("LUFS S", f1(m.shortTerm)); line1("LUFS I", f1(m.integrated)); line1("True peak", f1(m.truePeakDb) + " dBTP");
    line1("Peak out", f1(m.outPeakDb) + " dB"); line1("Loudness gain", juce::String(m.loudGainDb.load(), 1) + " dB");
}
void VisualPanel::drawScope(juce::Graphics& g, juce::Rectangle<float> r) {
    auto& m = proc.engine.meters; g.setColour(juce::Colour(0xff101012)); g.fillRoundedRectangle(r, 4.f);
    auto sq = r.reduced(6.f); sq.removeFromBottom(18.f); const float s = std::min(sq.getWidth(), sq.getHeight()); sq = sq.withSizeKeepingCentre(s, s);
    g.setColour(line); g.drawRect(sq); g.drawLine(sq.getX(), sq.getCentreY(), sq.getRight(), sq.getCentreY()); g.drawLine(sq.getCentreX(), sq.getY(), sq.getCentreX(), sq.getBottom());
    g.setColour(accent.withAlpha(0.7f));
    for (int i = 0; i < mp::Meters::kScope; ++i) { const float L = m.scopeL[i], R = m.scopeR[i]; const float x = sq.getCentreX() + (L - R) * 0.707f * s * 0.5f * 1.6f, y = sq.getCentreY() - (L + R) * 0.707f * s * 0.5f * 1.6f;
        g.fillRect(juce::jlimit(sq.getX(), sq.getRight(), x), juce::jlimit(sq.getY(), sq.getBottom(), y), 1.5f, 1.5f); }
    const float c = juce::jlimit(-1.f, 1.f, m.correlation.load()); auto cb = r.reduced(8.f).removeFromBottom(12.f);
    g.setColour(line); g.fillRect(cb); g.setColour(c >= 0 ? text : juce::Colours::indianred); g.fillRect(juce::Rectangle<float>(cb.getCentreX(), cb.getY(), cb.getWidth() * 0.5f * c, cb.getHeight()));
    g.setColour(dim); g.setFont(juce::FontOptions(10.f)); g.drawText("VECTORSCOPE · CORRELATION " + juce::String(c, 2), r.toNearestInt().removeFromTop(14).reduced(6, 0), juce::Justification::left);
}

// ======================= ParamPanel =======================
ParamPanel::ParamPanel(MixProProcessor& p, const std::vector<const char*>& ids) {
    for (auto* id : ids) {
        const int idx = mp::findParam(id); if (idx < 0) continue; const auto& info = mp::paramTable()[idx]; Cell c; c.kind = info.kind;
        c.label = std::make_unique<juce::Label>(); c.label->setText(info.name, juce::dontSendNotification); c.label->setJustificationType(juce::Justification::centred); c.label->setColour(juce::Label::textColourId, dim);
        c.label->setFont(juce::FontOptions(12.f)); addAndMakeVisible(*c.label);
        if (info.kind == 0) { auto s = std::make_unique<juce::Slider>(juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow); s->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 74, 16);
            s->onDragStart = [&p] { p.undo.beginNewTransaction(); }; sa.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, id, *s)); c.ctl = std::move(s); }
        else if (info.kind == 1) { auto cb = std::make_unique<juce::ComboBox>(); cb->addItemList(juce::StringArray::fromTokens(info.choices, "|", ""), 1);
            ca.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.apvts, id, *cb)); c.ctl = std::move(cb); }
        else { auto tb = std::make_unique<juce::ToggleButton>("On"); ba.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts, id, *tb)); c.ctl = std::move(tb); }
        addAndMakeVisible(*c.ctl); cells.push_back(std::move(c));
    }
}
void ParamPanel::resized() {
    if (cells.empty()) return; const int cw = 128, ch = 112, cols = std::max(1, getWidth() / cw), pad = (getWidth() - cols * cw) / 2;
    for (size_t i = 0; i < cells.size(); ++i) {
        const int x = pad + (int)(i % cols) * cw, y = (int)(i / cols) * ch; auto& c = cells[i]; c.label->setBounds(x, y, cw, 16);
        if (c.kind == 0) c.ctl->setBounds(x + 8, y + 16, cw - 16, ch - 18);
        else if (c.kind == 1) c.ctl->setBounds(x + 6, y + 32, cw - 12, 26); else c.ctl->setBounds(x + 30, y + 32, cw - 40, 26);
    }
}

// ======================= RefTools =======================
static juce::String buildReport(const mp::Fingerprint& f) {
    juce::String s; s << "REFERENCE FINGERPRINT: " << juce::String(f.name) << "\n";
    s << juce::String::formatted("Duration %.1f s @ %.0f Hz   |   Confidence %.0f%%\n\n", f.durationSec, f.sampleRate, f.confidence * 100.0);
    s << juce::String::formatted("LEVELS   LUFS %.1f | RMS %.1f dBFS | Peak %.1f dBFS | True peak %.1f dBTP | Crest %.1f dB | DR %.1f LU\n", f.lufs, f.rmsDb, f.peakDb, f.truePeakDb, f.crestDb, f.dynamicRangeDb);
    s << juce::String::formatted("SPECTRUM Centroid %.0f Hz | Rolloff(85%%) %.0f Hz | Flatness %.4f | Tonal ratio %.2f\n", f.centroidHz, f.rolloffHz, f.flatness, f.tonalRatio);
    s << juce::String::formatted("STEREO   Width %.2f | M/S %.1f dB | L/R %.1f dB | Correlation %.2f | Mono compat %.1f dB\n", f.stereoWidth, f.midSideDb, f.lrBalanceDb, f.correlation, f.monoCompatDb);
    s << juce::String::formatted("TIME     Transients %.1f /s | Decay T60 %.2f s (conf. %.0f%%) | Echo %.0f ms (strength %.2f)\n\n", f.transientDensity, f.reverbT60, f.reverbConfidence * 100.0, f.echoMs, f.echoStrength);
    s << "BANDS (relative dB):\n"; for (int b = 0; b < mp::kBands; ++b) s << juce::String::formatted("  %6.0f Hz  %+5.1f dB\n", mp::bandCenter(b), f.bandsNormDb[b]);
    s << "\nINTERPRETATION / RECOMMENDATIONS\n"; for (auto& l : f.interpret()) s << "- " << juce::String(l) << "\n"; return s;
}
RefTools::RefTools(MixProProcessor& p, bool c, bool lufs) : proc(p), compact(c), offerLufs(lufs) {
    for (auto* b : { &loadBtn, &exportBtn, &importBtn, &clearBtn }) addAndMakeVisible(*b); if (offerLufs) addAndMakeVisible(lufsBtn);
    addAndMakeVisible(status); status.setColour(juce::Label::textColourId, accent); status.setFont(juce::FontOptions(12.f));
    report.setMultiLine(true); report.setReadOnly(true); report.setFont(juce::Font(juce::FontOptions(13.f, juce::Font::plain))); if (!compact) addAndMakeVisible(report);
    loadBtn.onClick = [this] { chooser = std::make_unique<juce::FileChooser>("Select reference audio", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.ogg");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this](const juce::FileChooser& fc) { auto f = fc.getResult(); if (f.existsAsFile()) proc.loadReferenceFile(f); }); };
    exportBtn.onClick = [this] { auto j = proc.referenceJson(); if (j.isEmpty()) return; chooser = std::make_unique<juce::FileChooser>("Export fingerprint", proc.dataDir(), "*.json");
        chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting, [this, j](const juce::FileChooser& fc) { auto f = fc.getResult(); if (f != juce::File()) f.replaceWithText(j); }); };
    importBtn.onClick = [this] { chooser = std::make_unique<juce::FileChooser>("Import fingerprint", proc.dataDir(), "*.json");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this](const juce::FileChooser& fc) { auto f = fc.getResult(); if (f.existsAsFile()) proc.setReferenceFromJson(f.loadFileAsString()); }); };
    clearBtn.onClick = [this] { proc.clearReference(); };
    lufsBtn.onClick = [this] { auto f = proc.getReference(); if (f.valid) if (auto* prm = proc.apvts.getParameter("mTargetLufs")) prm->setValueNotifyingHost(prm->convertTo0to1(f.lufs)); };
    startTimerHz(4);
}
void RefTools::resized() {
    auto r = getLocalBounds(); auto row = r.removeFromTop(28); const int bw = 170;
    loadBtn.setBounds(row.removeFromLeft(bw)); row.removeFromLeft(4); exportBtn.setBounds(row.removeFromLeft(bw)); row.removeFromLeft(4); importBtn.setBounds(row.removeFromLeft(bw)); row.removeFromLeft(4); clearBtn.setBounds(row.removeFromLeft(60));
    if (offerLufs) { row.removeFromLeft(4); lufsBtn.setBounds(row.removeFromLeft(210)); }
    status.setBounds(r.removeFromTop(22)); r.removeFromTop(70); if (!compact) report.setBounds(r.reduced(0, 4));
}
void RefTools::paint(juce::Graphics& g) {
    auto r = getLocalBounds().withTrimmedTop(50).withHeight(66).toFloat(); g.setColour(juce::Colour(0xff101012)); g.fillRoundedRectangle(r, 4.f);
    auto w = proc.getWaveform(); g.setColour(accent);
    if (w.size() >= 1200) for (int i = 0; i < 600; ++i) { const float x = r.getX() + r.getWidth() * i / 600.f, cy = r.getCentreY(), h = r.getHeight() * 0.5f;
        g.drawLine(x, cy - w[2 * i + 1] * h, x, cy - w[2 * i] * h, 1.f); }
    else { g.setColour(dim); g.setFont(juce::FontOptions(12.f)); g.drawText(proc.referenceVersion() > 0 && proc.getReference().valid ? "Fingerprint loaded (no waveform)" : "No reference loaded", r.toNearestInt(), juce::Justification::centred); }
}
void RefTools::timerCallback() {
    status.setText(proc.getStatus(), juce::dontSendNotification);
    if (lastVersion != proc.referenceVersion()) { lastVersion = proc.referenceVersion(); auto f = proc.getReference(); report.setText(f.valid ? buildReport(f) : juce::String("No reference loaded.\nLoad a WAV / AIFF / FLAC file (legally obtained) or import a fingerprint JSON."), false); repaint(); }
}

// ======================= Pages =======================
namespace {
struct InfoPage : juce::Component {
    InfoPage(MixProProcessor& p, const juce::String& info, const std::vector<const char*>& ids, int refMode /*0 none,1 full,2 compact+lufs*/) : panel(p, ids) {
        text.setText(info, juce::dontSendNotification); text.setFont(juce::FontOptions(13.f)); text.setColour(juce::Label::textColourId, dim); text.setJustificationType(juce::Justification::topLeft); addAndMakeVisible(text);
        count = (int)ids.size(); addAndMakeVisible(panel); if (refMode) { ref = std::make_unique<RefTools>(p, refMode == 2, refMode == 2); addAndMakeVisible(*ref); }
    }
    void resized() override {
        auto r = getLocalBounds(); text.setBounds(r.removeFromTop(38)); const int cols = std::max(1, getWidth() / 128); const int rows = (panelCount() + cols - 1) / cols;
        panel.setBounds(r.removeFromTop(std::max(1, rows) * 112)); if (ref) ref->setBounds(r.reduced(0, 4));
    }
    int panelCount() const { return count; }
    juce::Label text; ParamPanel panel; std::unique_ptr<RefTools> ref; int count = 0;
};
std::unique_ptr<juce::Component> makeInfo(MixProProcessor& p, const juce::String& info, std::vector<const char*> ids, int refMode = 0) {
    return std::make_unique<InfoPage>(p, info, ids, refMode);
}

struct GenrePage : InfoPage {
    GenrePage(MixProProcessor& p) : InfoPage(p, "GENRE ENGINE - 34 editable genre profiles (low / high / presence / compression / saturation / space / width). Select a genre; use GENRE MIX, VOCAL MIX or INSTRUMENT MIX mode to apply it.", {"genre", "style", "role", "amount"}, 0), proc(p) {
        exp.setButtonText("Export genre profiles JSON"); imp.setButtonText("Import genre profiles JSON"); addAndMakeVisible(exp); addAndMakeVisible(imp); addAndMakeVisible(msg);
        msg.setColour(juce::Label::textColourId, accent);
        exp.onClick = [this] { chooser = std::make_unique<juce::FileChooser>("Export genre profiles", proc.dataDir(), "*.json");
            chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting, [this](const juce::FileChooser& fc) { auto f = fc.getResult(); if (f != juce::File()) msg.setText(proc.exportGenreProfiles(f) ? "Exported." : "Export failed.", juce::dontSendNotification); }); };
        imp.onClick = [this] { chooser = std::make_unique<juce::FileChooser>("Import genre profiles", proc.dataDir(), "*.json");
            chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this](const juce::FileChooser& fc) { auto f = fc.getResult(); if (f.existsAsFile()) msg.setText(proc.importGenreProfiles(f) ? "Imported and saved as your default profiles." : "Invalid genre file.", juce::dontSendNotification); }); };
    }
    void resized() override { InfoPage::resized(); auto r = getLocalBounds().removeFromBottom(120); exp.setBounds(r.removeFromTop(28).removeFromLeft(240)); r.removeFromTop(6); imp.setBounds(r.removeFromTop(28).removeFromLeft(240)); msg.setBounds(r.removeFromTop(24)); }
    MixProProcessor& proc; juce::TextButton exp, imp; juce::Label msg; std::unique_ptr<juce::FileChooser> chooser;
};

struct PresetPage : juce::Component, juce::ListBoxModel, juce::Timer {
    PresetPage(MixProProcessor& p) : proc(p) {
        addAndMakeVisible(list); list.setModel(this); list.setRowHeight(24); addAndMakeVisible(nameBox); nameBox.setTextToShowWhenEmpty("Preset name...", dim);
        saveBtn.setButtonText("Save current as user preset"); loadBtn.setButtonText("Load .mixpro file..."); addAndMakeVisible(saveBtn); addAndMakeVisible(loadBtn); addAndMakeVisible(msg); msg.setColour(juce::Label::textColourId, accent);
        saveBtn.onClick = [this] { msg.setText(proc.saveUserPreset(nameBox.getText()) ? "Saved to " + proc.userPresetDir().getFullPathName() : "Could not save (empty name?)", juce::dontSendNotification); rebuild(); };
        loadBtn.onClick = [this] { chooser = std::make_unique<juce::FileChooser>("Load preset", proc.userPresetDir(), "*.mixpro");
            chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this](const juce::FileChooser& fc) { auto f = fc.getResult(); if (f.existsAsFile()) msg.setText(proc.loadPresetFile(f) ? "Preset loaded." : "Invalid preset file.", juce::dontSendNotification); }); };
        rebuild();
    }
    void rebuild() { rows.clear(); for (auto& f : MixProProcessor::factoryList()) rows.push_back({ "[" + juce::String(f.category) + "]  " + juce::String(f.name), &f, {} });
        for (auto& f : proc.listUserPresets()) rows.push_back({ "[USER]  " + f.getFileNameWithoutExtension(), nullptr, f }); list.updateContent(); list.repaint(); }
    int getNumRows() override { return (int)rows.size(); }
    void paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool sel) override {
        if (row < 0 || row >= (int)rows.size()) return; if (sel) g.fillAll(accent.darker(0.5f)); g.setColour(text); g.setFont(juce::FontOptions(14.f)); g.drawText(rows[(size_t)row].label, 8, 0, w - 8, h, juce::Justification::left); }
    void listBoxItemClicked(int row, const juce::MouseEvent&) override { if (row < 0 || row >= (int)rows.size()) return; auto& r = rows[(size_t)row]; if (r.preset) proc.applyPreset(*r.preset); else proc.loadPresetFile(r.file); }
    void timerCallback() override {}
    void resized() override { auto r = getLocalBounds(); auto bot = r.removeFromBottom(70); list.setBounds(r.reduced(0, 4)); auto row = bot.removeFromTop(28); nameBox.setBounds(row.removeFromLeft(240)); row.removeFromLeft(6); saveBtn.setBounds(row.removeFromLeft(230)); row.removeFromLeft(6); loadBtn.setBounds(row.removeFromLeft(170)); msg.setBounds(bot); }
    struct Row { juce::String label; const mp::PresetData* preset; juce::File file; };
    MixProProcessor& proc; juce::ListBox list; juce::TextEditor nameBox; juce::TextButton saveBtn, loadBtn; juce::Label msg; std::vector<Row> rows; std::unique_ptr<juce::FileChooser> chooser;
};

struct SettingsPage : InfoPage {
    SettingsPage(MixProProcessor& p) : InfoPage(p, "MANUAL ENGINEERING (set mode to MANUAL ENGINEERING: the compressor values below are used directly; EQ values are offsets in every mode)  |  EXPORT / OFFLINE RENDER", 
        {"manHpf", "manLow", "manLowMid", "manHighMid", "manHigh", "manThr", "manRatio", "manAtt", "manRel", "manMakeup", "exportBits", "exportSr", "exportMono"}, 0), proc(p) {
        renderBtn.setButtonText("Render audio file with current settings..."); addAndMakeVisible(renderBtn); addAndMakeVisible(msg); msg.setColour(juce::Label::textColourId, accent);
        renderBtn.onClick = [this] { chooser = std::make_unique<juce::FileChooser>("Select audio to process", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.ogg");
            chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this](const juce::FileChooser& fc) { auto in = fc.getResult(); if (!in.existsAsFile()) return;
                chooser2 = std::make_unique<juce::FileChooser>("Save processed WAV", in.getSiblingFile(in.getFileNameWithoutExtension() + "_MIXPRO.wav"), "*.wav");
                chooser2->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting, [this, in](const juce::FileChooser& fc2) { auto out = fc2.getResult(); if (out != juce::File()) proc.renderFile(in, out); }); }); };
        startTimerHz(4);
    }
    void resized() override { InfoPage::resized(); auto r = getLocalBounds().removeFromBottom(70); renderBtn.setBounds(r.removeFromTop(28).removeFromLeft(330)); msg.setBounds(r.removeFromTop(24)); }
    void timerCallback() { msg.setText(proc.getStatus(), juce::dontSendNotification); }
    struct T : juce::Timer { std::function<void()> f; void timerCallback() override { f(); } };
    MixProProcessor& proc; juce::TextButton renderBtn; juce::Label msg; std::unique_ptr<juce::FileChooser> chooser, chooser2;
    void startTimerHz(int hz) { t.f = [this] { timerCallback(); }; t.startTimerHz(hz); } T t;
};
}

// ======================= Editor =======================
static const char* kTabNames[10] = {"HOME", "AUTO MIX", "ERA MATCH", "REFERENCE ANALYZER", "GENRE ENGINE", "VOCAL ENGINE", "INSTRUMENT ENGINE", "MASTER MATCH", "PRESET LIBRARY", "SETTINGS"};

MixProEditor::MixProEditor(MixProProcessor& p) : juce::AudioProcessorEditor(&p), proc(p), visual(p) {
    setLookAndFeel(&lnf); setSize(1120, 790); addAndMakeVisible(visual);
    for (int i = 0; i < 10; ++i) { tabs[i].setButtonText(kTabNames[i]); tabs[i].setClickingTogglesState(true); tabs[i].setRadioGroupId(77); tabs[i].onClick = [this, i] { showPage(i); }; addAndMakeVisible(tabs[i]); }
    modeBox.addItemList(juce::StringArray::fromTokens(MP_MODES, "|", ""), 1); modeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.apvts, "mode", modeBox); addAndMakeVisible(modeBox);
    bypassAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts, "bypass", bypassBtn); addAndMakeVisible(bypassBtn);
    for (auto* b : { &undoBtn, &redoBtn, &abBtn }) addAndMakeVisible(*b);
    undoBtn.onClick = [this] { proc.undo.undo(); }; redoBtn.onClick = [this] { proc.undo.redo(); }; abBtn.onClick = [this] { proc.toggleAB(); abBtn.setButtonText(proc.isB() ? "B" : "A"); };
    tabs[0].setToggleState(true, juce::dontSendNotification); showPage(0); startTimerHz(10);
}
MixProEditor::~MixProEditor() { page.reset(); setLookAndFeel(nullptr); }
void MixProEditor::timerCallback() { undoBtn.setEnabled(proc.undo.canUndo()); redoBtn.setEnabled(proc.undo.canRedo()); }

void MixProEditor::showPage(int i) {
    current = i; page.reset(); auto& p = proc;
    switch (i) {
    case 0: page = makeInfo(p, "MIX PRO - one unified system: ERA MATCH, Auto Mix, Genre, Vocal, Instrument, Master Match, Reference Analyzer. Insert one instance per channel/bus, choose the MODE (top) and the instrument ROLE; every control is connected to real DSP.", {"role", "style", "amount", "inGain", "outGain", "matchAmount"}); break;
    case 1: page = makeInfo(p, "AUTO MIX - analyses the signal live and tames accumulations/resonances (max 4.5 dB) on top of the role chain. Style scales EQ / dynamics / saturation. Set mode to AUTO MIX.", {"role", "style", "amount", "matchAmount", "matchFreeze", "dynamics", "character", "ambience", "body", "presence"}); break;
    case 2: page = makeInfo(p, "ERA MATCH - the loaded reference ALWAYS has priority; era shapes (ERA 01 late 90s / 02 early 2000s / 03 mid-late 2000s, blended continuously) are only starting points. Set mode to ERA MATCH (or REFERENCE MATCH for a pure reference match).", {"eraPos", "matchAmount", "matchRef", "matchFreeze", "role", "amount", "style"}, 1); break;
    case 3: page = makeInfo(p, "REFERENCE ANALYZER - load a legally obtained WAV / AIFF / FLAC to create a Reference Fingerprint (JSON). Values are measured; interpretation and confidence are derived from them.", {}, 1); break;
    case 4: page = std::make_unique<GenrePage>(p); break;
    case 5: page = makeInfo(p, "VOCAL ENGINE - roles: Lead, Second Voice, Backing, Doubles, Harmonies, Choir. HPF, EQ, de-esser, compression, saturation, reverb, delay, stereo. Set mode to VOCAL MIX.", {"role", "deEss", "deEssFreq", "presence", "body", "dynamics", "character", "ambience", "reverbSize", "reverbDamp", "preDelay", "delayMs", "delayFb", "delaySync", "delayDiv", "width", "position", "genre"}); break;
    case 6: page = makeInfo(p, "INSTRUMENT ENGINE - guitars, bass, bongos, guira, percussion, keys and more. Guitar: Body, Presence, Pick Attack, Character, Dynamic Control, Ambience, Stereo Position. Set mode to INSTRUMENT MIX.", {"role", "body", "presence", "attack", "sustain", "character", "dynamics", "ambience", "position", "width", "genre", "style"}); break;
    case 7: page = makeInfo(p, "MASTER MATCH - EQ + match, 3-band multiband, glue compressor, saturation, stereo, loudness match to YOUR target and true-peak-safe limiter. Load a reference to match its tonal balance. Set mode to MASTER MATCH.", {"mTargetLufs", "mCeiling", "mMb", "mGlue", "mMonoLow", "mTilt", "mMatchLoud", "matchAmount", "manLow", "manHigh", "width", "character"}, 2); break;
    case 8: page = std::make_unique<PresetPage>(p); break;
    default: page = std::make_unique<SettingsPage>(p); break;
    }
    addAndMakeVisible(*page); resized();
}
void MixProEditor::paint(juce::Graphics& g) {
    g.fillAll(bg); g.setColour(text); g.setFont(juce::FontOptions(30.f).withStyle("Bold")); g.drawText("MIX PRO", 18, 6, 190, 36, juce::Justification::left);
    g.setColour(accent); g.setFont(juce::FontOptions(11.f)); g.drawText("POWERED BY ERA MATCH", 20, 38, 200, 14, juce::Justification::left);
    g.setColour(dim); g.setFont(juce::FontOptions(12.f)); g.drawText("MODE", 236, 14, 50, 20, juce::Justification::left);
    g.setColour(line); g.drawHorizontalLine(52, 0.f, (float)getWidth()); g.setColour(dim); g.setFont(juce::FontOptions(11.f));
    g.drawText("BY ORYMY · YMY RECORDS   —   RECREATE THE ERA. MATCH THE SOUND.", 0, getHeight() - 22, getWidth(), 18, juce::Justification::centred);
}
void MixProEditor::resized() {
    auto r = getLocalBounds(); auto head = r.removeFromTop(52); modeBox.setBounds(280, 14, 230, 26);
    auto right = head.removeFromRight(520).reduced(0, 12); bypassBtn.setBounds(right.removeFromRight(190)); right.removeFromRight(6); abBtn.setBounds(right.removeFromRight(46)); right.removeFromRight(6); redoBtn.setBounds(right.removeFromRight(70)); right.removeFromRight(6); undoBtn.setBounds(right.removeFromRight(70));
    auto tabRow = r.removeFromTop(34).reduced(8, 4); const int tw = tabRow.getWidth() / 10; for (int i = 0; i < 10; ++i) tabs[i].setBounds(tabRow.removeFromLeft(tw).reduced(2, 0));
    visual.setBounds(r.removeFromTop(250).reduced(8, 4)); r.removeFromBottom(24); if (page) page->setBounds(r.reduced(14, 6));
}
