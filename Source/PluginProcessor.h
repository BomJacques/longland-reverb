#pragma once

#include <JuceHeader.h>
#include "LongLandDSP.h"

class LongLandAudioProcessor final : public juce::AudioProcessor,
                                     private juce::AudioProcessorValueTreeState::Listener
{
public:
    LongLandAudioProcessor();
    ~LongLandAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;

    int getNumPrograms() override { return 6; }
    int getCurrentProgram() override { return currentProgram.load(); }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}
    [[nodiscard]] bool isCurrentProgramEdited() const noexcept { return programEdited.load(); }

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState state;

private:
    void updateEngineParameters() noexcept;
    void parameterChanged (const juce::String&, float) override;
    void recomputeProgramEdited() noexcept;
    void setParameter (const char* id, float plainValue);

    longland::ReverbEngine engine;
    std::atomic<int> currentProgram { 0 };
    std::atomic<bool> programEdited { false };
    std::atomic<bool> loadingProgram { false };
    std::vector<float> monoScratch;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LongLandAudioProcessor)
};
