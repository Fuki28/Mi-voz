#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <cmath>

// Reduccion de ruido espectral (STFT: ventana de 1024 muestras, solape del 75 %).
// Estima el ruido de fondo por frecuencia (minimo que sube muy despacio) y atenua
// cada banda en la que solo hay ruido. Latencia fija: N muestras.
class SpectralDenoiser
{
public:
    static constexpr int order = 10;
    static constexpr int N = 1 << order;   // 1024
    static constexpr int H = N / 4;        // salto entre ventanas
    static constexpr int numBins = N / 2 + 1;
    static constexpr int latency = N;

    SpectralDenoiser() : fft(order)
    {
        for (int i = 0; i < N; ++i)
            window[(size_t) i] = std::sqrt(0.5f * (1.0f - std::cos(juce::MathConstants<float>::twoPi * (float) i / (float) N)));
        reset();
    }

    void prepare(double sampleRate)
    {
        // el piso de ruido estimado sube como mucho ~4.5 dB por segundo
        riseFactor = std::pow(10.0f, (4.5f * (float) H / (float) sampleRate) / 20.0f);
        reset();
    }

    void reset()
    {
        inRing.fill(0.0f);
        ola.fill(0.0f);
        noise.fill(0.0f);
        gainPrev.fill(1.0f);
        gains.fill(1.0f);
        writePos = 0;
        outPos = 0;
        hopCount = 0;
        frames = 0;
        for (auto& a : noiseOut)
            a.store(-150.0f, std::memory_order_relaxed);
    }

    void setParams(float newAmount, bool newAdaptive)
    {
        amount = newAmount;
        adaptive = newAdaptive;
    }

    void setPublish(bool shouldPublish) { publish = shouldPublish; }

    // nivel de ruido estimado en dB (misma escala que el analizador de la interfaz)
    float getNoiseDb(int bin) const
    {
        return noiseOut[(size_t) juce::jlimit(0, numBins - 1, bin)].load(std::memory_order_relaxed);
    }

    float processSample(float x)
    {
        const float y = ola[(size_t) outPos];
        ola[(size_t) outPos] = 0.0f;
        outPos = (outPos + 1) & (N - 1);

        inRing[(size_t) writePos] = x;
        writePos = (writePos + 1) & (N - 1);

        if (++hopCount >= H)
        {
            hopCount = 0;
            processFrame();
        }
        return y;
    }

private:
    void processFrame()
    {
        for (int j = 0; j < N; ++j)
            fftData[(size_t) j] = inRing[(size_t) ((writePos + j) & (N - 1))] * window[(size_t) j];
        for (int j = N; j < 2 * N; ++j)
            fftData[(size_t) j] = 0.0f;

        fft.performRealOnlyForwardTransform(fftData.data(), true);

        const bool active = amount > 0.001f;
        const float over = 1.0f + 1.0f * amount;
        const float floorG = std::pow(10.0f, -(6.0f + 30.0f * amount) / 20.0f);
        const bool learnNow = adaptive || frames < 40;

        for (int k = 0; k < numBins; ++k)
        {
            const float re = fftData[(size_t) (2 * k)];
            const float im = fftData[(size_t) (2 * k + 1)];
            const float mag = std::sqrt(re * re + im * im);

            float& nz = noise[(size_t) k];
            if (frames == 0)
                nz = mag;
            else if (learnNow)
            {
                if (mag < nz)
                    nz += 0.3f * (mag - nz);
                else
                    nz *= riseFactor;
            }
            nz = juce::jmax(nz, 1.0e-7f);

            if (active)
                gains[(size_t) k] = juce::jlimit(floorG, 1.0f, 1.0f - over * noiseBias * nz / (mag + 1.0e-9f));
            else
                gains[(size_t) k] = 1.0f;
        }

        for (int k = 0; k < numBins; ++k)
        {
            const float a = gains[(size_t) juce::jmax(0, k - 1)];
            const float b = gains[(size_t) k];
            const float c = gains[(size_t) juce::jmin(numBins - 1, k + 1)];
            const float target = 0.25f * a + 0.5f * b + 0.25f * c;

            float& prev = gainPrev[(size_t) k];
            prev += (target - prev) * (target > prev ? 0.6f : 0.4f);

            fftData[(size_t) (2 * k)] *= prev;
            fftData[(size_t) (2 * k + 1)] *= prev;
        }

        fft.performRealOnlyInverseTransform(fftData.data());

        for (int j = 0; j < N; ++j)
            ola[(size_t) ((outPos + j) & (N - 1))] += fftData[(size_t) j] * window[(size_t) j] * synthScale;

        if (publish)
            for (int k = 0; k < numBins; ++k)
                noiseOut[(size_t) k].store(20.0f * std::log10(noise[(size_t) k] * 4.0f / (float) N + 1.0e-9f),
                                            std::memory_order_relaxed);

        ++frames;
    }

    juce::dsp::FFT fft;

    std::array<float, N> window {};
    std::array<float, N> inRing {};
    std::array<float, N> ola {};
    std::array<float, 2 * N> fftData {};
    std::array<float, numBins> noise {};
    std::array<float, numBins> gains {};
    std::array<float, numBins> gainPrev {};
    std::array<std::atomic<float>, numBins> noiseOut {};

    int writePos = 0, outPos = 0, hopCount = 0;
    long long frames = 0;

    float amount = 0.0f;
    bool adaptive = true;
    bool publish = false;
    float riseFactor = 1.003f;
    // el minimo rastreado queda por debajo del nivel medio del ruido: se corrige con este factor
    float noiseBias = 4.0f;

    // ventana sqrt-Hann en analisis y sintesis, solape 75 %: la suma da 2
    float synthScale = 0.5f;
};
