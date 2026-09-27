#include "PluginProcessor.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <set>
#include <string>

namespace
{
int failures = 0;

void expect (bool condition, const std::string& message)
{
    std::cout << (condition ? "PASS " : "FAIL ") << message << '\n';
    failures += condition ? 0 : 1;
}

template <typename ComponentType, typename Predicate>
ComponentType* findComponent (juce::Component& root, Predicate&& predicate)
{
    for (auto* child : root.getChildren())
    {
        if (auto* typed = dynamic_cast<ComponentType*> (child); typed != nullptr && predicate (*typed))
            return typed;
        if (auto* nested = findComponent<ComponentType> (*child, predicate))
            return nested;
    }
    return nullptr;
}

bool isFinite (const juce::AudioBuffer<float>& buffer)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            if (! std::isfinite (buffer.getSample (channel, sample)))
                return false;
    return true;
}

bool hasVisualContent (const juce::Image& image)
{
    if (! image.isValid())
        return false;
    int darkest = 255;
    int brightest = 0;
    for (int y = 0; y < image.getHeight(); y += juce::jmax (1, image.getHeight() / 30))
        for (int x = 0; x < image.getWidth(); x += juce::jmax (1, image.getWidth() / 40))
        {
            const auto pixel = image.getPixelAt (x, y);
            const auto luminance = juce::roundToInt (pixel.getPerceivedBrightness() * 255.0f);
            darkest = juce::jmin (darkest, luminance);
            brightest = juce::jmax (brightest, luminance);
        }
    return brightest - darkest > 80;
}

