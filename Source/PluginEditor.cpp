#include "PluginEditor.h"

namespace
{
    constexpr int kCols = 5;
    constexpr int kRows = 3;
    constexpr int kCellW = 120;
    constexpr int kCellH = 130;
    constexpr int kTop = 90;
    constexpr int kBottom = 50;
}

MiVozAudioProcessorEditor::MiVozAudioProcessorEditor(MiVozAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    // fila 1: EQ y salida
    addKnob("hpf",        "Low Cut",   " Hz");
    addKnob("mud",        "Limpiar",   " dB");
    addKnob("presence",   "Presencia", " dB");
    addKnob("air",        "Aire",      " dB");
    addKnob("out",        "Salida",    " dB");
    // fila 2: dinamica
    addKnob("compThresh", "Comp Umbral", " dB");
    addKnob("compRatio",  "Comp Ratio",  ":1");
    addKnob("deessThr",   "DeEss Umbral"," dB");
    addKnob("deessAmt",   "DeEss Fuerza","");
    addKnob("sat",        "Saturacion",  "");
    // fila 3: efectos
    addKnob("reverbMix",  "Reverb",      "");
    addKnob("reverbSize", "Tamano",      "");
    addKnob("delayMix",   "Delay",       "");
    addKnob("delayTime",  "Tiempo",      " ms");
    addKnob("delayFb",    "Feedback",    "");

    addAndMakeVisible(learnButton);
    learnButton.onClick = [this]
    {
        if (audioProcessor.isLearning())
            return;
        audioProcessor.startLearning();
        status.setText("Escuchando... canta o habla con tu voz normal", juce::dontSendNotification);
        startTimerHz(10);
    };

    status.setJustificationType(juce::Justification::centred);
    status.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(status);

    setSize(kCols * kCellW, kTop + kRows * kCellH + kBottom);
}

void MiVozAudioProcessorEditor::addKnob(const juce::String& id, const juce::String& text, const juce::String& suffix)
{
    auto k = std::make_unique<Knob>();

    k->slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 18);
    k->slider.setTextValueSuffix(suffix);
    k->slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xffff9f1c));

    k->label.setText(text, juce::dontSendNotification);
    k->label.setJustificationType(juce::Justification::centred);
    k->label.setColour(juce::Label::textColourId, juce::Colours::white);

    k->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, id, k->slider);

    addAndMakeVisible(k->slider);
    addAndMakeVisible(k->label);
    knobs.push_back(std::move(k));
}

void MiVozAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff15171c));

    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(24.0f, juce::Font::bold));
    g.drawText("MiVoz", 15, 8, 200, 32, juce::Justification::centredLeft);

    g.setColour(juce::Colour(0xffff9f1c));
    g.setFont(juce::FontOptions(12.0f));
    const char* rows[kRows] = { "EQ / SALIDA", "DINAMICA", "EFECTOS" };
    for (int r = 0; r < kRows; ++r)
        g.drawText(rows[r], 15, kTop + r * kCellH - 14, 200, 14, juce::Justification::centredLeft);
}

void MiVozAudioProcessorEditor::resized()
{
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        const int col = (int) i % kCols;
        const int row = (int) i / kCols;
        auto cell = juce::Rectangle<int>(col * kCellW, kTop + row * kCellH, kCellW, kCellH).reduced(6, 2);
        knobs[i]->label.setBounds(cell.removeFromTop(18));
        knobs[i]->slider.setBounds(cell);
    }

    learnButton.setBounds(getWidth() - 235, 12, 220, 30);
    status.setBounds(0, getHeight() - kBottom + 10, getWidth(), 24);
}

void MiVozAudioProcessorEditor::timerCallback()
{
    MiVozAudioProcessor::LearnResult r;
    if (audioProcessor.pollLearnResult(r))
    {
        stopTimer();
        applyLearnResult(r);
    }
    else if (audioProcessor.isLearning())
    {
        status.setText("Escuchando... " + juce::String((int) (audioProcessor.getLearnProgress() * 100.0f)) + "%",
                       juce::dontSendNotification);
    }
}

void MiVozAudioProcessorEditor::applyLearnResult(const MiVozAudioProcessor::LearnResult& r)
{
    if (!r.ok)
    {
        status.setText("No se escucho suficiente voz. Revisa que entre audio y vuelve a intentar.",
                       juce::dontSendNotification);
        return;
    }

    auto setParam = [this](const juce::String& id, float v)
    {
        if (auto* prm = audioProcessor.apvts.getParameter(id))
            prm->setValueNotifyingHost(prm->convertTo0to1(v));
    };

    setParam("compThresh", r.compThresh);
    setParam("deessThr", r.deessThr);
    setParam("deessAmt", r.deessAmt);
    status.setText("Listo: ajuste el compresor y el de-esser a tu voz. Afina a gusto.",
                   juce::dontSendNotification);
}
