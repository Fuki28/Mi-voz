#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
#include "PluginProcessor.h"

#define MV_U8(x) juce::String::fromUTF8(x)

namespace ui
{
    const juce::Colour paper    (0xffefe7d0);
    const juce::Colour gridLine (0xffe1d7ba);
    const juce::Colour card     (0xfffaf6e6);
    const juce::Colour ink      (0xff2e2620);
    const juce::Colour inkSoft  (0xff5a4d42);
    const juce::Colour salmon   (0xfff0a088);
    const juce::Colour coral    (0xffb9503b);
    const juce::Colour teal     (0xff3f9f9f);
    const juce::Colour tealLight(0xff5cc0c0);
    const juce::Colour screen   (0xff14181b);
    const juce::Colour screenInk(0xfff4f1e6);
}

//==============================================================================
// Estilo visual: papel cuadriculado, tarjetas con borde grueso, perillas beige.
class MiVozLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MiVozLookAndFeel()
    {
        setColour(juce::ComboBox::backgroundColourId, ui::card);
        setColour(juce::ComboBox::outlineColourId, ui::ink);
        setColour(juce::ComboBox::textColourId, ui::ink);
        setColour(juce::ComboBox::arrowColourId, ui::ink);
        setColour(juce::PopupMenu::backgroundColourId, ui::card);
        setColour(juce::PopupMenu::textColourId, ui::ink);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, ui::salmon);
        setColour(juce::PopupMenu::highlightedTextColourId, ui::ink);
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float pos,
                          float startAngle, float endAngle, juce::Slider& slider) override
    {
        auto b = juce::Rectangle<float>((float) x, (float) y, (float) width, (float) height).reduced(2.0f);
        const float d = juce::jmin(b.getWidth(), b.getHeight());
        const float cx = b.getCentreX(), cy = b.getCentreY();
        const float ringR = d * 0.5f;
        const float ringW = juce::jmax(3.0f, d * 0.085f);
        const float angle = startAngle + pos * (endAngle - startAngle);
        const float arcR = ringR - ringW * 0.5f;

        juce::Path track;
        track.addCentredArc(cx, cy, arcR, arcR, 0.0f, startAngle, endAngle, true);
        g.setColour(juce::Colour(0xffd9cdb0));
        g.strokePath(track, juce::PathStrokeType(ringW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        if (pos > 0.002f)
        {
            juce::Path val;
            val.addCentredArc(cx, cy, arcR, arcR, 0.0f, startAngle, angle, true);
            g.setColour(slider.findColour(juce::Slider::rotarySliderFillColourId));
            g.strokePath(val, juce::PathStrokeType(ringW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        const float bodyR = ringR - ringW - d * 0.06f;

        g.setColour(ui::ink.withAlpha(0.28f));
        g.fillEllipse(cx - bodyR, cy - bodyR + d * 0.05f, bodyR * 2.0f, bodyR * 2.0f);

        juce::ColourGradient grad(juce::Colour(0xfff3eedf), cx - bodyR * 0.45f, cy - bodyR * 0.55f,
                                  juce::Colour(0xff9c9583), cx + bodyR * 0.6f, cy + bodyR * 0.85f, false);
        g.setGradientFill(grad);
        g.fillEllipse(cx - bodyR, cy - bodyR, bodyR * 2.0f, bodyR * 2.0f);

        g.setColour(ui::ink);
        g.drawEllipse(cx - bodyR, cy - bodyR, bodyR * 2.0f, bodyR * 2.0f, juce::jmax(1.5f, d * 0.035f));

        // bordes estriados
        g.setColour(ui::ink.withAlpha(0.30f));
        for (int i = 0; i < 20; ++i)
        {
            const float a = juce::MathConstants<float>::twoPi * (float) i / 20.0f;
            const float s = std::sin(a), c = -std::cos(a);
            g.drawLine(cx + s * bodyR * 0.84f, cy + c * bodyR * 0.84f,
                       cx + s * bodyR * 0.95f, cy + c * bodyR * 0.95f, 1.2f);
        }

        // aguja
        juce::Path ptr;
        ptr.addRoundedRectangle(-d * 0.035f, -bodyR * 0.86f, d * 0.07f, bodyR * 0.55f, d * 0.03f);
        g.setColour(ui::ink);
        g.fillPath(ptr, juce::AffineTransform::rotation(angle).translated(cx, cy));
    }

    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                          float, float, const juce::Slider::SliderStyle, juce::Slider& slider) override
    {
        const float cy = (float) y + (float) height * 0.5f;
        const float x0 = (float) x + 12.0f, x1 = (float) (x + width) - 12.0f;

        g.setColour(ui::ink.withAlpha(0.20f));
        g.drawLine(x0, cy + 2.0f, x1, cy + 2.0f, 8.0f);
        g.setColour(ui::ink);
        g.drawLine(x0, cy, x1, cy, 8.0f);
        g.setColour(juce::Colour(0xffd9cdb0));
        g.drawLine(x0, cy, x1, cy, 4.5f);
        g.setColour(ui::coral);
        g.drawLine(x0, cy, sliderPos, cy, 4.5f);

        const float tw = 26.0f, th = (float) height * 0.85f;
        auto thumb = juce::Rectangle<float>(tw, th).withCentre({ sliderPos, cy });
        g.setColour(ui::ink.withAlpha(0.30f));
        g.fillRoundedRectangle(thumb.translated(0.0f, 3.0f), 7.0f);
        g.setColour(slider.isMouseOverOrDragging() ? ui::salmon.brighter(0.1f) : ui::salmon);
        g.fillRoundedRectangle(thumb, 7.0f);
        g.setColour(ui::ink);
        g.drawRoundedRectangle(thumb, 7.0f, 2.5f);
        g.setColour(ui::ink.withAlpha(0.5f));
        g.drawLine(sliderPos - 4.0f, cy - th * 0.25f, sliderPos - 4.0f, cy + th * 0.25f, 1.5f);
        g.drawLine(sliderPos + 4.0f, cy - th * 0.25f, sliderPos + 4.0f, cy + th * 0.25f, 1.5f);
    }

    void drawComboBox(juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<float>(0.0f, 0.0f, (float) width, (float) height).reduced(3.0f, 4.0f);
        g.setColour(ui::ink.withAlpha(0.30f));
        g.fillRoundedRectangle(r.translated(0.0f, 3.0f), 8.0f);
        g.setColour(box.findColour(juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle(r, 8.0f);
        g.setColour(ui::ink);
        g.drawRoundedRectangle(r, 8.0f, 2.5f);

        juce::Path arrow;
        const float ax = r.getRight() - 26.0f, ay = r.getCentreY();
        arrow.addTriangle(ax, ay - 5.0f, ax + 16.0f, ay - 5.0f, ax + 8.0f, ay + 6.0f);
        g.setColour(ui::ink);
        g.fillPath(arrow);
    }

    juce::Font getComboBoxFont(juce::ComboBox& box) override
    {
        return juce::Font(juce::FontOptions((float) box.getHeight() * 0.42f, juce::Font::bold));
    }

    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds(14, 1, box.getWidth() - 50, box.getHeight() - 2);
        label.setFont(getComboBoxFont(box));
    }
};

//==============================================================================
// Boton redondeado estilo "pastilla" (Bypass, Asistente, Modo adaptativo)
class PillButton : public juce::Button
{
public:
    PillButton(const juce::String& text, juce::Colour base, bool withMic = false)
        : juce::Button(text), baseColour(base), mic(withMic)
    {
        setButtonText(text);
    }

    void paintButton(juce::Graphics& g, bool over, bool down) override
    {
        auto r = getLocalBounds().toFloat().reduced(4.0f, 6.0f);
        const bool pressed = down || getToggleState();
        const float radius = r.getHeight() * 0.5f;

        g.setColour(ui::ink.withAlpha(0.32f));
        g.fillRoundedRectangle(r.translated(0.0f, pressed ? 1.5f : 4.0f), radius);

        auto body = r.translated(0.0f, pressed ? 2.5f : 0.0f);
        auto fill = baseColour;
        if (getToggleState()) fill = fill.darker(0.18f);
        else if (over) fill = fill.brighter(0.08f);
        g.setColour(fill);
        g.fillRoundedRectangle(body, radius);
        g.setColour(ui::ink);
        g.drawRoundedRectangle(body, radius, 2.5f);

        // brillo
        g.setColour(juce::Colours::white.withAlpha(0.25f));
        g.fillRoundedRectangle(body.reduced(body.getWidth() * 0.08f, 0.0f).withHeight(body.getHeight() * 0.35f).translated(0.0f, 3.0f),
                               radius * 0.5f);

        auto textArea = body;
        if (mic)
        {
            const float s = body.getHeight() * 0.42f;
            const float ix = body.getX() + body.getWidth() * 0.12f;
            const float iy = body.getCentreY();
            juce::Path m;
            m.addRoundedRectangle(ix - s * 0.22f, iy - s * 0.55f, s * 0.44f, s * 0.8f, s * 0.2f);
            g.setColour(ui::ink);
            g.fillPath(m);
            juce::Path hold;
            hold.addCentredArc(ix, iy - s * 0.05f, s * 0.42f, s * 0.42f, 0.0f,
                               juce::MathConstants<float>::halfPi, juce::MathConstants<float>::pi * 1.5f, true);
            g.strokePath(hold, juce::PathStrokeType(s * 0.12f));
            g.drawLine(ix, iy + s * 0.37f, ix, iy + s * 0.6f, s * 0.12f);
            textArea.removeFromLeft(body.getWidth() * 0.18f);
        }

        g.setColour(ui::ink);
        g.setFont(juce::FontOptions(body.getHeight() * 0.40f, juce::Font::bold));
        g.drawFittedText(getButtonText().toUpperCase(), textArea.toNearestInt(), juce::Justification::centred, 1, 1.0f);
    }

private:
    juce::Colour baseColour;
    bool mic;
};

//==============================================================================
// Boton grande rojo del Sidechain
class BigButton : public juce::Button
{
public:
    explicit BigButton(const juce::String& text) : juce::Button(text) { setButtonText(text); }

    void paintButton(juce::Graphics& g, bool over, bool down) override
    {
        const bool on = getToggleState();
        auto plate = getLocalBounds().toFloat().reduced(4.0f);

        g.setColour(ui::ink.withAlpha(0.25f));
        g.fillRoundedRectangle(plate.translated(0.0f, 4.0f), 16.0f);
        g.setColour(juce::Colour(0xffddd3ba));
        g.fillRoundedRectangle(plate, 16.0f);
        g.setColour(ui::ink);
        g.drawRoundedRectangle(plate, 16.0f, 3.0f);

        auto cap = plate.reduced(plate.getWidth() * 0.07f, plate.getHeight() * 0.13f);
        const bool pressed = down || !on;
        if (!pressed)
            cap = cap.translated(0.0f, -2.0f);
        else
            cap = cap.translated(0.0f, 3.0f);

        if (on)
        {
            g.setColour(juce::Colour(0xffff5a40).withAlpha(0.28f));
            g.fillRoundedRectangle(cap.expanded(8.0f), 18.0f);
        }

        juce::Colour top = on ? juce::Colour(0xfff56f58) : juce::Colour(0xffa86058);
        juce::Colour bot = on ? juce::Colour(0xffc7352a) : juce::Colour(0xff733b36);
        if (over && !on) { top = top.brighter(0.08f); bot = bot.brighter(0.08f); }
        g.setGradientFill(juce::ColourGradient(top, cap.getX(), cap.getY(), bot, cap.getX(), cap.getBottom(), false));
        g.fillRoundedRectangle(cap, 12.0f);
        g.setColour(ui::ink);
        g.drawRoundedRectangle(cap, 12.0f, 3.0f);

        g.setColour(juce::Colours::white.withAlpha(on ? 0.30f : 0.12f));
        g.fillRoundedRectangle(cap.reduced(cap.getWidth() * 0.06f, 0.0f).withHeight(cap.getHeight() * 0.32f).translated(0.0f, 4.0f), 8.0f);

        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions(cap.getHeight() * 0.30f, juce::Font::bold));
        g.drawFittedText(getButtonText().toUpperCase(),
                         cap.withTrimmedBottom(cap.getHeight() * 0.22f).toNearestInt(), juce::Justification::centred, 1, 1.0f);

        g.setFont(juce::FontOptions(cap.getHeight() * 0.17f, juce::Font::bold));
        g.setColour(juce::Colours::white.withAlpha(on ? 1.0f : 0.7f));
        g.drawFittedText(on ? "ON" : "OFF", cap.removeFromBottom(cap.getHeight() * 0.30f).toNearestInt(),
                         juce::Justification::centred, 1, 1.0f);
    }
};

//==============================================================================
// Perilla con nombre y valor debajo
class KnobComp : public juce::Component
{
public:
    KnobComp(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId,
             const juce::String& labelText, juce::Colour arcColour)
        : label(labelText.toUpperCase())
    {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        slider.setColour(juce::Slider::rotarySliderFillColourId, arcColour);
        slider.setMouseDragSensitivity(220);
        slider.onValueChange = [this] { repaint(); };
        addAndMakeVisible(slider);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, paramId, slider);
    }

    void resized() override
    {
        const float s = (float) getWidth() / 102.0f;
        const int textH = juce::roundToInt(34.0f * s);
        slider.setBounds(getLocalBounds().withTrimmedBottom(textH));
    }

    void paint(juce::Graphics& g) override
    {
        const float s = (float) getWidth() / 102.0f;
        const int textH = juce::roundToInt(34.0f * s);
        auto area = getLocalBounds().removeFromBottom(textH);

        g.setColour(ui::ink);
        g.setFont(juce::FontOptions(11.5f * s, juce::Font::bold));
        g.drawFittedText(label, area.removeFromTop(textH / 2), juce::Justification::centred, 1, 1.0f);

        g.setColour(ui::inkSoft);
        g.setFont(juce::FontOptions(12.5f * s, juce::Font::bold));
        g.drawFittedText(slider.getTextFromValue(slider.getValue()), area, juce::Justification::centredTop, 1, 1.0f);
    }

private:
    juce::String label;
    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

//==============================================================================
// Grafica: espectro de la voz, curva del EQ y ruido detectado
class GraphComp : public juce::Component, private juce::Timer
{
public:
    explicit GraphComp(MiVozAudioProcessor& p) : proc(p), fft(10)
    {
        for (int i = 0; i < kN; ++i)
            window[(size_t) i] = 0.5f * (1.0f - std::cos(juce::MathConstants<float>::twoPi * (float) i / (float) kN));
        spec.fill(-140.0f);
        startTimerHz(30);
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        const float s = bounds.getWidth() / 1040.0f;

        g.setColour(ui::ink.withAlpha(0.28f));
        g.fillRoundedRectangle(bounds.reduced(3.0f).translated(0.0f, 4.0f), 14.0f * s);
        g.setColour(ui::screen);
        g.fillRoundedRectangle(bounds.reduced(3.0f), 14.0f * s);
        g.setColour(ui::screenInk);
        g.drawRoundedRectangle(bounds.reduced(3.0f), 14.0f * s, 3.0f);

        // titulo y leyenda
        g.setColour(ui::screenInk);
        g.setFont(juce::FontOptions(21.0f * s, juce::Font::bold));
        g.drawText("ESPECTRO DE TU VOZ", juce::Rectangle<float>(26.0f * s, 12.0f * s, 400.0f * s, 30.0f * s),
                   juce::Justification::centredLeft);

        g.setFont(juce::FontOptions(14.0f * s, juce::Font::bold));
        float lx = bounds.getWidth() - 26.0f * s;
        auto legend = [&](const juce::String& text, juce::Colour c, bool dashed, float w)
        {
            const float tw = w * s;
            lx -= tw;
            g.setColour(ui::screenInk);
            g.drawText(text, juce::Rectangle<float>(lx + 34.0f * s, 12.0f * s, tw - 34.0f * s, 30.0f * s), juce::Justification::centredLeft);
            g.setColour(c);
            if (dashed)
            {
                g.drawLine(lx, 27.0f * s, lx + 8.0f * s, 27.0f * s, 2.5f);
                g.drawLine(lx + 14.0f * s, 27.0f * s, lx + 22.0f * s, 27.0f * s, 2.5f);
            }
            else
                g.drawLine(lx, 27.0f * s, lx + 24.0f * s, 27.0f * s, 4.0f);
            lx -= 14.0f * s;
        };
        legend("RUIDO DETECTADO", ui::screenInk, true, 190.0f);
        legend("CURVA DEL EQ", ui::screenInk, false, 160.0f);
        legend("TU VOZ", ui::tealLight, false, 100.0f);

        // zona de dibujo
        auto plot = bounds.reduced(18.0f * s, 0.0f);
        plot.removeFromTop(52.0f * s);
        plot.removeFromBottom(24.0f * s);
        const float W = plot.getWidth(), Hh = plot.getHeight();

        auto xOf = [&](float f) { return plot.getX() + std::log(f / kFMin) / std::log(kFMax / kFMin) * W; };
        auto yOfDb = [&](float db) { return plot.getBottom() - juce::jlimit(0.0f, 1.0f, (db - kDbMin) / (kDbMax - kDbMin)) * Hh; };

        // cuadricula
        g.setColour(juce::Colour(0xff2b3136));
        for (int i = 1; i < 4; ++i)
            g.drawHorizontalLine(juce::roundToInt(plot.getY() + Hh * (float) i / 4.0f), plot.getX(), plot.getRight());
        g.setFont(juce::FontOptions(12.0f * s));
        const float marks[] = { 100.0f, 1000.0f, 10000.0f };
        const char* names[] = { "100 Hz", "1 kHz", "10 kHz" };
        for (int i = 0; i < 3; ++i)
        {
            const float x = xOf(marks[i]);
            g.setColour(juce::Colour(0xff3a4046));
            g.drawVerticalLine(juce::roundToInt(x), plot.getY(), plot.getBottom());
            g.setColour(juce::Colour(0xff9aa0a5));
            g.drawText(names[i], juce::Rectangle<float>(x - 40.0f * s, plot.getBottom() + 3.0f * s, 80.0f * s, 18.0f * s), juce::Justification::centred);
        }

        const double sr = juce::jmax(8000.0, proc.getCurrentSampleRate());

        // espectro
        juce::Path line;
        const float step = juce::jmax(1.5f, 2.0f * s);
        bool first = true;
        for (float x = 0.0f; x <= W; x += step)
        {
            const float f = kFMin * std::pow(kFMax / kFMin, x / W);
            const float db = specAt(f, sr);
            const float y = yOfDb(db);
            if (first) { line.startNewSubPath(plot.getX() + x, y); first = false; }
            else line.lineTo(plot.getX() + x, y);
        }
        juce::Path area(line);
        area.lineTo(plot.getRight(), plot.getBottom());
        area.lineTo(plot.getX(), plot.getBottom());
        area.closeSubPath();
        g.setGradientFill(juce::ColourGradient(ui::tealLight.withAlpha(0.50f), 0.0f, plot.getY(),
                                               ui::tealLight.withAlpha(0.03f), 0.0f, plot.getBottom(), false));
        g.fillPath(area);
        g.setColour(ui::tealLight);
        g.strokePath(line, juce::PathStrokeType(2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // ruido detectado (punteado)
        {
            juce::Path noise;
            bool nFirst = true;
            for (float x = 0.0f; x <= W; x += step * 2.0f)
            {
                const float f = kFMin * std::pow(kFMax / kFMin, x / W);
                const float b = f * (float) SpectralDenoiser::N / (float) sr;
                const int bi = juce::jlimit(1, SpectralDenoiser::numBins - 2, juce::roundToInt(b));
                const float db = (proc.getNoiseDb(bi - 1) + proc.getNoiseDb(bi) + proc.getNoiseDb(bi + 1)) / 3.0f;
                if (db < -130.0f)
                    continue;
                const float y = yOfDb(db);
                if (nFirst) { noise.startNewSubPath(plot.getX() + x, y); nFirst = false; }
                else noise.lineTo(plot.getX() + x, y);
            }
            if (!noise.isEmpty())
            {
                juce::Path dashed;
                const float dashes[] = { 7.0f * s, 6.0f * s };
                juce::PathStrokeType(1.8f).createDashedStroke(dashed, noise, dashes, 2);
                g.setColour(ui::screenInk.withAlpha(0.85f));
                g.fillPath(dashed);
            }
        }

        // curva del EQ (blanca)
        {
            auto hp = Coeffs::makeHighPass(sr, getParam("hpf"), 0.707f);
            auto mud = Coeffs::makePeakFilter(sr, 320.0f, 1.0f, juce::Decibels::decibelsToGain(getParam("mud")));
            auto pres = Coeffs::makePeakFilter(sr, 3500.0f, 0.8f, juce::Decibels::decibelsToGain(getParam("presence")));
            auto air = Coeffs::makeHighShelf(sr, 11000.0f, 0.707f, juce::Decibels::decibelsToGain(getParam("air")));

            juce::Path eq;
            bool eFirst = true;
            for (float x = 0.0f; x <= W; x += step * 2.0f)
            {
                const float f = kFMin * std::pow(kFMax / kFMin, x / W);
                const double mag = hp->getMagnitudeForFrequency(f, sr) * mud->getMagnitudeForFrequency(f, sr)
                                 * pres->getMagnitudeForFrequency(f, sr) * air->getMagnitudeForFrequency(f, sr);
                const float db = juce::jlimit(-30.0f, 30.0f, 20.0f * std::log10((float) mag + 1.0e-6f));
                const float y = plot.getY() + Hh * 0.42f - db * (Hh / 44.0f);
                if (eFirst) { eq.startNewSubPath(plot.getX() + x, y); eFirst = false; }
                else eq.lineTo(plot.getX() + x, y);
            }
            g.setColour(ui::screenInk);
            g.strokePath(eq, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

private:
    using Coeffs = juce::dsp::IIR::Coefficients<float>;

    static constexpr int kN = 1024;
    static constexpr float kFMin = 30.0f, kFMax = 18000.0f;
    static constexpr float kDbMin = -110.0f, kDbMax = -20.0f;

    float getParam(const char* id) const { return proc.apvts.getRawParameterValue(id)->load(); }

    // valor del espectro (dB) alrededor de una frecuencia, promediando en 1/6 de octava
    float specAt(float f, double sr) const
    {
        const float b = f * (float) kN / (float) sr;
        const float lo = b / 1.09f, hi = b * 1.09f;
        if (hi - lo < 1.0f)
        {
            const int i0 = juce::jlimit(0, kN / 2 - 1, (int) b);
            const float fr = b - (float) i0;
            return spec[(size_t) i0] * (1.0f - fr) + spec[(size_t) (i0 + 1)] * fr;
        }
        const int a = juce::jmax(1, (int) std::ceil(lo));
        const int c = juce::jmin(kN / 2, (int) std::floor(hi));
        float sum = 0.0f;
        int cnt = 0;
        for (int i = a; i <= c; ++i, ++cnt)
            sum += std::pow(10.0f, spec[(size_t) i] / 10.0f);
        return cnt > 0 ? 10.0f * std::log10(sum / (float) cnt + 1.0e-14f) : -140.0f;
    }

    void timerCallback() override
    {
        std::array<float, MiVozAudioProcessor::scopeSize> buf {};
        proc.copyScope(buf);

        for (int i = 0; i < kN; ++i)
            fftData[(size_t) i] = buf[(size_t) i] * window[(size_t) i];
        for (int i = kN; i < 2 * kN; ++i)
            fftData[(size_t) i] = 0.0f;

        fft.performRealOnlyForwardTransform(fftData.data(), true);

        for (int k = 0; k <= kN / 2; ++k)
        {
            const float re = fftData[(size_t) (2 * k)], im = fftData[(size_t) (2 * k + 1)];
            const float db = 20.0f * std::log10(std::sqrt(re * re + im * im) * 4.0f / (float) kN + 1.0e-9f);
            float& sv = spec[(size_t) k];
            sv = db > sv ? sv + (db - sv) * 0.6f : sv - 2.0f;
            sv = juce::jmax(sv, -140.0f);
        }
        repaint();
    }

    MiVozAudioProcessor& proc;
    juce::dsp::FFT fft;
    std::array<float, kN> window {};
    std::array<float, 2 * kN> fftData {};
    std::array<float, kN / 2 + 1> spec {};
};

//==============================================================================
// "Gema" que se ilumina con el nivel de salida
class GemComp : public juce::Component, private juce::Timer
{
public:
    explicit GemComp(MiVozAudioProcessor& p) : proc(p) { startTimerHz(30); }

    void paint(juce::Graphics& g) override
    {
        const float s = (float) getWidth() / 102.0f;
        const int textH = juce::roundToInt(34.0f * s);
        auto top = getLocalBounds().withTrimmedBottom(textH).toFloat().reduced(4.0f);
        const float d = juce::jmin(top.getWidth(), top.getHeight()) * 0.92f;
        const float cx = top.getCentreX(), cy = top.getCentreY();
        const float R = d * 0.5f;

        const float t = juce::jlimit(0.0f, 1.0f, (holdDb + 40.0f) / 40.0f);
        const bool hot = holdDb > -0.2f;
        const juce::Colour dark(0xff1f6667), bright(0xff7ff0e8);
        const juce::Colour body = hot ? ui::coral : dark.interpolatedWith(bright, t);

        juce::Path oct;
        for (int i = 0; i < 8; ++i)
        {
            const float a = juce::MathConstants<float>::twoPi * ((float) i + 0.5f) / 8.0f;
            const float px = cx + R * std::sin(a), py = cy - R * std::cos(a);
            if (i == 0) oct.startNewSubPath(px, py); else oct.lineTo(px, py);
        }
        oct.closeSubPath();

        g.setColour(ui::ink.withAlpha(0.28f));
        g.fillPath(oct, juce::AffineTransform::translation(0.0f, 4.0f));
        g.setGradientFill(juce::ColourGradient(body.brighter(0.5f), cx - R * 0.4f, cy - R * 0.6f,
                                               body.darker(0.4f), cx + R * 0.5f, cy + R * 0.8f, false));
        g.fillPath(oct);

        // facetas
        g.setColour(ui::ink.withAlpha(0.45f));
        const float r2 = R * 0.52f;
        for (int i = 0; i < 8; ++i)
        {
            const float a = juce::MathConstants<float>::twoPi * ((float) i + 0.5f) / 8.0f;
            g.drawLine(cx + R * std::sin(a), cy - R * std::cos(a), cx + r2 * std::sin(a), cy - r2 * std::cos(a), 1.3f);
        }
        juce::Path inner;
        for (int i = 0; i < 8; ++i)
        {
            const float a = juce::MathConstants<float>::twoPi * ((float) i + 0.5f) / 8.0f;
            const float px = cx + r2 * std::sin(a), py = cy - r2 * std::cos(a);
            if (i == 0) inner.startNewSubPath(px, py); else inner.lineTo(px, py);
        }
        inner.closeSubPath();
        g.setColour(juce::Colours::white.withAlpha(0.20f));
        g.fillPath(inner);
        g.setColour(ui::ink.withAlpha(0.45f));
        g.strokePath(inner, juce::PathStrokeType(1.3f));
        g.setColour(ui::ink);
        g.strokePath(oct, juce::PathStrokeType(juce::jmax(2.0f, d * 0.04f)));

        auto area = getLocalBounds().removeFromBottom(textH);
        g.setFont(juce::FontOptions(11.5f * s, juce::Font::bold));
        g.drawFittedText(MV_U8("PICO SALIDA"), area.removeFromTop(textH / 2), juce::Justification::centred, 1, 1.0f);
        g.setColour(ui::inkSoft);
        g.setFont(juce::FontOptions(12.5f * s, juce::Font::bold));
        g.drawFittedText(holdDb < -99.0f ? juce::String("-inf dB") : juce::String(holdDb, 1) + " dB", area,
                         juce::Justification::centredTop, 1, 1.0f);
    }

private:
    void timerCallback() override
    {
        const float pk = juce::jmax(proc.peakL.exchange(0.0f), proc.peakR.exchange(0.0f));
        const float db = pk > 1.0e-5f ? juce::Decibels::gainToDecibels(pk) : -100.0f;
        holdDb = db > holdDb ? db : juce::jmax(-100.0f, holdDb - 0.6f);
        repaint();
    }

    MiVozAudioProcessor& proc;
    float holdDb = -100.0f;
};
