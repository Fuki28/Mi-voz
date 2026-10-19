#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr float kSibFreq = 5500.0f;
    constexpr double kLearnSeconds = 6.0;
}

juce::AudioProcessorValueTreeState::ParameterLayout MiVozAudioProcessor::createLayout()
{
    using P = juce::AudioParameterFloat;
    using R = juce::NormalisableRange<float>;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    auto add = [&p](const char* id, const char* name, R range, float def)
    {
        p.push_back(std::make_unique<P>(juce::ParameterID { id, 1 }, name, range, def));
    };

    add("hpf",         "Low Cut",       R(40.0f, 200.0f, 1.0f),   80.0f);
    add("mud",         "Limpiar Mid",   R(-8.0f, 3.0f, 0.1f),    -2.0f);
    add("presence",    "Presencia",     R(-3.0f, 9.0f, 0.1f),     2.0f);
    add("air",         "Aire",          R(0.0f, 9.0f, 0.1f),      2.0f);
    add("out",         "Salida",        R(-12.0f, 12.0f, 0.1f),   0.0f);

    add("compThresh",  "Comp Umbral",   R(-40.0f, 0.0f, 0.1f),  -18.0f);
    add("compRatio",   "Comp Ratio",    R(1.0f, 10.0f, 0.1f),     3.0f);
    add("deessThr",    "DeEss Umbral",  R(-50.0f, 0.0f, 0.1f),  -28.0f);
    add("deessAmt",    "DeEss Fuerza",  R(0.0f, 1.0f, 0.01f),     0.5f);
    add("sat",         "Saturacion",    R(0.0f, 1.0f, 0.01f),     0.15f);

    add("reverbMix",   "Reverb Mezcla", R(0.0f, 0.5f, 0.01f),     0.10f);
    add("reverbSize",  "Reverb Tamano", R(0.0f, 1.0f, 0.01f),     0.40f);
    add("delayMix",    "Delay Mezcla",  R(0.0f, 0.5f, 0.01f),     0.0f);
    add("delayTime",   "Delay Tiempo",  R(60.0f, 600.0f, 1.0f), 250.0f);
    add("delayFb",     "Delay Feedback",R(0.0f, 0.8f, 0.01f),     0.30f);

    return { p.begin(), p.end() };
}

MiVozAudioProcessor::MiVozAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", createLayout())
{
}

bool MiVozAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet() == layouts.getMainOutputChannelSet();
}

void MiVozAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 2 };

    hpf.prepare(spec);
    lowMid.prepare(spec);
    presence.prepare(spec);
    air.prepare(spec);
    compressor.prepare(spec);
    reverb.prepare(spec);

    delay.setMaximumDelayInSamples((int) (sampleRate * 1.0));
    delay.prepare(spec);
    delay.reset();

    auto sibCoeffs = Coeffs::makeHighPass(sampleRate, kSibFreq);
    for (auto& f : sibFilter)
    {
        f.coefficients = sibCoeffs;
        f.reset();
    }
    learnFilter.coefficients = sibCoeffs;
    learnFilter.reset();

    deessEnv = 0.0f;
    lastGainDb = *apvts.getRawParameterValue("out");
}

void MiVozAudioProcessor::startLearning()
{
    learnDone.store(false);
    learnProgress.store(0.0f);
    learnRequest.store(true);
}

bool MiVozAudioProcessor::pollLearnResult(LearnResult& out)
{
    if (!learnDone.exchange(false))
        return false;
    out.ok = learnOk.load();
    out.compThresh = rCompThr.load();
    out.deessThr = rDeessThr.load();
    out.deessAmt = rDeessAmt.load();
    return true;
}

void MiVozAudioProcessor::runLearning(const juce::AudioBuffer<float>& buffer)
{
    if (learnRequest.exchange(false))
    {
        learnSum = learnSibSum = 0.0;
        learnCount = learnElapsed = 0;
        learnFilter.reset();
        learning.store(true);
    }

    if (!learning.load())
        return;

    const int n = buffer.getNumSamples();
    const float* l = buffer.getReadPointer(0);
    const float* r = buffer.getReadPointer(1);

    for (int i = 0; i < n; ++i)
    {
        const float m = 0.5f * (l[i] + r[i]);
        const float s = learnFilter.processSample(m);
        if (std::abs(m) > 0.003f) // ignora silencio (~ -50 dBFS)
        {
            learnSum += (double) m * m;
            learnSibSum += (double) s * s;
            ++learnCount;
        }
    }

    learnElapsed += n;
    learnProgress.store((float) juce::jlimit(0.0, 1.0, (double) learnElapsed / (kLearnSeconds * sr)));

    if ((double) learnElapsed >= kLearnSeconds * sr)
    {
        const bool enough = learnCount > (long long) (sr * 0.5);
        if (enough)
        {
            const double rmsDb = 10.0 * std::log10(learnSum / (double) learnCount + 1.0e-12);
            const double sibDb = 10.0 * std::log10(learnSibSum / (double) learnCount + 1.0e-12);
            const double sibRel = sibDb - rmsDb; // cuanto pesa la zona de las "s" respecto a la voz

            rCompThr.store((float) juce::jlimit(-40.0, -6.0, rmsDb + 2.0));
            rDeessThr.store((float) juce::jlimit(-50.0, -6.0, sibDb + 6.0));
            rDeessAmt.store(sibRel > -12.0 ? 0.7f : 0.5f);
        }
        learnOk.store(enough);
        learning.store(false);
        learnDone.store(true);
    }
}

void MiVozAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2 || n == 0)
        return;

    runLearning(buffer);

    auto get = [this](const char* id) { return apvts.getRawParameterValue(id)->load(); };

    const float hpfHz     = get("hpf");
    const float mudDb     = get("mud");
    const float presDb    = get("presence");
    const float airDb     = get("air");
    const float outDb     = get("out");
    const float compThr   = get("compThresh");
    const float compRatio = get("compRatio");
    const float deessThr  = get("deessThr");
    const float deessAmt  = get("deessAmt");
    const float sat       = get("sat");
    const float revMix    = get("reverbMix");
    const float revSize   = get("reverbSize");
    const float dlyMix    = get("delayMix");
    const float dlyMs     = get("delayTime");
    const float dlyFb     = get("delayFb");

    // --- actualizar filtros ---
    *hpf.state      = *Coeffs::makeHighPass(sr, hpfHz, 0.707f);
    *lowMid.state   = *Coeffs::makePeakFilter(sr, 320.0f, 1.0f, juce::Decibels::decibelsToGain(mudDb));
    *presence.state = *Coeffs::makePeakFilter(sr, 3500.0f, 0.8f, juce::Decibels::decibelsToGain(presDb));
    *air.state      = *Coeffs::makeHighShelf(sr, 11000.0f, 0.707f, juce::Decibels::decibelsToGain(airDb));

    compressor.setThreshold(compThr);
    compressor.setRatio(compRatio);
    compressor.setAttack(8.0f);
    compressor.setRelease(120.0f);

    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> ctx(block);

    // 1) limpieza + compresion
    hpf.process(ctx);
    lowMid.process(ctx);
    compressor.process(ctx);

    // 2) de-esser (resta la banda de sibilancia solo cuando se pasa del umbral)
    {
        float* l = buffer.getWritePointer(0);
        float* r = buffer.getWritePointer(1);
        const float thrLin = juce::Decibels::decibelsToGain(deessThr);
        const float atk = std::exp(-1.0f / (0.001f * (float) sr));
        const float rel = std::exp(-1.0f / (0.050f * (float) sr));

        for (int i = 0; i < n; ++i)
        {
            const float bl = sibFilter[0].processSample(l[i]);
            const float br = sibFilter[1].processSample(r[i]);
            const float det = juce::jmax(std::abs(bl), std::abs(br));

            deessEnv = det > deessEnv ? atk * deessEnv + (1.0f - atk) * det
                                      : rel * deessEnv + (1.0f - rel) * det;

            float g = 1.0f;
            if (deessEnv > thrLin && deessAmt > 0.0f)
                g = std::pow(thrLin / deessEnv, deessAmt);

            l[i] = l[i] - bl * (1.0f - g);
            r[i] = r[i] - br * (1.0f - g);
        }
    }

    // 3) brillo
    presence.process(ctx);
    air.process(ctx);

    // 4) saturacion suave
    if (sat > 0.001f)
    {
        const float drive = 1.0f + sat * 5.0f;
        for (int ch = 0; ch < 2; ++ch)
        {
            float* d = buffer.getWritePointer(ch);
            for (int i = 0; i < n; ++i)
                d[i] = std::tanh(drive * d[i]) / drive;
        }
    }

    // 5) delay
    {
        const float delaySamples = juce::jlimit(1.0f, (float) (sr * 0.99), dlyMs * 0.001f * (float) sr);
        for (int ch = 0; ch < 2; ++ch)
        {
            float* d = buffer.getWritePointer(ch);
            for (int i = 0; i < n; ++i)
            {
                const float x = d[i];
                const float wet = delay.popSample(ch, delaySamples);
                delay.pushSample(ch, x + wet * dlyFb);
                d[i] = x + wet * dlyMix;
            }
        }
    }

    // 6) reverb
    {
        juce::dsp::Reverb::Parameters rp;
        rp.roomSize = revSize;
        rp.damping = 0.5f;
        rp.wetLevel = revMix * 0.6f;
        rp.dryLevel = 1.0f;
        rp.width = 1.0f;
        rp.freezeMode = 0.0f;
        reverb.setParameters(rp);
        reverb.process(ctx);
    }

    // 7) ganancia final (con compensacion automatica del compresor)
    const float makeupDb = juce::jlimit(0.0f, 12.0f, -compThr * (1.0f - 1.0f / compRatio) * 0.4f);
    const float targetDb = outDb + makeupDb;
    buffer.applyGainRamp(0, n, juce::Decibels::decibelsToGain(lastGainDb),
                         juce::Decibels::decibelsToGain(targetDb));
    lastGainDb = targetDb;
}

juce::AudioProcessorEditor* MiVozAudioProcessor::createEditor()
{
    return new MiVozAudioProcessorEditor(*this);
}

void MiVozAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void MiVozAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MiVozAudioProcessor();
}
