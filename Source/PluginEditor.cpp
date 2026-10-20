#include "PluginEditor.h"

namespace
{
    struct CardInfo
    {
        const char* title;
        const char* hint;
        juce::Colour dot;
    };

    const CardInfo kCards[6] = {
        { "1. LIMPIEZA",         "Quita el ruido de fondo. El asistente lo ajusta solo.", juce::Colour(0xffe9b44c) },
        { "2. COLOR Y CUERPO",   "Mira la curva blanca del gr\xc3\xa1" "fico: es lo que le haces a tu voz.", juce::Colour(0xffe9b44c) },
        { "3. ESPACIO Y FX",     "Cuerpo y espacio. Poquito, para que no tape la voz.", juce::Colour(0xff4aa8a8) },
        { "4. DIN\xc3\x81MICA",  "", juce::Colour(0xff8a8a80) },
        { "5. SIDECHAIN",        "Un toque para activar o desactivar. Baja el reverb y el delay mientras cantas.", juce::Colour(0xffd9503f) },
        { "6. SALIDA",           "Techo evita que se pase de 0 dB. Compara con Bypass.", juce::Colour(0xff4aa8a8) }
    };

    constexpr int kCardX[3] = { 30, 385, 740 };
    constexpr int kCardY[2] = { 392, 636 };
    constexpr int kCardW = 330;
    constexpr int kCardH = 224;

    struct Preset
    {
        const char* name;
        std::vector<std::pair<const char*, float>> values;
    };

    const std::vector<Preset>& presets()
    {
        static const std::vector<Preset> list = {
            { "MiVoz (inicio)", { { "hpf", 80 }, { "nrAmount", 0.45f }, { "nrAdaptive", 1 }, { "gate", -52 }, { "mud", -2 }, { "presence", 2 },
                                  { "air", 2 }, { "sat", 0.15f }, { "reverbMix", 0.12f }, { "delayMix", 0 }, { "compThresh", -16 },
                                  { "compRatio", 3 }, { "compAttack", 10 }, { "compRelease", 300 }, { "deessThr", -28 }, { "deessAmt", 0.5f },
                                  { "out", 0 }, { "ceiling", -0.3f } } },
            { "Trap limpio",    { { "hpf", 90 }, { "nrAmount", 0.55f }, { "gate", -48 }, { "mud", -3 }, { "presence", 3 }, { "air", 3 },
                                  { "sat", 0.25f }, { "reverbMix", 0.10f }, { "delayMix", 0.12f }, { "delayTime", 280 }, { "delayFb", 0.3f },
                                  { "compThresh", -20 }, { "compRatio", 4 }, { "compAttack", 6 }, { "compRelease", 200 },
                                  { "deessThr", -28 }, { "deessAmt", 0.6f } } },
            { "Voz c\xc3\xa1lida",
                                { { "hpf", 70 }, { "nrAmount", 0.4f }, { "gate", -55 }, { "mud", -1 }, { "presence", 0.5f }, { "air", 0.5f },
                                  { "sat", 0.3f }, { "reverbMix", 0.15f }, { "delayMix", 0 }, { "compThresh", -16 }, { "compRatio", 2.5f },
                                  { "compAttack", 15 }, { "compRelease", 250 }, { "deessThr", -30 }, { "deessAmt", 0.4f } } },
            { "Brillante / pop", { { "hpf", 100 }, { "nrAmount", 0.5f }, { "gate", -50 }, { "mud", -3 }, { "presence", 4 }, { "air", 5 },
                                  { "sat", 0.1f }, { "reverbMix", 0.18f }, { "delayMix", 0 }, { "compThresh", -18 }, { "compRatio", 3.5f },
                                  { "compAttack", 8 }, { "compRelease", 200 }, { "deessThr", -30 }, { "deessAmt", 0.7f } } },
            { "Radio / podcast", { { "hpf", 120 }, { "nrAmount", 0.7f }, { "gate", -45 }, { "mud", -4 }, { "presence", 3 }, { "air", 1 },
                                  { "sat", 0.2f }, { "reverbMix", 0 }, { "delayMix", 0 }, { "compThresh", -24 }, { "compRatio", 5 },
                                  { "compAttack", 5 }, { "compRelease", 150 }, { "deessThr", -28 }, { "deessAmt", 0.5f } } }
        };
        return list;
    }
}

