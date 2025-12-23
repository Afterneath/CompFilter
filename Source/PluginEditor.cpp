#include "PluginProcessor.h"
#include "PluginEditor.h"

CompFilterAudioProcessorEditor::CompFilterAudioProcessorEditor (CompFilterAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setSize (1200, 750); 
    setLookAndFeel(&mellowLook);

    addAndMakeVisible(inMeter);
    addAndMakeVisible(outMeter);
    addAndMakeVisible(darkVoid);
    darkVoid.toBack(); 

    auto setup = [&](juce::Slider& s, std::string id, std::string name, int darknessLevel)
    {
        addAndMakeVisible(s);
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 20);
        
        juce::Colour base = juce::Colours::lightgrey;
        if (darknessLevel == 1) base = juce::Colours::grey;
        
        s.setColour(juce::Slider::thumbColourId, base);
        
        if (!name.empty()) 
        {
            auto* l = new juce::Label();
            l->setText(name, juce::dontSendNotification);
            l->setJustificationType(juce::Justification::centred);
            l->attachToComponent(&s, false);
            labels.push_back(l);
        }
        attachments.push_back(std::make_unique<SliderAttachment>(audioProcessor.apvts, id, s));
    };

    setup(inGainS, "inGain", "Input", 0);
    setup(outGainS, "outGain", "Output", 0);
    setup(threshS, "threshold", "Threshold", 0);
    setup(kneeS, "knee", "Knee", 0);
    
    // Central Gravity Knob - No Label attached (Manual)
    setup(pullS, "pull", "", 1); 
    pullS.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    
    setup(pullAttS, "pullAttack", "Attack", 2);
    setup(pullRelS, "pullRelease", "Release", 2);
    setup(lpfS, "lpfCutoff", "LPF Base", 2);
    setup(hpfS, "hpfCutoff", "HPF Base", 2);
    
    // Anti Gravity Knobs
    setup(antiGravLpfS, "antiGravLpf", "LPF Resist", 1);
    setup(antiGravHpfS, "antiGravHpf", "HPF Resist", 1);

    startTimerHz(60); 
}

CompFilterAudioProcessorEditor::~CompFilterAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
    stopTimer();
    for (auto* l : labels) delete l;
}

void CompFilterAudioProcessorEditor::timerCallback()
{
    inMeter.setLevel(audioProcessor.currentInputRMS * 2.5f);
    outMeter.setLevel(audioProcessor.currentOutputRMS * 2.5f);
    darkVoid.setActivity(audioProcessor.currentFilterMix, audioProcessor.currentPullAmount);
    repaint(); 
}

// Draw the web of connections
void CompFilterAudioProcessorEditor::drawConnectingLines(juce::Graphics& g)
{
    // Points
    auto centerP = pullS.getBounds().getCentre().toFloat();
    auto lpfResistP = antiGravLpfS.getBounds().getCentre().toFloat();
    auto hpfResistP = antiGravHpfS.getBounds().getCentre().toFloat();
    auto lpfBaseP = lpfS.getBounds().getCentre().toFloat();
    auto hpfBaseP = hpfS.getBounds().getCentre().toFloat();

    // 1. Center -> AntiGravity (Always visible)
    g.setColour(juce::Colours::white.withAlpha(0.6f));
    g.drawLine(juce::Line<float>(centerP, lpfResistP), 2.0f);
    g.drawLine(juce::Line<float>(centerP, hpfResistP), 2.0f);

    // 2. AntiGravity -> Filter Base (Variable Opacity based on Resist)
    
    // Left side
    float lpfResistVal = antiGravLpfS.getValue() / 100.0f; // 0 to 1
    float lpfLineAlpha = 1.0f - lpfResistVal; // 0% Resist = 100% Alpha
    if (lpfLineAlpha < 0.1f) lpfLineAlpha = 0.1f; // min visibility
    
    g.setColour(juce::Colours::white.withAlpha(lpfLineAlpha * 0.8f));
    float lpfThick = 1.0f + (lpfLineAlpha * 3.0f); // Thicker if stronger
    g.drawLine(juce::Line<float>(lpfResistP, lpfBaseP), lpfThick);

    // Right side
    float hpfResistVal = antiGravHpfS.getValue() / 100.0f; 
    float hpfLineAlpha = 1.0f - hpfResistVal;
    
    g.setColour(juce::Colours::white.withAlpha(hpfLineAlpha * 0.8f));
    float hpfThick = 1.0f + (hpfLineAlpha * 3.0f);
    g.drawLine(juce::Line<float>(hpfResistP, hpfBaseP), hpfThick);
}

