#include <juce_audio_utils/juce_audio_utils.h>

#include <algorithm>
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

std::unique_ptr<juce::AudioPluginInstance> loadInstance (juce::AudioPluginFormatManager& manager,
                                                         const juce::PluginDescription& description,
                                                         juce::String& error)
{
    return manager.createPluginInstance (description, 44100.0, 256, error);
}
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc != 2)
    {
        std::cerr << "Expected VST3 bundle path\n";
        return 2;
    }

    const juce::String bundlePath = juce::String::fromUTF8 (argv[1]);
    juce::VST3PluginFormat format;
    juce::OwnedArray<juce::PluginDescription> descriptions;
    format.findAllTypesForFile (descriptions, bundlePath);
    expect (descriptions.size() == 1, "VST3 bundle exposes exactly one audio class");
    if (descriptions.isEmpty())
        return 1;

    const auto& description = *descriptions[0];
    expect (description.name == "Long Land", "VST3 class name is Long Land");
    expect (description.manufacturerName == "Long Land Audio", "VST3 vendor identity is stable");
    expect (description.pluginFormatName == "VST3", "bundle is recognized as VST3");
    expect (description.isInstrument == false, "VST3 remains an audio effect");

    juce::AudioPluginFormatManager manager;
    manager.addFormat (std::make_unique<juce::VST3PluginFormat>());
    juce::String firstError;
    auto first = loadInstance (manager, description, firstError);
    expect (first != nullptr, "VST3 wrapper instantiates at 44.1 kHz / 256 samples: " + firstError.toStdString());
    if (first == nullptr)
        return 1;

    const std::set<std::string> expectedNames
    {
        "Mix", "Pre-delay", "Decay", "Room Size", "Diffusion", "Drift", "Drift Rate",
        "Low Cut", "High Cut", "Width", "Age", "Early Field", "Output", "Infinite"
    };
    std::set<std::string> actualIDs;
    std::set<std::string> actualNames;
    juce::HostedAudioProcessorParameter* mix = nullptr;
    for (auto* parameter : first->getParameters())
        if (auto* identified = dynamic_cast<juce::HostedAudioProcessorParameter*> (parameter))
        {
            actualIDs.insert (identified->getParameterID().toStdString());
            actualNames.insert (identified->getName (64).toStdString());
            if (identified->getName (64) == "Mix")
                mix = identified;
        }
    expect (first->getParameters().size() == 16 && actualIDs.size() == 16,
            "VST3 wrapper publishes 16 unique hosted IDs (14 controls plus bypass/program)");
    expect (std::includes (actualNames.begin(), actualNames.end(), expectedNames.begin(), expectedNames.end()),
            "VST3 wrapper exposes all 14 expected Long Land control names");

    if (mix != nullptr)
        mix->setValueNotifyingHost (0.675f);
    juce::MemoryBlock state;
    first->getStateInformation (state);
    expect (! state.isEmpty(), "VST3 wrapper returns non-empty host state");

    juce::String secondError;
    auto second = loadInstance (manager, description, secondError);
    expect (second != nullptr, "second VST3 instance loads for state restore: " + secondError.toStdString());
    if (second != nullptr)
    {
        second->setStateInformation (state.getData(), static_cast<int> (state.getSize()));
        juce::HostedAudioProcessorParameter* restoredMix = nullptr;
        for (auto* parameter : second->getParameters())
            if (auto* identified = dynamic_cast<juce::HostedAudioProcessorParameter*> (parameter);
                identified != nullptr && identified->getName (64) == "Mix")
                restoredMix = identified;
        expect (restoredMix != nullptr && std::abs (restoredMix->getValue() - 0.675f) < 0.0001f,
                "VST3 host state restores the automated Mix value");
    }

    expect (first->hasEditor(), "VST3 wrapper advertises the native editor");
    std::unique_ptr<juce::AudioProcessorEditor> editor (first->createEditorIfNeeded());
    expect (editor != nullptr && editor->getWidth() * 10 == editor->getHeight() * 16,
            "VST3 creates a 16:10 native editor");

    std::cout << "METRIC vst3Classes=" << descriptions.size() << " parameters=" << actualIDs.size()
              << " stateBytes=" << state.getSize() << "\n";
    return failures == 0 ? 0 : 1;
}
