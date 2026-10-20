#pragma once

#include "PluginProcessor.h"
#include "MiVozUI.h"
#include <array>
#include <memory>
#include <vector>

class MiVozAudioProcessorEditor : public juce::AudioProcessorEditor,
                                  private juce::Timer
{
public:
    explicit MiVozAudioProcessorEditor(MiVozAudioProcessor&);
    ~MiVozAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    static constexpr int kW = 1100;
    static constexpr int kH = 920;

    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    KnobComp* addKnob(int card, const char* paramId, const juce::String& name, juce::Colour arc);
    juce::Rectangle<int> cardRect(int index) const;
    void timerCallback() override;
    void applyLearnResult(const MiVozAudioProcessor::LearnResult& r);
    void applyPreset(int index);
    void setParam(const char* id, float value);

    MiVozLookAndFeel laf;
    MiVozAudioProcessor& proc;

    GraphComp graph;
    GemComp gem;
    juce::ComboBox presetBox;
    PillButton bypassBtn { "Bypass", ui::salmon };
    PillButton learnBtn { "Asistente - 6 s", ui::salmon, true };
    PillButton adaptiveBtn { "Modo adaptativo", juce::Colour(0xff8fd0d0) };
    BigButton sideBtn { "Sidechain" };
    juce::Slider mixSlider;
    juce::Label status;

    std::vector<std::unique_ptr<KnobComp>> knobs;
    std::array<std::vector<KnobComp*>, 6> cardKnobs;

    std::unique_ptr<ButtonAtt> bypassAtt, adaptiveAtt, sideAtt;
    std::unique_ptr<SliderAtt> mixAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MiVozAudioProcessorEditor)
};
