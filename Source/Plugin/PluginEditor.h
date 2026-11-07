#pragma once
#include "PluginProcessor.h"

namespace ui {
const juce::Colour bg(0xff0d0d0f), panel(0xff17171a), line(0xff2a2a2f), text(0xffe9e9ec), dim(0xff8b8b93), accent(0xffc8a24a);

struct LookAndFeel : juce::LookAndFeel_V4 {
    LookAndFeel() {
        setColour(juce::ResizableWindow::backgroundColourId, bg);
        setColour(juce::Slider::rotarySliderFillColourId, accent); setColour(juce::Slider::rotarySliderOutlineColourId, line);
        setColour(juce::Slider::thumbColourId, text); setColour(juce::Slider::textBoxTextColourId, text); setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour(juce::Label::textColourId, text);
        setColour(juce::ComboBox::backgroundColourId, panel); setColour(juce::ComboBox::textColourId, text); setColour(juce::ComboBox::outlineColourId, line); setColour(juce::ComboBox::arrowColourId, dim);
        setColour(juce::PopupMenu::backgroundColourId, panel); setColour(juce::PopupMenu::textColourId, text); setColour(juce::PopupMenu::highlightedBackgroundColourId, accent.darker(0.5f));
        setColour(juce::TextButton::buttonColourId, panel); setColour(juce::TextButton::buttonOnColourId, accent.darker(0.3f)); setColour(juce::TextButton::textColourOffId, text); setColour(juce::TextButton::textColourOnId, juce::Colours::black);
        setColour(juce::ToggleButton::textColourId, text); setColour(juce::ToggleButton::tickColourId, accent);
        setColour(juce::TextEditor::backgroundColourId, panel); setColour(juce::TextEditor::textColourId, text); setColour(juce::TextEditor::outlineColourId, line);
        setColour(juce::ListBox::backgroundColourId, panel);
    }
};
}

class VisualPanel : public juce::Component, private juce::Timer {
public:
    explicit VisualPanel(MixProProcessor& p) : proc(p) { startTimerHz(30); }
    void paint(juce::Graphics& g) override;
private:
    void timerCallback() override { repaint(); }
    void drawSpectrum(juce::Graphics&, juce::Rectangle<float>);
    void drawMeters(juce::Graphics&, juce::Rectangle<float>);
    void drawScope(juce::Graphics&, juce::Rectangle<float>);
    juce::String chainText() const;
    MixProProcessor& proc;
};

class ParamPanel : public juce::Component {
public:
    ParamPanel(MixProProcessor& p, const std::vector<const char*>& ids);
    void resized() override;
private:
    struct Cell { std::unique_ptr<juce::Label> label; std::unique_ptr<juce::Component> ctl; int kind = 0; };
    std::vector<Cell> cells; std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sa;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> ca; std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> ba;
};

class RefTools : public juce::Component, private juce::Timer {
public:
    RefTools(MixProProcessor& p, bool compact, bool offerLufs);
    void resized() override; void paint(juce::Graphics&) override;
private:
    void timerCallback() override;
    MixProProcessor& proc; bool compact; int lastVersion = -1;
    juce::TextButton loadBtn{"Load reference audio..."}, exportBtn{"Export fingerprint JSON"}, importBtn{"Import fingerprint JSON"}, clearBtn{"Clear"}, lufsBtn{"Use reference LUFS as target"};
    juce::Label status; juce::TextEditor report; std::unique_ptr<juce::FileChooser> chooser; bool offerLufs;
};

class MixProEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit MixProEditor(MixProProcessor&);
    ~MixProEditor() override;
    void paint(juce::Graphics&) override; void resized() override;
private:
    void timerCallback() override;
    void showPage(int index);
    MixProProcessor& proc; ui::LookAndFeel lnf; VisualPanel visual;
    juce::TextButton tabs[10]; std::unique_ptr<juce::Component> page; int current = 0;
    juce::ComboBox modeBox; std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAtt;
    juce::TextButton undoBtn{"Undo"}, redoBtn{"Redo"}, abBtn{"A"}; juce::ToggleButton bypassBtn{"Bypass (Before / After)"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAtt;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixProEditor)
};
