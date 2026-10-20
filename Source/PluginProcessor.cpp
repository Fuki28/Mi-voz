#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr float kSibFreq = 5500.0f;
    constexpr double kLearnSeconds = 6.0;
    constexpr int kDryRingSize = 2048;
    constexpr int kDryMask = kDryRingSize - 1;
}

juce::AudioProcessorValueTreeState::ParameterLayout MiVozAudioProcessor::createLayout()
{
    using R = juce::NormalisableRange<float>;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    auto addF = [&p](const char* id, const char* name, R range, float def,
                     juce::String suffix, int decimals, float scale = 1.0f, bool sign = false)
    {
        auto attrs = juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [suffix, decimals, scale, sign](float v, int)
            {
                juce::String s(v * scale, decimals);
                if (sign && v > 0.0f)
                    s = "+" + s;
                return s + suffix;
            });
        p.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { id, 1 }, name, range, def, attrs));
    };

    auto addB = [&p](const char* id, const char* name, bool def)
    {
        p.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { id, 1 }, name, def));
    };

    // --- limpieza ---
    addF("nrAmount", "Reduccion de ruido", R(0.0f, 1.0f, 0.01f), 0.45f, " %", 0, 100.0f);
    {
        auto attrs = juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [](float v, int) { return v <= -69.5f ? juce::String("OFF") : juce::String(v, 0) + " dB"; });
        p.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "gate", 1 }, "Gate",
                                                                R(-70.0f, -20.0f, 1.0f), -52.0f, attrs));
    }
    addF("hpf", "Low Cut", R(40.0f, 200.0f, 1.0f), 80.0f, " Hz", 0);
    addB("nrAdaptive", "Modo adaptativo", true);

    // --- color y cuerpo ---
    addF("mud", "Limpiar", R(-8.0f, 3.0f, 0.1f), -2.0f, " dB", 1, 1.0f, true);
    addF("presence", "Presencia", R(-3.0f, 9.0f, 0.1f), 2.0f, " dB", 1, 1.0f, true);
    addF("air", "Aire", R(0.0f, 9.0f, 0.1f), 2.0f, " dB", 1, 1.0f, true);

    // --- espacio y fx ---
    addF("sat", "Saturacion", R(0.0f, 1.0f, 0.01f), 0.15f, "", 2);
    addF("reverbMix", "Reverb", R(0.0f, 1.0f, 0.01f), 0.12f, "", 2);
    addF("delayMix", "Delay", R(0.0f, 1.0f, 0.01f), 0.0f, "", 2);
    addF("delayTime", "Delay tiempo", R(60.0f, 600.0f, 1.0f), 250.0f, " ms", 0);
    addF("delayFb", "Delay feedback", R(0.0f, 0.8f, 0.01f), 0.3f, "", 2);

    // --- dinamica ---
    addF("compThresh", "Comp umbral", R(-40.0f, 0.0f, 0.1f), -16.0f, " dB", 0);
    addF("compRatio", "Comp ratio", R(1.0f, 10.0f, 0.1f), 3.0f, ":1", 1);
    addF("compAttack", "Comp ataque", R(1.0f, 80.0f, 0.5f), 10.0f, " ms", 0);
    addF("compRelease", "Comp release", R(20.0f, 800.0f, 1.0f), 300.0f, " ms", 0);
    addF("deessThr", "DeEss umbral", R(-50.0f, 0.0f, 0.1f), -28.0f, " dB", 0);
    addF("deessAmt", "DeEss fuerza", R(0.0f, 1.0f, 0.01f), 0.5f, "", 2);

    // --- sidechain ---
    addB("sidechain", "Sidechain", false);

    // --- salida ---
    addF("out", "Salida", R(-12.0f, 12.0f, 0.1f), 0.0f, " dB", 1, 1.0f, true);
    addF("ceiling", "Techo", R(-12.0f, 0.0f, 0.1f), -0.3f, " dB", 1);
    addF("mix", "Mezcla", R(0.0f, 1.0f, 0.01f), 1.0f, " %", 0, 100.0f);
    addB("bypass", "Bypass", false);

    return { p.begin(), p.end() };
}