void CompFilterAudioProcessorEditor::drawFrequencyOverlay(juce::Graphics& g, juce::Slider& slider, float normalizedTarget)
{
    auto bounds = slider.getBounds().toFloat();
    auto center = bounds.getCentre();
    float radius = (juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f);
    
    float rotaryStart = juce::MathConstants<float>::pi * 0.75f;
    float rotaryEnd   = juce::MathConstants<float>::pi * 2.25f;
    float currentNorm = slider.valueToProportionOfLength(slider.getValue());
    
    float angleCurrent = rotaryStart + (currentNorm * (rotaryEnd - rotaryStart));
    float angleTarget  = rotaryStart + (normalizedTarget * (rotaryEnd - rotaryStart));

    if (std::abs(angleCurrent - angleTarget) > 0.01f)
    {
        g.setColour(juce::Colours::white.withAlpha(0.4f));
        float innerR = radius * 0.85f; 
        float outerR = radius * 1.15f; 
        
        juce::Path strokeArc;
        strokeArc.addCentredArc(center.x, center.y, (innerR + outerR) * 0.5f, (innerR + outerR) * 0.5f, 
                                0.0f, angleCurrent, angleTarget, true);
        g.strokePath(strokeArc, juce::PathStrokeType(outerR - innerR, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
    }
}

void CompFilterAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll(juce::Colour::fromRGB(20, 20, 20)); // Deep Dark

    // Connecting Lines (Behind Knobs)
    drawConnectingLines(g);

    // Boxes
    g.setColour(juce::Colours::white.withAlpha(0.05f));
    auto bounds = getLocalBounds();
    g.drawRect(bounds.removeFromLeft(120).reduced(10), 1.0f);
    bounds = getLocalBounds();
    g.drawRect(bounds.removeFromRight(120).reduced(10), 1.0f);
    
    // Title
    g.setColour(juce::Colours::white.withAlpha(0.9f));
    g.setFont(32.0f);
    auto center = getLocalBounds().getCentre();
    // Spaced out text
    g.drawText("G  R  A  V  I  T  Y", center.x - 200, center.y - 280, 400, 40, juce::Justification::centred);

    if (audioProcessor.currentPullAmount > 0.01f) {
        drawFrequencyOverlay(g, lpfS, audioProcessor.monitorLpfNorm);
        drawFrequencyOverlay(g, hpfS, audioProcessor.monitorHpfNorm);
    }
}

void CompFilterAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();

    // 1. GAIN COLUMNS
    auto leftArea = area.removeFromLeft(120).reduced(10);
    inMeter.setBounds(leftArea.removeFromLeft(20));
    inGainS.setBounds(leftArea.removeFromTop(100).translated(10, 50));

    auto rightArea = area.removeFromRight(120).reduced(10);
    outMeter.setBounds(rightArea.removeFromRight(20));
    outGainS.setBounds(rightArea.removeFromTop(100).translated(-10, 50));

    // 2. THE VOID
    auto center = area;
    int voidSize = 900; 
    darkVoid.setBounds(center.getCentreX() - voidSize/2, center.getCentreY() - voidSize/2, voidSize, voidSize);

    // 3. KNOB LAYOUT (The Web)
    auto buttonArea = area.reduced(20);
    int midY = buttonArea.getCentreY();
    int midX = buttonArea.getCentreX();

    // Gravity Knob (Center)
    int gravitySize = 200;
    pullS.setBounds(midX - gravitySize/2, midY - gravitySize/2, gravitySize, gravitySize);

    // X Offsets
    int xInner = 240; // Anti-Gravity Knobs
    int xOuter = 400; // Filter Knobs
    int yLow = midY + 120; // Lower row for filters

    // Left Wing (LPF Chain)
    antiGravLpfS.setBounds(midX - xInner, midY - 60, 100, 100);
    lpfS.setBounds(midX - xOuter, yLow, 120, 120);

    // Right Wing (HPF Chain)
    antiGravHpfS.setBounds(midX + xInner - 100, midY - 60, 100, 100);
    hpfS.setBounds(midX + xOuter - 120, yLow, 120, 120);

    // Utilities (Threshold/Attack/Release/Knee) - Pushed to corners or bottom
    // Let's put them below the main action
    int yUtil = midY + 260;
    int xUtilOffset = 150;

    threshS.setBounds(midX - xUtilOffset - 180, yUtil, 80, 80);
    kneeS.setBounds(midX - xUtilOffset - 80, yUtil, 80, 80);
    
    pullAttS.setBounds(midX + xUtilOffset, yUtil, 80, 80);
    pullRelS.setBounds(midX + xUtilOffset + 100, yUtil, 80, 80);
}