bool savePng (const juce::Image& image, const juce::File& file)
{
    file.deleteFile();
    if (auto output = file.createOutputStream())
        return juce::PNGImageFormat().writeImageToStream (image, *output);
    return false;
}
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc != 2)
    {
        std::cerr << "Expected snapshot directory\n";
        return 2;
    }
    const juce::File snapshotDirectory (juce::String::fromUTF8 (argv[1]));

    LongLandAudioProcessor processor;
    expect (processor.getName() == "Long Land", "processor identity remains Long Land");
    expect (processor.getNumPrograms() == 6, "six factory programs remain available");

    const std::set<std::string> expectedIDs
    {
        "mix", "predelay", "decay", "size", "diffusion", "modDepth", "modRate",
        "lowCut", "highCut", "width", "age", "earlyLate", "output", "freeze"
    };
    std::set<std::string> actualIDs;
    for (auto* parameter : processor.getParameters())
        if (auto* identified = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter))
            actualIDs.insert (identified->paramID.toStdString());
    expect (actualIDs == expectedIDs, "all 14 existing APVTS parameter IDs are unchanged");

    juce::AudioProcessor::BusesLayout stereoLayout;
    stereoLayout.inputBuses.add (juce::AudioChannelSet::stereo());
    stereoLayout.outputBuses.add (juce::AudioChannelSet::stereo());
    expect (processor.isBusesLayoutSupported (stereoLayout), "stereo VST3 bus layout remains supported");

    processor.setRateAndBufferSizeDetails (44100.0, 256);
    processor.prepareToPlay (44100.0, 256);
    juce::AudioBuffer<float> audio (2, 256);
    juce::MidiBuffer midi;
    for (int sample = 0; sample < audio.getNumSamples(); ++sample)
    {
        audio.setSample (0, sample, 0.08f * std::sin (0.021f * static_cast<float> (sample)));
        audio.setSample (1, sample, 0.07f * std::cos (0.017f * static_cast<float> (sample)));
    }
    processor.processBlock (audio, midi);
    expect (isFinite (audio), "44.1 kHz / 256-sample processing stays finite");

    processor.setCurrentProgram (2);
    if (auto* mix = processor.state.getParameter ("mix"))
        mix->setValueNotifyingHost (mix->convertTo0to1 (61.25f));
    juce::MemoryBlock savedState;
    processor.getStateInformation (savedState);
    LongLandAudioProcessor restored;
    restored.setStateInformation (savedState.getData(), static_cast<int> (savedState.getSize()));
    expect (restored.getCurrentProgram() == 2, "program index survives state round-trip");
    expect (std::abs (restored.state.getRawParameterValue ("mix")->load() - 61.25f) < 0.001f,
            "custom APVTS value survives state round-trip");

    bool editorStressPassed = true;
    bool visualEvidencePassed = false;
    bool valueUnitFormattingPassed = false;
    for (int creation = 0; creation < 8; ++creation)
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
        editorStressPassed = editorStressPassed && editor != nullptr;
        if (editor == nullptr)
            break;

        auto* detail = findComponent<juce::TextButton> (*editor, [] (const juce::TextButton& button)
        {
            return button.getTitle() == "Toggle detailed parameters";
        });
        auto* infinite = findComponent<juce::ToggleButton> (*editor, [] (const juce::ToggleButton& button)
        {
            return button.getTitle() == "Infinite decay";
        });
        editorStressPassed = editorStressPassed && detail != nullptr && infinite != nullptr;
        if (detail == nullptr || infinite == nullptr)
            break;

        if (creation == 0)
        {
            static constexpr std::array<std::pair<const char*, const char*>, 13> expectedUnits
            {{
                { "MIX", "%" }, { "TIME", "s" }, { "LAND / SIZE", "%" },
                { "WEATHER / AGE", "%" }, { "PRE-DELAY", "ms" }, { "DIFFUSION", "%" },
                { "EARLY FIELD", "%" }, { "DRIFT", "ms" }, { "TIDE", "Hz" },
                { "LOW SKY", "Hz" }, { "HIGH SKY", "Hz" }, { "EXPANSE", "%" },
                { "OUTPUT", "dB" }
            }};
            valueUnitFormattingPassed = true;
            for (const auto& [title, unit] : expectedUnits)
            {
                auto* slider = findComponent<juce::Slider> (*editor, [title] (const juce::Slider& candidate)
                {
                    return candidate.getTitle() == title;
                });
                const auto formatted = slider != nullptr
                    ? slider->getTextFromValue (slider->getValue()) : juce::String ("<missing>");
                const auto unitPresent = formatted.containsIgnoreCase (unit);
                if (! unitPresent)
                    std::cout << "FORMAT " << title << "='" << formatted << "' expected " << unit << '\n';
                valueUnitFormattingPassed = valueUnitFormattingPassed && slider != nullptr && unitPresent;
            }

            editor->setSize (960, 600);
            detail->setToggleState (false, juce::dontSendNotification);
            if (detail->onClick)
                detail->onClick();
            const auto primarySnapshot = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
            detail->setToggleState (true, juce::dontSendNotification);
            if (detail->onClick)
                detail->onClick();
            const auto detailSnapshot = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
            const auto directoryReady = snapshotDirectory.createDirectory().wasOk() || snapshotDirectory.isDirectory();
            visualEvidencePassed = directoryReady
                                && hasVisualContent (primarySnapshot)
                                && hasVisualContent (detailSnapshot)
                                && savePng (primarySnapshot,
                                            snapshotDirectory.getChildFile ("longland-editor-primary-960x600.png"))
                                && savePng (detailSnapshot,
                                            snapshotDirectory.getChildFile ("longland-editor-detail-960x600.png"));
        }

        for (int pass = 0; pass < 12; ++pass)
        {
            const auto showDetail = (pass % 2) != 0;
            detail->setToggleState (showDetail, juce::dontSendNotification);
            if (detail->onClick)
                detail->onClick();

            const auto width = 720 + (pass % 5) * 144;
            editor->setSize (width, width * 10 / 16);
            const auto snapshot = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
            editorStressPassed = editorStressPassed && snapshot.isValid()
                               && snapshot.getWidth() == editor->getWidth()
                               && snapshot.getHeight() == editor->getHeight();

            infinite->setToggleState ((pass % 3) == 0, juce::sendNotification);
            audio.clear();
            audio.setSample (0, 0, 0.2f);
            audio.setSample (1, 0, -0.2f);
            processor.processBlock (audio, midi);
            editorStressPassed = editorStressPassed && isFinite (audio);
        }
    }
    expect (editorStressPassed,
            "editor survives repeated creation, Primary/Detail switching, 16:10 resizes, painting and live audio");
    expect (visualEvidencePassed,
            "compiled editor Primary and expanded Detail snapshots contain visible non-black content");
    expect (valueUnitFormattingPassed,
            "all 13 live value formatters preserve their %, s, ms, Hz or dB unit suffix");

    std::cout << "METRIC editorCreations=8 viewSwitches=96 paintSnapshots=96 sampleRate=44100 blockSize=256\n";
    return failures == 0 ? 0 : 1;
}
