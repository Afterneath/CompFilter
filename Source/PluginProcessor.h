#pragma once
#include <JuceHeader.h>

class CompFilterAudioProcessor  : public juce::AudioProcessor
{
public:
    CompFilterAudioProcessor();
    ~CompFilterAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Gravity"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int index) override {}
    const juce::String getProgramName (int index) override { return {}; }
    void changeProgramName (int index, const juce::String& newName) override {}
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    
    // Visualization Atomics
    std::atomic<float> currentInputRMS { 0.0f };
    std::atomic<float> currentOutputRMS { 0.0f };
    std::atomic<float> currentFilterMix { 0.0f }; 
    std::atomic<float> currentPullAmount { 0.0f }; 
    
    // Normalized positions for UI Ghost Pointers
    std::atomic<float> monitorLpfNorm { 1.0f };
    std::atomic<float> monitorHpfNorm { 0.0f };

    juce::AudioProcessorValueTreeState apvts;

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    using Filter = juce::dsp::StateVariableTPTFilter<float>;
    
    // 4-Stage Series Filters
    std::vector<std::array<std::unique_ptr<Filter>, 4>> lowPassFilters;
    std::vector<std::array<std::unique_ptr<Filter>, 4>> highPassFilters;

    float compEnvelope = 0.0f;
    const float compAttackTime = 10.0f; 
    const float compReleaseTime = 100.0f;
    float compAttackCoeff = 0.0f;
    float compReleaseCoeff = 0.0f;

    float pullEnvelope = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompFilterAudioProcessor)
};