MiVozAudioProcessor::MiVozAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)
                         .withInput("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts(*this, nullptr, "PARAMS", createLayout())
{
    for (auto& a : scopeBuf)
        a.store(0.0f, std::memory_order_relaxed);
    setLatencySamples(SpectralDenoiser::latency);
}

bool MiVozAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    const auto sc = layouts.getChannelSet(true, 1);
    return sc.isDisabled() || sc == juce::AudioChannelSet::stereo();
}

void MiVozAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    sampleRateShared.store(sampleRate);
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 2 };

    hpf.prepare(spec);
    lowMid.prepare(spec);
    presence.prepare(spec);
    air.prepare(spec);
    compressor.prepare(spec);
    reverb.prepare(spec);
    reverb.reset();
    limiter.prepare(spec);
    limiter.setRelease(80.0f);
    limiter.reset();

    for (auto& d : denoiser)
        d.prepare(sampleRate);
    denoiser[0].setPublish(true);

    dryBuf.setSize(2, samplesPerBlock);
    rvBuf.setSize(2, samplesPerBlock);
    keyBuf.assign((size_t) samplesPerBlock, 0.0f);
    for (auto& r : dryRing)
        r.assign((size_t) kDryRingSize, 0.0f);
    dryW = 0;

    delayLen = (int) (sampleRate * 1.0) + 4;
    for (auto& d : delayBuf)
        d.assign((size_t) delayLen, 0.0f);
    delayW = 0;

    auto sibCoeffs = Coeffs::makeHighPass(sampleRate, kSibFreq);
    for (auto& f : sibFilter)
    {
        f.coefficients = sibCoeffs;
        f.reset();
    }
    learnFilter.coefficients = sibCoeffs;
    learnFilter.reset();

    gateAtk = std::exp(-1.0f / (0.001f * (float) sampleRate));
    gateRel = std::exp(-1.0f / (0.060f * (float) sampleRate));
    gateOpenCoef = 1.0f - std::exp(-1.0f / (0.003f * (float) sampleRate));
    gateCloseCoef = 1.0f - std::exp(-1.0f / (0.080f * (float) sampleRate));
    duckA = std::exp(-1.0f / (0.004f * (float) sampleRate));
    duckR = std::exp(-1.0f / (0.300f * (float) sampleRate));

    gateEnv = 0.0f;
    gateGain = 1.0f;
    duckEnv = 0.0f;
    deessEnv = 0.0f;

    lastHpf = lastMud = lastPres = lastAir = -1000.0f;
    lastGainDb = apvts.getRawParameterValue("out")->load();
    lastMix = apvts.getRawParameterValue("bypass")->load() > 0.5f ? 0.0f : apvts.getRawParameterValue("mix")->load();

    setLatencySamples(SpectralDenoiser::latency);
}

void MiVozAudioProcessor::ensureScratch(int numSamples)
{
    if (dryBuf.getNumSamples() < numSamples)
    {
        dryBuf.setSize(2, numSamples, false, false, true);
        rvBuf.setSize(2, numSamples, false, false, true);
    }
    if ((int) keyBuf.size() < numSamples)
        keyBuf.resize((size_t) numSamples, 0.0f);
}

void MiVozAudioProcessor::copyScope(std::array<float, scopeSize>& dest) const
{
    const int pos = scopePos.load(std::memory_order_relaxed);
    for (int j = 0; j < scopeSize; ++j)
        dest[(size_t) j] = scopeBuf[(size_t) ((pos + j) & (scopeSize - 1))].load(std::memory_order_relaxed);
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
    out.gate = rGate.load();
    out.nrAmount = rNr.load();
    return true;
}

