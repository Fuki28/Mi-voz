#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>

class MiVozAudioProcessor : public juce::AudioProcessor
{
public:
    MiVozAudioProcessor();
    ~MiVozAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MiVoz"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // ---- Asistente: escucha la voz unos segundos y sugiere ajustes ----
    struct LearnResult
    {
        bool ok = false;
        float compThresh = -18.0f;
        float deessThr = -28.0f;
        float deessAmt = 0.5f;
    };

    void startLearning();
    bool isLearning() const { return learning.load(); }
    float getLearnProgress() const { return learnProgress.load(); }
    bool pollLearnResult(LearnResult& out); // true una sola vez cuando termina

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    using Filter = juce::dsp::IIR::Filter<float>;
    using Coeffs = juce::dsp::IIR::Coefficients<float>;
    using StereoFilter = juce::dsp::ProcessorDuplicator<Filter, Coeffs>;

    void runLearning(const juce::AudioBuffer<float>& buffer);

    double sr = 44100.0;

    StereoFilter hpf, lowMid, presence, air;
    juce::dsp::Compressor<float> compressor;
    juce::dsp::Reverb reverb;
    juce::dsp::DelayLine<float> delay { 192000 };

    // de-esser
    Filter sibFilter[2];
    float deessEnv = 0.0f;

    // asistente
    Filter learnFilter;
    std::atomic<bool> learnRequest { false }, learning { false }, learnDone { false }, learnOk { false };
    std::atomic<float> learnProgress { 0.0f };
    std::atomic<float> rCompThr { -18.0f }, rDeessThr { -28.0f }, rDeessAmt { 0.5f };
    double learnSum = 0.0, learnSibSum = 0.0;
    long long learnCount = 0, learnElapsed = 0;

    float lastGainDb = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MiVozAudioProcessor)
};
