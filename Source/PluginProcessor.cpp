#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace IDs
{
constexpr auto mix = "mix";
constexpr auto predelay = "predelay";
constexpr auto decay = "decay";
constexpr auto size = "size";
constexpr auto diffusion = "diffusion";
constexpr auto modDepth = "modDepth";
constexpr auto modRate = "modRate";
constexpr auto lowCut = "lowCut";
constexpr auto highCut = "highCut";
constexpr auto width = "width";
constexpr auto age = "age";
constexpr auto earlyLate = "earlyLate";
constexpr auto output = "output";
constexpr auto freeze = "freeze";
}

namespace
{
struct Preset
{
    float mix, pre, decay, size, diffusion, depth, rate, low, high, width, age, early, output;
    bool freeze;
};

constexpr std::array<const char*, 14> parameterIDs
{
    IDs::mix, IDs::predelay, IDs::decay, IDs::size, IDs::diffusion, IDs::modDepth, IDs::modRate,
    IDs::lowCut, IDs::highCut, IDs::width, IDs::age, IDs::earlyLate, IDs::output, IDs::freeze
};

constexpr std::array<Preset, 6> presets
{{
    { 35, 24, 7.5f, 72, 82, 1.8f, .23f, 90, 7600, 115, 22, 27, 0, false },
    { 31, 10, 2.8f, 40, 91, 1.1f, .31f, 75, 9200, 100.0f, 14, 34, 0, false },
    { 44, 58, 14.0f, 88, 76, 3.7f, .11f, 160, 5100, 138, 33, 16, 0, false },
    { 52, 92, 21.0f, 94, 68, 5.2f, .071f, 220, 3600, 155, 47, 12, 0, false },
    { 39, 34, 8.4f, 69, 88, 2.4f, .42f, 130, 4300, 124, 76, 31, 0, false },
    { 100, 40, 35.0f, 100, 92, 4.8f, .08f, 180, 6200, 145, 29, 8, 0, true }
}};

std::array<float, 14> valuesForPreset (const Preset& p) noexcept
{
    return { p.mix, p.pre, p.decay, p.size, p.diffusion, p.depth, p.rate, p.low, p.high,
             p.width, p.age, p.early, p.output, p.freeze ? 1.0f : 0.0f };
}
}

LongLandAudioProcessor::LongLandAudioProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "LongLandState", createParameterLayout())
{
    for (const auto* id : parameterIDs)
        state.addParameterListener (id, this);
}

LongLandAudioProcessor::~LongLandAudioProcessor()
{
    for (const auto* id : parameterIDs)
        state.removeParameterListener (id, this);
}

juce::AudioProcessorValueTreeState::ParameterLayout LongLandAudioProcessor::createParameterLayout()
{
    using APF = juce::AudioParameterFloat;
    using APB = juce::AudioParameterBool;
    using Range = juce::NormalisableRange<float>;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<APF> (juce::ParameterID { IDs::mix, 1 }, "Mix", Range (0.0f, 100.0f, 0.01f), 35.0f, "%"));
    layout.add (std::make_unique<APF> (juce::ParameterID { IDs::predelay, 1 }, "Pre-delay", Range (0.0f, 2000.0f, 0.01f, 0.35f), 24.0f, " ms"));
    layout.add (std::make_unique<APF> (juce::ParameterID { IDs::decay, 1 }, "Decay", Range (0.15f, 60.0f, 0.001f, 0.32f), 7.5f, " s"));
    layout.add (std::make_unique<APF> (juce::ParameterID { IDs::size, 1 }, "Room Size", Range (0.0f, 100.0f, 0.01f), 72.0f, "%"));
    layout.add (std::make_unique<APF> (juce::ParameterID { IDs::diffusion, 1 }, "Diffusion", Range (0.0f, 100.0f, 0.01f), 82.0f, "%"));
    layout.add (std::make_unique<APF> (juce::ParameterID { IDs::modDepth, 1 }, "Drift", Range (0.0f, 12.0f, 0.001f, 0.55f), 1.8f, " ms"));
    layout.add (std::make_unique<APF> (juce::ParameterID { IDs::modRate, 1 }, "Drift Rate", Range (0.01f, 5.0f, 0.001f, 0.42f), 0.23f, " Hz"));
    layout.add (std::make_unique<APF> (juce::ParameterID { IDs::lowCut, 1 }, "Low Cut", Range (20.0f, 2000.0f, 0.1f, 0.30f), 90.0f, " Hz"));
    layout.add (std::make_unique<APF> (juce::ParameterID { IDs::highCut, 1 }, "High Cut", Range (800.0f, 20000.0f, 0.1f, 0.38f), 7600.0f, " Hz"));
    layout.add (std::make_unique<APF> (juce::ParameterID { IDs::width, 1 }, "Width", Range (0.0f, 200.0f, 0.01f), 115.0f, "%"));
    layout.add (std::make_unique<APF> (juce::ParameterID { IDs::age, 1 }, "Age", Range (0.0f, 100.0f, 0.01f), 22.0f, "%"));
    layout.add (std::make_unique<APF> (juce::ParameterID { IDs::earlyLate, 1 }, "Early Field", Range (0.0f, 100.0f, 0.01f), 27.0f, "%"));
    layout.add (std::make_unique<APF> (juce::ParameterID { IDs::output, 1 }, "Output", Range (-24.0f, 12.0f, 0.01f), 0.0f, " dB"));
    layout.add (std::make_unique<APB> (juce::ParameterID { IDs::freeze, 1 }, "Infinite", false));
    return layout;
}

void LongLandAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    monoScratch.resize (static_cast<std::size_t> (std::max (1, samplesPerBlock)));
    updateEngineParameters();
}

bool LongLandAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();
    return (input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo())
        && output == input;
}

void LongLandAudioProcessor::updateEngineParameters() noexcept
{
    const auto value = [this] (const char* id) { return state.getRawParameterValue (id)->load(); };
    longland::Parameters p;
    p.mix = value (IDs::mix) * 0.01f;
    p.preDelayMs = value (IDs::predelay);
    p.decaySeconds = value (IDs::decay);
    p.size = value (IDs::size) * 0.01f;
    p.diffusion = value (IDs::diffusion) * 0.01f;
    p.modDepthMs = value (IDs::modDepth);
    p.modRateHz = value (IDs::modRate);
    p.lowCutHz = value (IDs::lowCut);
    p.highCutHz = value (IDs::highCut);
    p.width = value (IDs::width) * 0.01f;
    p.age = value (IDs::age) * 0.01f;
    p.earlyLate = value (IDs::earlyLate) * 0.01f;
    p.outputDb = value (IDs::output);
    p.freeze = value (IDs::freeze) > 0.5f;
    engine.setParameters (p);
}

void LongLandAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    updateEngineParameters();

    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();
    if (numSamples <= 0 || numChannels <= 0)
        return;

    if (numChannels == 1)
    {
        // Do not allocate on the audio thread if a host exceeds the maximum block
        // size declared in prepareToPlay. Process the mono input in bounded chunks.
        auto* mono = buffer.getWritePointer (0);
        const auto scratchCapacity = static_cast<int> (monoScratch.size());
        if (scratchCapacity <= 0)
            return;

        for (int offset = 0; offset < numSamples; offset += scratchCapacity)
        {
            const auto chunkSize = juce::jmin (scratchCapacity, numSamples - offset);
            std::copy_n (mono + offset, chunkSize, monoScratch.begin());
            engine.process (mono + offset, monoScratch.data(), chunkSize);
        }
        return;
    }

    engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), numSamples);

    // The supported layouts are mono and stereo, but clear any unexpected extra
    // host channels rather than leaving stale data in them.
    for (int channel = 2; channel < numChannels; ++channel)
        buffer.clear (channel, 0, numSamples);
}

juce::AudioProcessorEditor* LongLandAudioProcessor::createEditor()
{
    return new LongLandAudioProcessorEditor (*this);
}

double LongLandAudioProcessor::getTailLengthSeconds() const
{
    if (state.getRawParameterValue (IDs::freeze)->load() > 0.5f) return std::numeric_limits<double>::infinity();
    return static_cast<double> (state.getRawParameterValue (IDs::decay)->load()) * 1.8 + 2.0;
}

const juce::String LongLandAudioProcessor::getProgramName (int index)
{
    static const char* names[] { "Long Land", "Hall 1986", "Fog Machine", "Sunken Mall", "VHS Chapel", "Forever" };
    return juce::isPositiveAndBelow (index, getNumPrograms()) ? names[index] : juce::String();
}

void LongLandAudioProcessor::setParameter (const char* id, float plainValue)
{
    if (auto* p = state.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (plainValue));
}

void LongLandAudioProcessor::parameterChanged (const juce::String&, float)
{
    if (! loadingProgram.load())
        recomputeProgramEdited();
}

void LongLandAudioProcessor::recomputeProgramEdited() noexcept
{
    const auto index = juce::jlimit (0, getNumPrograms() - 1, currentProgram.load());
    const auto expected = valuesForPreset (presets[static_cast<std::size_t> (index)]);
    bool differs = false;
    for (std::size_t i = 0; i < parameterIDs.size(); ++i)
    {
        if (const auto* value = state.getRawParameterValue (parameterIDs[i]))
            differs = differs || std::abs (value->load() - expected[i]) > 0.0005f;
    }
    programEdited.store (differs);
}

void LongLandAudioProcessor::setCurrentProgram (int index)
{
    index = juce::jlimit (0, getNumPrograms() - 1, index);
    loadingProgram.store (true);
    currentProgram.store (index);
    const auto& p = presets[static_cast<std::size_t> (index)];
    setParameter (IDs::mix, p.mix); setParameter (IDs::predelay, p.pre); setParameter (IDs::decay, p.decay);
    setParameter (IDs::size, p.size); setParameter (IDs::diffusion, p.diffusion); setParameter (IDs::modDepth, p.depth);
    setParameter (IDs::modRate, p.rate); setParameter (IDs::lowCut, p.low); setParameter (IDs::highCut, p.high);
    setParameter (IDs::width, p.width); setParameter (IDs::age, p.age); setParameter (IDs::earlyLate, p.early);
    setParameter (IDs::output, p.output);
    setParameter (IDs::freeze, p.freeze ? 1.0f : 0.0f);
    programEdited.store (false);
    loadingProgram.store (false);
}

void LongLandAudioProcessor::getStateInformation (juce::MemoryBlock& destination)
{
    auto root = state.copyState();
    root.setProperty ("program", currentProgram.load(), nullptr);
    root.setProperty ("custom", programEdited.load(), nullptr);
    if (auto xml = root.createXml()) copyXmlToBinary (*xml, destination);
}

void LongLandAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (state.state.getType()))
        {
            auto restored = juce::ValueTree::fromXml (*xml);
            loadingProgram.store (true);
            currentProgram.store (juce::jlimit (0, getNumPrograms() - 1,
                                                static_cast<int> (restored.getProperty ("program", 0))));
            state.replaceState (restored);
            loadingProgram.store (false);
            recomputeProgramEdited();
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new LongLandAudioProcessor(); }
