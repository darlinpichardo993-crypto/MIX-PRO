#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "../Engines/MixEngine.h"
#include "../Engines/FactoryPresets.h"
#include "../Analysis/ReferenceAnalyzer.h"

class MixProProcessor : public juce::AudioProcessor, private juce::AudioProcessorValueTreeState::Listener {
public:
    MixProProcessor();
    ~MixProProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "MIX PRO"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 2.5; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::UndoManager undo;
    juce::AudioProcessorValueTreeState apvts;
    mp::MixEngine engine;

    // ---- reference / fingerprint ----
    void loadReferenceFile(const juce::File& f);
    bool setReferenceFromJson(const juce::String& json);
    juce::String referenceJson() const;
    mp::Fingerprint getReference() const;
    std::vector<float> getWaveform() const;
    juce::String getStatus() const;
    int referenceVersion() const { return refVersion_.load(); }
    void clearReference();

    // ---- presets ----
    static const std::vector<mp::PresetData>& factoryList();
    void applyPreset(const mp::PresetData& p);
    juce::File dataDir() const;
    juce::File userPresetDir() const;
    bool saveUserPreset(const juce::String& name);
    bool loadPresetFile(const juce::File& f);
    juce::Array<juce::File> listUserPresets() const;
    bool exportGenreProfiles(const juce::File& f) const;
    bool importGenreProfiles(const juce::File& f);

    // ---- A/B + offline render ----
    void toggleAB();
    bool isB() const { return abIdx_ == 1; }
    void renderFile(const juce::File& in, const juce::File& out);
    float param(int idx) const { return raw_[idx]->load(); }

private:
    void parameterChanged(const juce::String&, float) override;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void setStatus(const juce::String& s);
    void commitReference(const mp::Fingerprint& fp, std::vector<float> wave);

    std::atomic<float>* raw_[mp::kNumParams];
    mutable juce::SpinLock refLock_; mutable juce::CriticalSection uiLock_;
    mp::Fingerprint ref_; bool refValid_ = false, refDirty_ = false; std::vector<float> wave_; juce::String status_;
    std::atomic<int> refVersion_{0}, jobs_{0}; int masterLatency_ = 96; double sr_ = 44100.0;
    juce::ValueTree ab_[2]; int abIdx_ = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixProProcessor)
};