MiVozAudioProcessorEditor::MiVozAudioProcessorEditor(MiVozAudioProcessor& p)
    : AudioProcessorEditor(&p), proc(p), graph(p), gem(p)
{
    setLookAndFeel(&laf);

    const auto coral = ui::coral;
    const auto teal = ui::teal;

    // 1. Limpieza
    addKnob(0, "nrAmount", MV_U8("Reducci\xc3\xb3n"), coral);
    addKnob(0, "gate", "Gate", coral);
    addKnob(0, "hpf", "Low cut", coral);
    // 2. Color y cuerpo
    addKnob(1, "mud", "Limpiar", coral);
    addKnob(1, "presence", "Presencia", coral);
    addKnob(1, "air", "Aire", coral);
    // 3. Espacio y FX
    addKnob(2, "sat", MV_U8("Saturaci\xc3\xb3n"), teal);
    addKnob(2, "reverbMix", "Reverb", teal);
    addKnob(2, "delayMix", "Delay", teal);
    // 4. Dinamica
    addKnob(3, "compThresh", "Comp umbral", coral);
    addKnob(3, "compRatio", "Comp ratio", coral);
    addKnob(3, "compAttack", "Ataque", coral);
    addKnob(3, "compRelease", "Release", coral);
    addKnob(3, "deessThr", "De-ess umbral", coral);
    addKnob(3, "deessAmt", "De-ess fuerza", coral);
    // 6. Salida
    addKnob(5, "out", "Salida", teal);
    addKnob(5, "ceiling", "Techo", teal);

    addAndMakeVisible(graph);
    addAndMakeVisible(gem);

    // presets
    {
        int id = 1;
        for (auto& pr : presets())
            presetBox.addItem(juce::String::fromUTF8(pr.name), id++);
        presetBox.setSelectedId(1, juce::dontSendNotification);
        presetBox.onChange = [this] { applyPreset(presetBox.getSelectedId() - 1); };
        addAndMakeVisible(presetBox);
    }

    bypassBtn.setClickingTogglesState(true);
    bypassAtt = std::make_unique<ButtonAtt>(proc.apvts, "bypass", bypassBtn);
    addAndMakeVisible(bypassBtn);

    adaptiveBtn.setClickingTogglesState(true);
    adaptiveAtt = std::make_unique<ButtonAtt>(proc.apvts, "nrAdaptive", adaptiveBtn);
    addAndMakeVisible(adaptiveBtn);

    sideBtn.setClickingTogglesState(true);
    sideAtt = std::make_unique<ButtonAtt>(proc.apvts, "sidechain", sideBtn);
    addAndMakeVisible(sideBtn);

    learnBtn.onClick = [this]
    {
        if (proc.isLearning())
            return;
        proc.startLearning();
        status.setText(MV_U8("Escuchando... canta o habla con tu voz normal"), juce::dontSendNotification);
        startTimerHz(10);
    };
    addAndMakeVisible(learnBtn);

    mixSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    mixSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    mixSlider.onValueChange = [this] { repaint(); };
    mixAtt = std::make_unique<SliderAtt>(proc.apvts, "mix", mixSlider);
    addAndMakeVisible(mixSlider);

    status.setJustificationType(juce::Justification::centredLeft);
    status.setColour(juce::Label::textColourId, ui::ink);
    status.setText(MV_U8("Listo. Pulsa Asistente y canta 6 segundos para ajustar solo."), juce::dontSendNotification);
    addAndMakeVisible(status);

    setResizable(true, true);
    setResizeLimits(660, 552, 1650, 1380);
    getConstrainer()->setFixedAspectRatio((double) kW / (double) kH);
    setSize(kW, kH);
}

