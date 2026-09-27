#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class LongLandAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                           private juce::Timer
{
public:
    explicit LongLandAudioProcessorEditor (LongLandAudioProcessor&);
    ~LongLandAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class LandLookAndFeel;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    static constexpr std::size_t primaryControlCount = 4;
    static constexpr std::size_t controlCount = 13;

    void timerCallback() override;
    void setDetailView (bool shouldShow);
    void loadFactoryProgram (int programIndex);
    void syncProgramBox();
    void refreshReadoutText();
    void configureControl (std::size_t index, const char* parameterID,
                           const char* displayName, const char* helpText);
    void drawLiveReadouts (juce::Graphics&);

    LongLandAudioProcessor& processor;
    juce::Image primaryPlate;
    juce::Image detailPlate;
    juce::Image knobAtlas;
    std::unique_ptr<LandLookAndFeel> look;

    std::array<juce::Slider, controlCount> controls;
    std::array<juce::Label, controlCount> controlLabels;
    std::vector<std::unique_ptr<SliderAttachment>> controlAttachments;

    juce::ComboBox programBox;
    juce::TextButton detailButton { "DETAIL" };
    juce::ToggleButton freezeButton { "INFINITE" };
    std::unique_ptr<ButtonAttachment> freezeAttachment;
    std::array<juce::String, controlCount> liveValueTexts;
    juce::String screenTelemetry;
    juce::String engineTelemetry { "ENGINE IDLE   /   64-bit VST3" };

    bool showingDetails = false;
    bool loadingProgram = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LongLandAudioProcessorEditor)
};