void MiVozAudioProcessor::runLearning(const juce::AudioBuffer<float>& buffer)
{
    if (learnRequest.exchange(false))
    {
        learnSum = learnSibSum = 0.0;
        learnCount = learnElapsed = 0;
        learnBlockLen = juce::jmax(1, (int) (sr * 0.02));
        learnBlockCount = 0;
        learnBlocksSeen = 0;
        learnBlockSum = 0.0;
        learnMinBlockDb = 0.0f;
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

        // nivel minimo en bloques de 20 ms = piso de ruido aproximado
        learnBlockSum += (double) m * m;
        if (++learnBlockCount >= learnBlockLen)
        {
            const float db = 10.0f * std::log10((float) (learnBlockSum / (double) learnBlockLen) + 1.0e-12f);
            learnMinBlockDb = learnBlocksSeen == 0 ? db : juce::jmin(learnMinBlockDb, db);
            ++learnBlocksSeen;
            learnBlockCount = 0;
            learnBlockSum = 0.0;
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
            const double sibRel = sibDb - rmsDb;

            rCompThr.store((float) juce::jlimit(-40.0, -6.0, rmsDb + 2.0));
            rDeessThr.store((float) juce::jlimit(-50.0, -6.0, sibDb + 6.0));
            rDeessAmt.store(sibRel > -12.0 ? 0.7f : 0.5f);

            // el piso de ruido decide cuanto limpiar
            const float floorDb = learnMinBlockDb;
            rGate.store(juce::jlimit(-70.0f, -25.0f, floorDb + 10.0f));
            rNr.store(juce::jlimit(0.25f, 0.7f, 0.25f + (floorDb + 75.0f) / 25.0f * 0.45f));
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

    ensureScratch(n);

    auto get = [this](const char* id) { return apvts.getRawParameterValue(id)->load(); };

    const float nrAmt     = get("nrAmount");
    const bool  nrAdapt   = get("nrAdaptive") > 0.5f;
    const float gateDb    = get("gate");
    const float hpfHz     = get("hpf");
    const float mudDb     = get("mud");
    const float presDb    = get("presence");
    const float airDb     = get("air");
    const float sat       = get("sat");
    const float revMix    = get("reverbMix");
    const float dlyMix    = get("delayMix");
    const float dlyMs     = get("delayTime");
    const float dlyFb     = get("delayFb");
    const float compThr   = get("compThresh");
    const float compRatio = get("compRatio");
    const float compAtk   = get("compAttack");
    const float compRel   = get("compRelease");
    const float deessThr  = get("deessThr");
    const float deessAmt  = get("deessAmt");
    const bool  scOn      = get("sidechain") > 0.5f;
    const float outDb     = get("out");
    const float ceilingDb = get("ceiling");
    const float mixP      = get("mix");
    const bool  bypass    = get("bypass") > 0.5f;

    runLearning(buffer);

    float* L = buffer.getWritePointer(0);
    float* R = buffer.getWritePointer(1);

    // entrada de sidechain externa (si el host la conecta)
    const float* scL = nullptr;
    const float* scR = nullptr;
    if (buffer.getNumChannels() >= 4)
        if (auto* scBus = getBus(true, 1))
            if (scBus->isEnabled())
            {
                scL = buffer.getReadPointer(2);
                scR = buffer.getReadPointer(3);
            }

    // ---- 1) copiar lo que necesitan la grafica, el sidechain y la mezcla (senal original) ----
    {
        float* dL = dryBuf.getWritePointer(0);
        float* dR = dryBuf.getWritePointer(1);
        int sp = scopePos.load(std::memory_order_relaxed);

        for (int i = 0; i < n; ++i)
        {
            const float l = L[i];
            const float r = R[i];

            scopeBuf[(size_t) sp].store(0.5f * (l + r), std::memory_order_relaxed);
            sp = (sp + 1) & (scopeSize - 1);

            dryRing[0][(size_t) dryW] = l;
            dryRing[1][(size_t) dryW] = r;
            dL[i] = dryRing[0][(size_t) ((dryW - SpectralDenoiser::latency) & kDryMask)];
            dR[i] = dryRing[1][(size_t) ((dryW - SpectralDenoiser::latency) & kDryMask)];
            dryW = (dryW + 1) & kDryMask;

            keyBuf[(size_t) i] = scL != nullptr ? juce::jmax(std::abs(scL[i]), std::abs(scR[i]))
                                                : juce::jmax(std::abs(l), std::abs(r));
        }
        scopePos.store(sp, std::memory_order_relaxed);
    }

    // ---- 2) filtros ----
    if (hpfHz != lastHpf)
    {
        *hpf.state = *Coeffs::makeHighPass(sr, hpfHz, 0.707f);
        lastHpf = hpfHz;
    }
    if (mudDb != lastMud)
    {
        *lowMid.state = *Coeffs::makePeakFilter(sr, 320.0f, 1.0f, juce::Decibels::decibelsToGain(mudDb));
        lastMud = mudDb;
    }
    if (presDb != lastPres)
    {
        *presence.state = *Coeffs::makePeakFilter(sr, 3500.0f, 0.8f, juce::Decibels::decibelsToGain(presDb));
        lastPres = presDb;
    }
    if (airDb != lastAir)
    {
        *air.state = *Coeffs::makeHighShelf(sr, 11000.0f, 0.707f, juce::Decibels::decibelsToGain(airDb));
        lastAir = airDb;
    }

    compressor.setThreshold(compThr);
    compressor.setRatio(compRatio);
    compressor.setAttack(compAtk);
    compressor.setRelease(compRel);

    juce::dsp::AudioBlock<float> fullBlock(buffer);
    auto mainBlock = fullBlock.getSubsetChannelBlock(0, 2);
    juce::dsp::ProcessContextReplacing<float> ctx(mainBlock);

    // ---- 3) low cut -> reduccion de ruido -> gate ----
    hpf.process(ctx);

    denoiser[0].setParams(nrAmt, nrAdapt);
    denoiser[1].setParams(nrAmt, nrAdapt);

    {
        const bool gateOn = gateDb > -69.5f;
        const float gThr = juce::Decibels::decibelsToGain(gateDb);

        for (int i = 0; i < n; ++i)
        {
            float l = denoiser[0].processSample(L[i]);
            float r = denoiser[1].processSample(R[i]);

            if (gateOn)
            {
                const float det = juce::jmax(std::abs(l), std::abs(r));
                gateEnv = det > gateEnv ? gateAtk * gateEnv + (1.0f - gateAtk) * det
                                        : gateRel * gateEnv + (1.0f - gateRel) * det;

                float target = 1.0f;
                if (gateEnv < gThr)
                {
                    const float ratio = gateEnv / gThr;
                    target = juce::jmax(0.03f, ratio * ratio);
                }
                gateGain += (target - gateGain) * (target > gateGain ? gateOpenCoef : gateCloseCoef);
                l *= gateGain;
                r *= gateGain;
            }
            else
            {
                gateGain = 1.0f;
            }

            L[i] = l;
            R[i] = r;
        }
    }

    // ---- 4) tono y dinamica ----
    lowMid.process(ctx);
    compressor.process(ctx);

    // de-esser
    {
        const float thrLin = juce::Decibels::decibelsToGain(deessThr);
        const float atk = std::exp(-1.0f / (0.001f * (float) sr));
        const float rel = std::exp(-1.0f / (0.050f * (float) sr));

        for (int i = 0; i < n; ++i)
        {
            const float bl = sibFilter[0].processSample(L[i]);
            const float br = sibFilter[1].processSample(R[i]);
            const float det = juce::jmax(std::abs(bl), std::abs(br));

            deessEnv = det > deessEnv ? atk * deessEnv + (1.0f - atk) * det
                                      : rel * deessEnv + (1.0f - rel) * det;

            float g = 1.0f;
            if (deessEnv > thrLin && deessAmt > 0.0f)
                g = std::pow(thrLin / deessEnv, deessAmt);

            L[i] = L[i] - bl * (1.0f - g);
            R[i] = R[i] - br * (1.0f - g);
        }
    }

    presence.process(ctx);
    air.process(ctx);

    // saturacion suave
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

    // ---- 5) efectos: reverb y delay, con ducking opcional (sidechain) ----
    {
        for (int ch = 0; ch < 2; ++ch)
            rvBuf.copyFrom(ch, 0, buffer, ch, 0, n);

        juce::dsp::Reverb::Parameters rp;
        rp.roomSize = 0.5f;
        rp.damping = 0.5f;
        rp.wetLevel = 0.33f;
        rp.dryLevel = 0.0f;
        rp.width = 1.0f;
        rp.freezeMode = 0.0f;
        reverb.setParameters(rp);
        {
            juce::dsp::AudioBlock<float> rb(rvBuf);
            auto rbSub = rb.getSubBlock(0, (size_t) n);
            juce::dsp::ProcessContextReplacing<float> rctx(rbSub);
            reverb.process(rctx);
        }

        const float rvGain = revMix * 0.8f;
        const float dlGain = dlyMix * 0.7f;
        const float delaySamples = juce::jlimit(1.0f, (float) (delayLen - 3), dlyMs * 0.001f * (float) sr);

        const float duckLo = 0.00316f; // -50 dB
        const float duckHi = 0.0316f;  // -30 dB

        float* rL = rvBuf.getWritePointer(0);
        float* rR = rvBuf.getWritePointer(1);
        float* chans[2] = { L, R };
        float* rvs[2] = { rL, rR };

        for (int i = 0; i < n; ++i)
        {
            const float k = keyBuf[(size_t) i];
            duckEnv = k > duckEnv ? duckA * duckEnv + (1.0f - duckA) * k
                                  : duckR * duckEnv + (1.0f - duckR) * k;

            float duckGain = 1.0f;
            if (scOn)
                duckGain = 1.0f - 0.75f * juce::jlimit(0.0f, 1.0f, (duckEnv - duckLo) / (duckHi - duckLo));

            float rpos = (float) delayW - delaySamples;
            if (rpos < 0.0f)
                rpos += (float) delayLen;
            const int i0 = (int) rpos;
            const int i1 = (i0 + 1) % delayLen;
            const float frac = rpos - (float) i0;

            for (int ch = 0; ch < 2; ++ch)
            {
                const float x = chans[ch][i];
                const float echo = delayBuf[ch][(size_t) i0] * (1.0f - frac) + delayBuf[ch][(size_t) i1] * frac;
                delayBuf[ch][(size_t) delayW] = x + echo * dlyFb;

                chans[ch][i] = x + (echo * dlGain + rvs[ch][i] * rvGain) * duckGain;
            }
            delayW = (delayW + 1) % delayLen;
        }
    }

    // ---- 6) salida: ganancia, limitador, mezcla con la senal original ----
    const float makeupDb = juce::jlimit(0.0f, 12.0f, -compThr * (1.0f - 1.0f / compRatio) * 0.4f);
    const float targetDb = outDb + makeupDb;
    for (int ch = 0; ch < 2; ++ch)
        buffer.applyGainRamp(ch, 0, n, juce::Decibels::decibelsToGain(lastGainDb),
                             juce::Decibels::decibelsToGain(targetDb));
    lastGainDb = targetDb;

    limiter.setThreshold(ceilingDb);
    limiter.process(ctx);

    const float mixTarget = bypass ? 0.0f : mixP;
    for (int ch = 0; ch < 2; ++ch)
    {
        buffer.applyGainRamp(ch, 0, n, lastMix, mixTarget);
        buffer.addFromWithRamp(ch, 0, dryBuf.getReadPointer(ch), n, 1.0f - lastMix, 1.0f - mixTarget);
    }
    lastMix = mixTarget;

    // medidor de pico
    float pl = 0.0f, pr = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        pl = juce::jmax(pl, std::abs(L[i]));
        pr = juce::jmax(pr, std::abs(R[i]));
    }
    peakL.store(juce::jmax(peakL.load(), pl));
    peakR.store(juce::jmax(peakR.load(), pr));
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