MiVozAudioProcessorEditor::~MiVozAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

KnobComp* MiVozAudioProcessorEditor::addKnob(int card, const char* paramId, const juce::String& name, juce::Colour arc)
{
    auto k = std::make_unique<KnobComp>(proc.apvts, paramId, name, arc);
    auto* raw = k.get();
    addAndMakeVisible(*k);
    knobs.push_back(std::move(k));
    cardKnobs[(size_t) card].push_back(raw);
    return raw;
}

juce::Rectangle<int> MiVozAudioProcessorEditor::cardRect(int index) const
{
    return { kCardX[index % 3], kCardY[index / 3], kCardW, kCardH };
}

void MiVozAudioProcessorEditor::setParam(const char* id, float value)
{
    if (auto* prm = proc.apvts.getParameter(id))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost(prm->convertTo0to1(value));
        prm->endChangeGesture();
    }
}

void MiVozAudioProcessorEditor::applyPreset(int index)
{
    const auto& list = presets();
    if (index < 0 || index >= (int) list.size())
        return;
    for (auto& kv : list[(size_t) index].values)
        setParam(kv.first, kv.second);
    status.setText(MV_U8("Preset aplicado. Es un punto de partida: afina con las perillas."), juce::dontSendNotification);
}

void MiVozAudioProcessorEditor::paint(juce::Graphics& g)
{
    const float s = (float) getWidth() / (float) kW;
    g.fillAll(ui::paper);

    g.saveState();
    g.addTransform(juce::AffineTransform::scale(s));

    // hoja cuadriculada
    g.setColour(ui::gridLine);
    for (int x = 0; x <= kW; x += 22)
        g.fillRect(x, 0, 1, kH);
    for (int y = 0; y <= kH; y += 22)
        g.fillRect(0, y, kW, 1);

    // titulo
    g.setColour(ui::ink);
    g.setFont(juce::FontOptions(50.0f, juce::Font::bold));
    g.drawText("MiVoz", juce::Rectangle<float>(30.0f, 14.0f, 200.0f, 66.0f), juce::Justification::centredLeft);
    g.setFont(juce::FontOptions(19.0f, juce::Font::bold));
    g.setColour(ui::inkSoft);
    g.drawText("VOCAL CHAIN", juce::Rectangle<float>(218.0f, 36.0f, 170.0f, 30.0f), juce::Justification::centredLeft);

    // tarjetas
    for (int i = 0; i < 6; ++i)
    {
        auto r = cardRect(i).toFloat();
        g.setColour(ui::ink.withAlpha(0.28f));
        g.fillRoundedRectangle(r.translated(0.0f, 5.0f), 16.0f);
        g.setColour(ui::card);
        g.fillRoundedRectangle(r, 16.0f);
        g.setColour(ui::ink);
        g.drawRoundedRectangle(r, 16.0f, 2.8f);

        // punto de color + titulo
        g.setColour(kCards[i].dot);
        g.fillEllipse(r.getX() + 16.0f, r.getY() + 15.0f, 15.0f, 15.0f);
        g.setColour(ui::ink);
        g.drawEllipse(r.getX() + 16.0f, r.getY() + 15.0f, 15.0f, 15.0f, 2.0f);

        g.setFont(juce::FontOptions(19.0f, juce::Font::bold));
        g.drawText(juce::String::fromUTF8(kCards[i].title), juce::Rectangle<float>(r.getX() + 40.0f, r.getY() + 8.0f, r.getWidth() - 56.0f, 30.0f),
                   juce::Justification::centredLeft);

        // linea bajo el titulo
        g.setColour(ui::ink.withAlpha(0.18f));
        g.fillRect(r.getX() + 16.0f, r.getY() + 40.0f, r.getWidth() - 32.0f, 1.5f);

        // texto de ayuda
        const juce::String hint = juce::String::fromUTF8(kCards[i].hint);
        if (hint.isNotEmpty())
        {
            const bool narrow = (i == 0);
            const int hw = narrow ? 160 : kCardW - 32;
            const int hy = (i == 4) ? 160 : 168;
            g.setColour(ui::inkSoft);
            g.setFont(juce::FontOptions(13.0f));
            g.drawFittedText(hint, juce::Rectangle<int>(cardRect(i).getX() + 16, cardRect(i).getY() + hy, hw, 52),
                             juce::Justification::topLeft, 3, 1.0f);
        }
    }

    // pie: mezcla
    g.setColour(ui::ink);
    g.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    g.drawText("MEZCLA " + mixSlider.getTextFromValue(mixSlider.getValue()),
               juce::Rectangle<float>(30.0f, 868.0f, 150.0f, 34.0f), juce::Justification::centredLeft);

    g.restoreState();
}

