#include "PluginProcessor.h"
#include "PluginEditor.h"

CompFilterAudioProcessor::CompFilterAudioProcessor()
     : AudioProcessor (BusesProperties()
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
       apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    for (int ch = 0; ch < 2; ++ch) {
        std::array<std::unique_ptr<Filter>, 4> lpfStack;
        std::array<std::unique_ptr<Filter>, 4> hpfStack;
        for (int i = 0; i < 4; ++i) {
            lpfStack[i] = std::make_unique<Filter>();
            lpfStack[i]->setType(juce::dsp::StateVariableTPTFilterType::lowpass);
            hpfStack[i] = std::make_unique<Filter>();
            hpfStack[i]->setType(juce::dsp::StateVariableTPTFilterType::highpass);
        }
        lowPassFilters.push_back(std::move(lpfStack));
        highPassFilters.push_back(std::move(hpfStack));
    }
}

CompFilterAudioProcessor::~CompFilterAudioProcessor() {}

juce::AudioProcessorValueTreeState::ParameterLayout CompFilterAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    
    layout.add(std::make_unique<juce::AudioParameterFloat>("inGain", "Input Gain", -24.0f, 24.0f, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("outGain", "Output Gain", -24.0f, 24.0f, 0.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>("lpfCutoff", "LPF Base", 
        juce::NormalisableRange<float>(2000.0f, 20000.0f, 1.0f, 0.3f), 20000.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("hpfCutoff", "HPF Base", 
        juce::NormalisableRange<float>(20.0f, 2000.0f, 1.0f, 0.3f), 20.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>("threshold", "Threshold", -60.0f, 0.0f, -20.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("knee", "Knee Width", 0.0f, 30.0f, 5.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>("pull", "Gravity", 0.0f, 100.0f, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("pullAttack", "Pull Attack", 1.0f, 500.0f, 50.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("pullRelease", "Pull Release", 1.0f, 500.0f, 200.0f));

    // NEW: Anti-Gravity Controls (0 to 100%)
    layout.add(std::make_unique<juce::AudioParameterFloat>("antiGravLpf", "Low Resist", 0.0f, 100.0f, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("antiGravHpf", "High Resist", 0.0f, 100.0f, 0.0f));

    return layout;
}

void CompFilterAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = 1; 

    for (int ch = 0; ch < 2; ++ch) {
        for (int i = 0; i < 4; ++i) {
            lowPassFilters[ch][i]->prepare(spec);
            lowPassFilters[ch][i]->reset();
            highPassFilters[ch][i]->prepare(spec);
            highPassFilters[ch][i]->reset();
        }
    }
    compAttackCoeff = std::exp(-1.0f / (sampleRate * (compAttackTime / 1000.0f)));
    compReleaseCoeff = std::exp(-1.0f / (sampleRate * (compReleaseTime / 1000.0f)));
    compEnvelope = 0.0f;
    pullEnvelope = 0.0f;
}

void CompFilterAudioProcessor::releaseResources() {}

void CompFilterAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    if (buffer.getNumSamples() == 0) return;

    auto lpfRange = apvts.getParameter("lpfCutoff")->getNormalisableRange();
    auto hpfRange = apvts.getParameter("hpfCutoff")->getNormalisableRange();

    float inGainDb = *apvts.getRawParameterValue("inGain");
    float outGainDb = *apvts.getRawParameterValue("outGain");
    float threshDb = *apvts.getRawParameterValue("threshold");
    float kneeDb = *apvts.getRawParameterValue("knee");
    
    // Base Values
    float baseLpfNorm = lpfRange.convertTo0to1(*apvts.getRawParameterValue("lpfCutoff"));
    float baseHpfNorm = hpfRange.convertTo0to1(*apvts.getRawParameterValue("hpfCutoff"));
    
    // NEW TARGET: 2000Hz
    float targetLpfNorm = lpfRange.convertTo0to1(2000.0f);
    float targetHpfNorm = hpfRange.convertTo0to1(2000.0f);

    float pullAmount = *apvts.getRawParameterValue("pull") / 100.0f; 
    
    // Anti-Gravity Factors (0.0 = Full Pull, 1.0 = No Pull)
    float resistLpf = *apvts.getRawParameterValue("antiGravLpf") / 100.0f;
    float resistHpf = *apvts.getRawParameterValue("antiGravHpf") / 100.0f;

    float pullAttMs = *apvts.getRawParameterValue("pullAttack");
    float pullRelMs = *apvts.getRawParameterValue("pullRelease");
    float pAttCoeff = std::exp(-1.0f / (getSampleRate() * (pullAttMs / 1000.0f)));
    float pRelCoeff = std::exp(-1.0f / (getSampleRate() * (pullRelMs / 1000.0f)));

    float kneeStartDb = threshDb - (kneeDb / 2.0f);
    float kneeEndDb = threshDb + (kneeDb / 2.0f);

    float inGainLinear = juce::Decibels::decibelsToGain(inGainDb);
    float outGainLinear = juce::Decibels::decibelsToGain(outGainDb);

    float sumInputSq = 0.0f;
    float sumOutputSq = 0.0f;
    float maxMixRatio = 0.0f;
    float maxPullMod = 0.0f;
    
    float currentLpfNorm = baseLpfNorm;
    float currentHpfNorm = baseHpfNorm;

    int numCh = std::min(buffer.getNumChannels(), 2);

    for (int ch = 0; ch < numCh; ++ch)
    {
        auto* channelData = buffer.getWritePointer(ch);

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            float inSample = channelData[sample] * inGainLinear;
            sumInputSq += inSample * inSample;
            float absSample = std::abs(inSample);

            // Envelopes
            if (absSample > compEnvelope) compEnvelope = compAttackCoeff * compEnvelope + (1.0f - compAttackCoeff) * absSample;
            else compEnvelope = compReleaseCoeff * compEnvelope + (1.0f - compReleaseCoeff) * absSample;

            if (absSample > pullEnvelope) pullEnvelope = pAttCoeff * pullEnvelope + (1.0f - pAttCoeff) * absSample;
            else pullEnvelope = pRelCoeff * pullEnvelope + (1.0f - pRelCoeff) * absSample;

            // Global Pull (0.0 to 1.0)
            float rawPull = juce::jmin(pullEnvelope * 4.0f, 1.0f) * pullAmount;
            if (rawPull > maxPullMod) maxPullMod = rawPull;

            // Apply Anti-Gravity Scales
            float lpfPull = rawPull * (1.0f - resistLpf);
            float hpfPull = rawPull * (1.0f - resistHpf);

            // Calculate active norms
            float activeLpfNorm = baseLpfNorm - ((baseLpfNorm - targetLpfNorm) * lpfPull);
            float activeHpfNorm = baseHpfNorm + ((targetHpfNorm - baseHpfNorm) * hpfPull);
            
            float activeLpfHz = lpfRange.convertFrom0to1(activeLpfNorm);
            float activeHpfHz = hpfRange.convertFrom0to1(activeHpfNorm);

            // Hard Clamp to 2000Hz (Center)
            if (activeLpfHz < 2000.0f) activeLpfHz = 2000.0f;
            if (activeHpfHz > 2000.0f) activeHpfHz = 2000.0f;

            if (ch == 0 && sample == 0) {
                currentLpfNorm = activeLpfNorm;
                currentHpfNorm = activeHpfNorm;
            }

            // Serial Processing
            float processed = inSample;
            for (int s = 0; s < 4; ++s) {
                lowPassFilters[ch][s]->setCutoffFrequency(activeLpfHz);
                processed = lowPassFilters[ch][s]->processSample(0, processed);
            }
            for (int s = 0; s < 4; ++s) {
                highPassFilters[ch][s]->setCutoffFrequency(activeHpfHz);
                processed = highPassFilters[ch][s]->processSample(0, processed);
            }
            
            float filteredSample = processed;

            // Mix
            float envDb = juce::Decibels::gainToDecibels(compEnvelope);
            float mixRatio = 0.0f;
            if (kneeDb > 0.0f) {
                if (envDb > kneeEndDb) mixRatio = juce::jmin((envDb - threshDb) * 0.2f, 1.0f);
                else if (envDb > kneeStartDb) mixRatio = ((envDb - kneeStartDb) / kneeDb) * 0.5f;
            } else {
                if (envDb > threshDb) mixRatio = juce::jmin((envDb - threshDb) * 0.2f, 1.0f);
            }
            if (mixRatio > maxMixRatio) maxMixRatio = mixRatio;

            channelData[sample] = ((inSample * (1.0f - mixRatio)) + (filteredSample * mixRatio)) * outGainLinear;
            sumOutputSq += channelData[sample] * channelData[sample];
        }
    }

    currentInputRMS = std::sqrt(sumInputSq / (buffer.getNumSamples() * numCh));
    currentOutputRMS = std::sqrt(sumOutputSq / (buffer.getNumSamples() * numCh));
    currentFilterMix = currentFilterMix * 0.9f + maxMixRatio * 0.1f;
    currentPullAmount = currentPullAmount * 0.9f + maxPullMod * 0.1f;
    
    monitorLpfNorm = currentLpfNorm;
    monitorHpfNorm = currentHpfNorm;
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new CompFilterAudioProcessor(); }
// --- ADD THIS TO THE VERY BOTTOM OF PluginProcessor.cpp ---

juce::AudioProcessorEditor* CompFilterAudioProcessor::createEditor()
{
    return new CompFilterAudioProcessorEditor (*this);
}

void CompFilterAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Saves your parameters (Gravity, Anti-Grav, etc.) when you save a project
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void CompFilterAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // Restores your parameters when you reload the plugin
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}
