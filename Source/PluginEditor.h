#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <vector>

class MellowLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MellowLookAndFeel()
    {
        setColour(juce::Slider::thumbColourId, juce::Colours::white);
        setColour(juce::Slider::rotarySliderFillColourId, juce::Colours::grey);
        setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colours::darkgrey);
        setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.9f));
    }
    
    // Outline and Drop Shadow for Rotaries
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                           const float rotaryStartAngle, const float rotaryEndAngle, juce::Slider& slider) override
    {
        // 1. Draw Custom Outline with Shadow
        auto bounds = juce::Rectangle<float>(x, y, width, height).reduced(2.0f);
        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
        auto center = bounds.getCentre();
        auto rx = center.x - radius;
        auto ry = center.y - radius;
        auto rw = radius * 2.0f;
        
        // Gradient Shadow
        juce::Path outlinePath;
        outlinePath.addEllipse(rx, ry, rw, rw);
        
        juce::DropShadow shadow(juce::Colours::black.withAlpha(0.8f), 8, {0, 4});
        shadow.drawForPath(g, outlinePath);
        
        // White Bold Outline
        g.setColour(juce::Colours::white);
        g.drawEllipse(rx, ry, rw, rw, 2.0f);
        
        // 2. Call default for the actual knob internals
        LookAndFeel_V4::drawRotarySlider(g, x, y, width, height, sliderPos, rotaryStartAngle, rotaryEndAngle, slider);
    }
};

struct Particle {
    float x, y;
    float speed;
    float size;
    float life; 
    float angle;
};

class DarkVoidVisual : public juce::Component
{
public:
    DarkVoidVisual() { for(int i=0; i<80; ++i) spawnParticle(); }
    
    void setActivity(float alpha, float pullAlpha) { 
        mixActivity = alpha; 
        pullActivity = pullAlpha;
        updateParticles();
        repaint(); 
    }
    
    void paint(juce::Graphics& g) override
    {
        auto center = getLocalBounds().getCentre().toFloat();
        float maxRadius = (float)juce::jmin(getWidth(), getHeight()) / 2.0f;

        // 1. Amplitude Reactive Core (Underneath) - 15% opacity
        float coreSize = maxRadius * (0.5f + (mixActivity * 0.4f)); 
        g.setColour(juce::Colours::grey.withAlpha(0.15f));
        g.fillEllipse(center.x - coreSize, center.y - coreSize, coreSize*2, coreSize*2);
        
        // 2. Outer Ring (Reactive)
        float ringSize = maxRadius * (0.8f + (mixActivity * 0.1f));
        g.setColour(juce::Colours::grey.withAlpha(0.1f));
        g.drawEllipse(center.x - ringSize, center.y - ringSize, ringSize*2, ringSize*2, 2.0f);

        // 3. The Void Gradient
        juce::ColourGradient grad(juce::Colours::black.withAlpha(0.95f), center,
                                  juce::Colours::transparentBlack, 
                                  juce::Point<float>(center.x, center.y - maxRadius), true);
        g.setGradientFill(grad);
        g.fillEllipse(getLocalBounds().toFloat());

        // 4. Particles (Drifting in)
        for(auto& p : particles) {
            g.setColour(juce::Colours::white.withAlpha(p.life * 0.7f));
            g.fillEllipse(p.x - p.size/2, p.y - p.size/2, p.size, p.size);
        }
        
        // 5. Black Particles exploding out? (Subtle inverted effect)
        if (mixActivity > 0.5f) {
             g.setColour(juce::Colours::black.withAlpha(0.3f));
             // Just draw a few random black specks near center
             juce::Random& r = juce::Random::getSystemRandom();
             for(int i=0; i<5; ++i) 
                 g.fillEllipse(center.x + r.nextFloat()*40-20, center.y + r.nextFloat()*40-20, 4, 4);
        }
    }

private:
    float mixActivity = 0.0f;
    float pullActivity = 0.0f;
    std::vector<Particle> particles;

    void spawnParticle() {
        Particle p;
        float dist = 350.0f; 
        float angle = juce::Random::getSystemRandom().nextFloat() * juce::MathConstants<float>::twoPi;
        p.x = 450.0f + std::cos(angle) * dist; 
        p.y = 450.0f + std::sin(angle) * dist;
        p.angle = angle + juce::MathConstants<float>::pi; 
        p.speed = 0.5f + juce::Random::getSystemRandom().nextFloat();
        p.size = 1.0f + juce::Random::getSystemRandom().nextFloat() * 2.5f;
        p.life = 0.0f; 
        particles.push_back(p);
    }

    void updateParticles() {
        auto center = getLocalBounds().getCentre().toFloat();
        for(int i=particles.size()-1; i>=0; --i) {
            float distToCenter = std::sqrt(std::pow(particles[i].x - center.x, 2) + std::pow(particles[i].y - center.y, 2));
            if (distToCenter < 10.0f) {
                particles.erase(particles.begin() + i);
                spawnParticle();
            }
        }
        for(auto& p : particles) {
            float speedMult = 1.0f + (pullActivity * 8.0f);
            p.x += std::cos(p.angle) * p.speed * speedMult;
            p.y += std::sin(p.angle) * p.speed * speedMult;
            if (p.life < 1.0f) p.life += 0.02f;
        }
        while(particles.size() < 100) spawnParticle();
    }
};

class SimpleMeter : public juce::Component
{
public:
    void setLevel(float level) { currentLevel = level; repaint(); }
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::black.withAlpha(0.5f));
        float h = (float)getHeight() * juce::jmin(currentLevel, 1.0f);
        g.setColour(juce::Colours::white.withAlpha(0.6f));
        g.fillRect(0.0f, (float)getHeight() - h, (float)getWidth(), h);
    }
private:
    float currentLevel = 0.0f;
};

class CompFilterAudioProcessorEditor  : public juce::AudioProcessorEditor, public juce::Timer
{
public:
    CompFilterAudioProcessorEditor (CompFilterAudioProcessor&);
    ~CompFilterAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    CompFilterAudioProcessor& audioProcessor;
    MellowLookAndFeel mellowLook;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    juce::Slider inGainS, outGainS;
    juce::Slider threshS, kneeS;
    juce::Slider lpfS, hpfS;
    juce::Slider pullS, pullAttS, pullRelS;
    // New Anti-Gravity Knobs
    juce::Slider antiGravLpfS, antiGravHpfS;

    std::vector<std::unique_ptr<SliderAttachment>> attachments;
    std::vector<juce::Label*> labels;

    DarkVoidVisual darkVoid;
    SimpleMeter inMeter, outMeter;
    
    void drawFrequencyOverlay(juce::Graphics& g, juce::Slider& slider, float normalizedTarget);
    void drawConnectingLines(juce::Graphics& g);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompFilterAudioProcessorEditor)
};