void MiVozAudioProcessorEditor::resized()
{
    const float s = (float) getWidth() / (float) kW;
    auto sc = [s](juce::Rectangle<int> r)
    {
        return juce::Rectangle<int>(juce::roundToInt((float) r.getX() * s), juce::roundToInt((float) r.getY() * s),
                                    juce::roundToInt((float) r.getWidth() * s), juce::roundToInt((float) r.getHeight() * s));
    };

    presetBox.setBounds(sc({ 405, 24, 270, 54 }));
    bypassBtn.setBounds(sc({ 695, 22, 130, 58 }));
    learnBtn.setBounds(sc({ 835, 22, 235, 58 }));
    graph.setBounds(sc({ 30, 96, 1040, 282 }));

    for (int c = 0; c < 6; ++c)
    {
        const auto cr = cardRect(c);
        auto& list = cardKnobs[(size_t) c];
        for (size_t i = 0; i < list.size(); ++i)
        {
            if (c == 3) // dinamica: 2 filas de 3
            {
                const int row = (int) i / 3, col = (int) i % 3;
                list[i]->setBounds(sc({ cr.getX() + 12 + col * 102, cr.getY() + 46 + row * 88, 102, 86 }));
            }
            else
            {
                list[i]->setBounds(sc({ cr.getX() + 12 + (int) i * 102, cr.getY() + 46, 102, 118 }));
            }
        }
    }

    const auto c0 = cardRect(0), c4 = cardRect(4), c5 = cardRect(5);
    adaptiveBtn.setBounds(sc({ c0.getX() + 176, c0.getY() + 172, 142, 42 }));
    sideBtn.setBounds(sc({ c4.getX() + 52, c4.getY() + 50, 226, 104 }));
    gem.setBounds(sc({ c5.getX() + 12 + 2 * 102, c5.getY() + 46, 102, 118 }));

    mixSlider.setBounds(sc({ 185, 868, 360, 34 }));
    status.setBounds(sc({ 560, 868, 510, 34 }));
    status.setFont(juce::FontOptions(15.0f * s, juce::Font::bold));
}

void MiVozAudioProcessorEditor::timerCallback()
{
    MiVozAudioProcessor::LearnResult r;
    if (proc.pollLearnResult(r))
    {
        stopTimer();
        applyLearnResult(r);
    }
    else if (proc.isLearning())
    {
        status.setText(MV_U8("Escuchando... ") + juce::String((int) (proc.getLearnProgress() * 100.0f)) + " %",
                       juce::dontSendNotification);
    }
}

void MiVozAudioProcessorEditor::applyLearnResult(const MiVozAudioProcessor::LearnResult& r)
{
    if (!r.ok)
    {
        status.setText(MV_U8("No se escuch\xc3\xb3 suficiente voz. Revisa que entre audio y vuelve a intentar."),
                       juce::dontSendNotification);
        return;
    }

    setParam("compThresh", r.compThresh);
    setParam("deessThr", r.deessThr);
    setParam("deessAmt", r.deessAmt);
    setParam("gate", r.gate);
    setParam("nrAmount", r.nrAmount);
    status.setText(MV_U8("Listo: ajust\xc3\xa9 ruido, compresor y de-esser a tu voz. Afina a gusto."),
                   juce::dontSendNotification);
}
