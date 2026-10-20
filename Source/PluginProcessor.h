#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <vector>
#include "SpectralDenoiser.h"

class MiVozAudioProcessor : public juce::AudioProcessor
{
public:
    static constexpr int scopeSize = 1024;

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
        float compThresh = -16.0f;
        float deessThr = -28.0f;
        float deessAmt = 0.5f;
        float gate = -52.0f;
        float nrAmount = 0.45f;
    };

    void startLearning();
    bool isLearning() const { return learning.load(); }
    float getLearnProgress() const { return learnProgress.load(); }
    bool pollLearnResult(LearnResult& out); // true una sola vez cuando termina

    // ---- datos para la interfaz ----
    void copyScope(std::array<float, scopeSize>& dest) const;
    float getNoiseDb(int bin) const { return denoiser[0].getNoiseDb(bin); }
    double getCurrentSampleRate() const { return sampleRateShared.load(); }
    std::atomic<float> peakL { 0.0f }, peakR { 0.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    using Filter = juce::dsp::IIR::Filter<float>;
    using Coeffs = juce::dsp::IIR::Coefficients<float>;
    using StereoFilter = juce::dsp::ProcessorDuplicator<Filter, Coeffs>;

    void ensureScratch(int numSamples);
    void runLearning(const juce::AudioBuffer<float>& buffer);

    double sr = 44100.0;
    std::atomic<double> sampleRateShared { 44100.0 };

    StereoFilter hpf, lowMid, presence, air;
    juce::dsp::Compressor<float> compressor;
    juce::dsp::Reverb reverb;
    juce::dsp::Limiter<float> limiter;
    SpectralDenoiser denoiser[2];

    // cache de parametros de los filtros (evita recalcular en cada bloque)
    float lastHpf = -1.0f, lastMud = -100.0f, lastPres = -100.0f, lastAir = -100.0f;

    // buffers auxiliares
    juce::AudioBuffer<float> dryBuf, rvBuf;
    std::vector<float> keyBuf;
    std::vector<float> dryRing[2];
    int dryW = 0;
    std::vector<float> delayBuf[2];
    int delayLen = 0, delayW = 0;

    // de-esser
    Filter sibFilter[2];
    float deessEnv = 0.0f;

    // gate
    float gateEnv = 0.0f, gateGain = 1.0f;
    float gateAtk = 0.0f, gateRel = 0.0f, gateOpenCoef = 0.0f, gateCloseCoef = 0.0f;

    // ducking de efectos (sidechain)
    float duckEnv = 0.0f, duckA = 0.0f, duckR = 0.0f;

    float lastGainDb = 0.0f;
    float lastMix = 1.0f;

    // scope (para el analizador)
    std::array<std::atomic<float>, scopeSize> scopeBuf {};
    std::atomic<int> scopePos { 0 };

    // asistente
    Filter learnFilter;
    std::atomic<bool> learnRequest { false }, learning { false }, learnDone { false }, learnOk { false };
    std::atomic<float> learnProgress { 0.0f };
    std::atomic<float> rCompThr { -16.0f }, rDeessThr { -28.0f }, rDeessAmt { 0.5f }, rGate { -52.0f }, rNr { 0.45f };
    double learnSum = 0.0, learnSibSum = 0.0;
    long long learnCount = 0, learnElapsed = 0;
    int learnBlockLen = 0, learnBlockCount = 0, learnBlocksSeen = 0;
    double learnBlockSum = 0.0;
    float learnMinBlockDb = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MiVozAudioProcessor)
};
