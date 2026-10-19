#pragma once

#include "PluginProcessor.h"
#include <memory>
#include <vector>

class MiVozAudioProcessorEditor : public juce::AudioProcessorEditor,
                                  private juce::Timer
{
public:
    explicit MiVozAudioProcessorEditor(MiVozAudioProcessor&);
    ~MiVozAudioProcessorEditor() override { stopTimer(); }

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void addKnob(const juce::String& id, const juce::String& text, const juce::String& suffix);
    void timerCallback() override;
    void applyLearnResult(const MiVozAudioProcessor::LearnResult& r);

    MiVozAudioProcessor& audioProcessor;
    std::vector<std::unique_ptr<Knob>> knobs;
    juce::TextButton learnButton { "Asistente: canta 6 segundos" };
    juce::Label status;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MiVozAudioProcessorEditor)
};
