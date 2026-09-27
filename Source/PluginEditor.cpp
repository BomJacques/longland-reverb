#include "PluginEditor.h"

#include <BinaryData.h>

#include <array>
#include <cmath>

namespace
{
constexpr float sourceWidth = 1280.0f;
constexpr float sourceHeight = 800.0f;
constexpr int atlasCellSize = 224;
constexpr int atlasColumns = 8;
constexpr int atlasFrameCount = 63;

const auto cream = juce::Colour (0xffead9ae);
const auto amber = juce::Colour (0xffffa54e);
const auto ink = juce::Colour (0xff101613);

juce::Font smallCaps (float size)
{
    return juce::Font (juce::FontOptions (size).withStyle ("Bold"));
}

juce::Image loadEmbeddedImage (const void* bytes, int byteCount)
{
    return juce::ImageFileFormat::loadFrom (bytes, static_cast<std::size_t> (byteCount));
}

juce::Rectangle<int> scaleSourceRect (juce::Rectangle<float> source, int width, int height)
{
    const auto xScale = static_cast<float> (width) / sourceWidth;
    const auto yScale = static_cast<float> (height) / sourceHeight;
    return { juce::roundToInt (source.getX() * xScale),
             juce::roundToInt (source.getY() * yScale),
             juce::roundToInt (source.getWidth() * xScale),
             juce::roundToInt (source.getHeight() * yScale) };
}

constexpr std::array<juce::Point<float>, 4> primaryKnobCentres
{{
    { 212.0f, 566.0f }, { 497.0f, 566.0f }, { 783.0f, 566.0f }, { 1068.0f, 566.0f }
}};

constexpr std::array<juce::Point<float>, 13> detailKnobCentres
{{
    { 157.0f, 480.0f }, { 318.0f, 480.0f }, { 479.0f, 480.0f }, { 640.0f, 480.0f },
    { 801.0f, 480.0f }, { 962.0f, 480.0f }, { 1123.0f, 480.0f },
    { 238.0f, 627.0f }, { 399.0f, 627.0f }, { 560.0f, 627.0f },
    { 720.0f, 627.0f }, { 881.0f, 627.0f }, { 1042.0f, 627.0f }
}};

constexpr std::array<juce::Point<float>, 4> primaryValueCentres
{{
    { 212.0f, 698.0f }, { 497.0f, 698.0f }, { 783.0f, 698.0f }, { 1068.0f, 698.0f }
}};

constexpr std::array<juce::Point<float>, 13> detailValueCentres
{{
    { 157.0f, 544.0f }, { 318.0f, 544.0f }, { 479.0f, 544.0f }, { 640.0f, 544.0f },
    { 801.0f, 544.0f }, { 962.0f, 544.0f }, { 1123.0f, 544.0f },
    { 238.0f, 691.0f }, { 399.0f, 691.0f }, { 560.0f, 691.0f },
    { 720.0f, 691.0f }, { 881.0f, 691.0f }, { 1042.0f, 691.0f }
}};
}

class LongLandAudioProcessorEditor::LandLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    explicit LandLookAndFeel (juce::Image atlasToUse) : atlas (std::move (atlasToUse))
    {
        setColour (juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::ComboBox::textColourId, cream);
        setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
        setColour (juce::ComboBox::arrowColourId, juce::Colours::transparentBlack);
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff17312c));
        setColour (juce::PopupMenu::textColourId, cream);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xffb64129));
        setColour (juce::PopupMenu::highlightedTextColourId, cream);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float position, float, float, juce::Slider& slider) override
    {
        if (! atlas.isValid())
            return;

        const auto frame = juce::jlimit (0, atlasFrameCount - 1,
                                         juce::roundToInt (position * static_cast<float> (atlasFrameCount - 1)));
        const auto sourceX = (frame % atlasColumns) * atlasCellSize;
        const auto sourceY = (frame / atlasColumns) * atlasCellSize;
        const auto side = juce::jmin (width, height);
        const auto destinationX = x + (width - side) / 2;
        const auto destinationY = y + (height - side) / 2;
        g.drawImage (atlas,
                     destinationX, destinationY, side, side,
                     sourceX, sourceY, atlasCellSize, atlasCellSize,
                     false);

        if (slider.hasKeyboardFocus (true))
        {
            g.setColour (amber.withAlpha (0.9f));
            g.drawEllipse (juce::Rectangle<float> (static_cast<float> (destinationX), static_cast<float> (destinationY),
                                                    static_cast<float> (side), static_cast<float> (side)).reduced (2.0f),
                           2.0f);
        }
    }

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool, bool) override
    {
        // The approved Blender plate contains the raised button hardware.
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& button, bool, bool) override
    {
        g.setColour (cream);
        g.setFont (smallCaps (juce::jmax (7.0f, static_cast<float> (button.getHeight()) * 0.19f)));
        g.drawFittedText (button.getButtonText(), button.getLocalBounds().reduced (5, 3),
                          juce::Justification::centred, 1);
    }

    void drawTickBox (juce::Graphics& g, juce::Component& component, float, float, float, float,
                      bool ticked, bool enabled, bool, bool) override
    {
        if (ticked)
        {
            auto box = component.getLocalBounds().toFloat().withSizeKeepingCentre (
                juce::jmin (18.0f, static_cast<float> (component.getWidth()) * 0.62f),
                juce::jmin (18.0f, static_cast<float> (component.getHeight()) * 0.62f));
            juce::Path tick;
            tick.startNewSubPath (box.getX() + 2.0f, box.getCentreY());
            tick.lineTo (box.getCentreX() - 1.0f, box.getBottom() - 2.0f);
            tick.lineTo (box.getRight() - 1.0f, box.getY() + 2.0f);
            g.setColour (cream.withAlpha (enabled ? 1.0f : 0.5f));
            g.strokePath (tick, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
        }

        if (component.hasKeyboardFocus (true))
        {
            g.setColour (amber);
            g.drawRoundedRectangle (component.getLocalBounds().toFloat().reduced (1.0f), 2.0f, 1.5f);
        }
    }

    void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override
    {
        // Selector hardware and arrow are part of the clean R4 plate.
    }

    juce::Font getComboBoxFont (juce::ComboBox& box) override
    {
        return smallCaps (juce::jmax (8.0f, static_cast<float> (box.getHeight()) * 0.27f));
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (box.getLocalBounds().reduced (14, 2).withTrimmedRight (36));
        label.setFont (getComboBoxFont (box));
        label.setJustificationType (juce::Justification::centredLeft);
        label.setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        label.setColour (juce::Label::textColourId, cream);
    }

private:
    juce::Image atlas;
};

LongLandAudioProcessorEditor::LongLandAudioProcessorEditor (LongLandAudioProcessor& owner)
    : AudioProcessorEditor (&owner), processor (owner),
      primaryPlate (loadEmbeddedImage (BinaryData::longland3dprimaryclean_png,
                                       BinaryData::longland3dprimaryclean_pngSize)),
      detailPlate (loadEmbeddedImage (BinaryData::longland3ddetailclean_png,
                                      BinaryData::longland3ddetailclean_pngSize)),
      knobAtlas (loadEmbeddedImage (BinaryData::longlandredknobatlas_png,
                                    BinaryData::longlandredknobatlas_pngSize)),
      look (std::make_unique<LandLookAndFeel> (knobAtlas))
{
    setLookAndFeel (look.get());
    setOpaque (true);

    struct ControlSpec { const char* id; const char* label; const char* help; };
    static constexpr ControlSpec specs[]
    {
        { "mix",       "MIX",            "Dry and reverberated signal balance" },
        { "decay",     "TIME",           "Reverberation decay time" },
        { "size",      "LAND / SIZE",    "Apparent acoustic field size" },
        { "age",       "WEATHER / AGE",  "Darkness, wear and nonlinear colour" },
        { "predelay",  "PRE-DELAY",      "Delay before the reverberated field" },
        { "diffusion", "DIFFUSION",      "Reflection density and smoothness" },
        { "earlyLate", "EARLY FIELD",    "Early reflection contribution" },
        { "modDepth",  "DRIFT",          "Delay-line modulation depth" },
        { "modRate",   "TIDE",           "Delay-line modulation rate" },
        { "lowCut",    "LOW SKY",        "Reverberated signal high-pass filter" },
        { "highCut",   "HIGH SKY",       "Reverberated signal low-pass filter" },
        { "width",     "EXPANSE",         "Stereo field width" },
        { "output",    "OUTPUT",          "Final output level" }
    };
    static_assert (std::size (specs) == controlCount);

    for (std::size_t index = 0; index < controlCount; ++index)
        configureControl (index, specs[index].id, specs[index].label, specs[index].help);

    programBox.setTitle ("Factory program");
    programBox.setDescription ("Load one of Long Land's six factory programs");
    programBox.setTooltip ("Factory programs. Moving any parameter marks the current state as edited.");
    programBox.addItem ("CUSTOM / EDITED", 100);
    for (int program = 0; program < processor.getNumPrograms(); ++program)
        programBox.addItem (processor.getProgramName (program), program + 1);
    programBox.setWantsKeyboardFocus (true);
    syncProgramBox();
    programBox.onChange = [this]
    {
        const auto selected = programBox.getSelectedId();
        if (selected >= 1 && selected <= processor.getNumPrograms())
            loadFactoryProgram (selected - 1);
        else
            syncProgramBox();
    };
    addAndMakeVisible (programBox);

    detailButton.setClickingTogglesState (true);
    detailButton.setTitle ("Toggle detailed parameters");
    detailButton.setDescription ("Switch between the four primary controls and nine direct detail parameters");
    detailButton.setTooltip ("Reveal all direct detail parameters");
    detailButton.setWantsKeyboardFocus (true);
    detailButton.onClick = [this] { setDetailView (detailButton.getToggleState()); };
    addAndMakeVisible (detailButton);

    freezeButton.setButtonText ({});
    freezeButton.setTitle ("Infinite decay");
    freezeButton.setDescription ("Sustain the current reverberated field indefinitely");
    freezeButton.setTooltip ("Infinite: sustain the reverb field");
    freezeButton.setWantsKeyboardFocus (true);
    addAndMakeVisible (freezeButton);
    freezeAttachment = std::make_unique<ButtonAttachment> (processor.state, "freeze", freezeButton);

    refreshReadoutText();
    setDetailView (false);
    setResizable (true, true);
    setResizeLimits (720, 450, 1440, 900);
    if (auto* boundsConstrainer = getConstrainer())
        boundsConstrainer->setFixedAspectRatio (16.0 / 10.0);
    setSize (960, 600);
    timerCallback();
    startTimerHz (12);
}

LongLandAudioProcessorEditor::~LongLandAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void LongLandAudioProcessorEditor::configureControl (std::size_t index, const char* parameterID,
                                                      const char* displayName, const char* helpText)
{
    auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (processor.state.getParameter (parameterID));
    jassert (parameter != nullptr);
    if (parameter == nullptr)
        return;

    auto& control = controls[index];
    const auto& range = parameter->getNormalisableRange();
    control.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    control.setRange (static_cast<double> (range.start), static_cast<double> (range.end),
                      static_cast<double> (range.interval));
    control.setSkewFactor (static_cast<double> (range.skew), range.symmetricSkew);
    control.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    control.setDoubleClickReturnValue (true, parameter->convertFrom0to1 (parameter->getDefaultValue()));
    control.setVelocityBasedMode (false);
    control.setTitle (displayName);
    control.setDescription (helpText);
    control.setTooltip (juce::String (displayName) + ": " + helpText
                        + ". Shift-drag for fine control; double-click to reset.");
    const auto id = juce::String (parameterID);
    const auto unitLabel = id == "decay" ? juce::String (" s")
                         : (id == "predelay" || id == "modDepth") ? juce::String (" ms")
                         : (id == "modRate" || id == "lowCut" || id == "highCut") ? juce::String (" Hz")
                         : id == "output" ? juce::String (" dB")
                         : juce::String (" %");
    addAndMakeVisible (control);

    // The approved clean plate already contains the permanent engraved label.
    auto& label = controlLabels[index];
    label.setText (displayName, juce::dontSendNotification);
    label.setVisible (false);

    controlAttachments.push_back (std::make_unique<SliderAttachment> (processor.state, parameterID, control));
    // SliderAttachment installs the parameter's default converters, so apply the
    // display/accessibility formatter afterwards to preserve visible units.
    control.textFromValueFunction = [parameter, unitLabel] (double value)
    {
        auto text = parameter->getText (parameter->convertTo0to1 (static_cast<float> (value)), 18);
        if (unitLabel.isNotEmpty() && ! text.endsWithIgnoreCase (unitLabel.trim()))
            text += unitLabel;
        return text;
    };
    control.valueFromTextFunction = [parameter] (const juce::String& text)
    {
        return parameter->convertFrom0to1 (parameter->getValueForText (text));
    };
    control.onValueChange = [this]
    {
        syncProgramBox();
        refreshReadoutText();
        repaint();
    };
}

void LongLandAudioProcessorEditor::syncProgramBox()
{
    if (loadingProgram)
        return;
    const auto selected = processor.isCurrentProgramEdited() ? 100 : processor.getCurrentProgram() + 1;
    if (programBox.getSelectedId() != selected)
        programBox.setSelectedId (selected, juce::dontSendNotification);
}

void LongLandAudioProcessorEditor::loadFactoryProgram (int programIndex)
{
    const juce::ScopedValueSetter<bool> loading (loadingProgram, true);
    processor.setCurrentProgram (programIndex);
    programBox.setSelectedId (programIndex + 1, juce::dontSendNotification);
    refreshReadoutText();
    repaint();
}

void LongLandAudioProcessorEditor::refreshReadoutText()
{
    for (std::size_t index = 0; index < controlCount; ++index)
        liveValueTexts[index] = controls[index].getTextFromValue (controls[index].getValue());

    const auto raw = [this] (const char* id) { return processor.state.getRawParameterValue (id)->load(); };
    screenTelemetry = "DECAY " + juce::String (raw ("decay"), 2) + " S   /   AGE "
                    + juce::String (juce::roundToInt (raw ("age"))) + "%   /   WIDTH "
                    + juce::String (juce::roundToInt (raw ("width"))) + "%   /   EARLY "
                    + juce::String (juce::roundToInt (raw ("earlyLate"))) + "%";
}

void LongLandAudioProcessorEditor::setDetailView (bool shouldShow)
{
    showingDetails = shouldShow;
    detailButton.setToggleState (showingDetails, juce::dontSendNotification);
    detailButton.setButtonText (showingDetails ? "PRIMARY" : "DETAIL");
    detailButton.setTooltip (showingDetails ? "Return to the four primary controls"
                                            : "Reveal all direct detail parameters");

    for (std::size_t index = 0; index < controlCount; ++index)
        controls[index].setVisible (showingDetails || index < primaryControlCount);

    resized();
    repaint();
}

void LongLandAudioProcessorEditor::timerCallback()
{
    syncProgramBox();
    refreshReadoutText();
    const auto sampleRate = processor.getSampleRate();
    const auto blockSize = processor.getBlockSize();
    const auto next = sampleRate > 0.0
        ? juce::String (sampleRate / 1000.0, 1) + " kHz   /   " + juce::String (blockSize)
            + " samples   /   64-bit VST3"
        : juce::String ("ENGINE IDLE   /   64-bit VST3");
    if (engineTelemetry != next)
    {
        engineTelemetry = next;
        repaint();
    }
}

void LongLandAudioProcessorEditor::drawLiveReadouts (juce::Graphics& g)
{
    const auto xScale = static_cast<float> (getWidth()) / sourceWidth;
    const auto yScale = static_cast<float> (getHeight()) / sourceHeight;
    const auto fontScale = juce::jmin (xScale, yScale);
    const auto drawValue = [&] (std::size_t index, juce::Point<float> centre, float width, float height, float fontSize)
    {
        const auto source = juce::Rectangle<float> (width, height).withCentre (centre);
        const auto target = scaleSourceRect (source, getWidth(), getHeight());
        g.setColour (cream);
        g.setFont (smallCaps (juce::jmax (7.0f, fontSize * fontScale)));
        g.drawFittedText (liveValueTexts[index], target.reduced (4, 1),
                          juce::Justification::centred, 1);
    };

    if (showingDetails)
    {
        for (std::size_t index = 0; index < controlCount; ++index)
            drawValue (index, detailValueCentres[index], 104.0f, 25.0f, 13.0f);
    }
    else
    {
        for (std::size_t index = 0; index < primaryControlCount; ++index)
            drawValue (index, primaryValueCentres[index], 128.0f, 30.0f, 16.0f);
    }

    g.setColour (amber);
    g.setFont (smallCaps (juce::jmax (7.0f, 12.0f * fontScale)));
    g.drawFittedText (screenTelemetry, scaleSourceRect ({ 390.0f, 322.0f, 500.0f, 34.0f }, getWidth(), getHeight()),
                      juce::Justification::centred, 1);

    g.setColour (cream.withAlpha (0.88f));
    g.setFont (smallCaps (juce::jmax (6.0f, 9.0f * fontScale)));
    g.drawFittedText (engineTelemetry, scaleSourceRect ({ 99.0f, 714.0f, 365.0f, 29.0f }, getWidth(), getHeight()),
                      juce::Justification::centredLeft, 1);
}

void LongLandAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff202321));
    const auto& plate = showingDetails ? detailPlate : primaryPlate;
    if (plate.isValid())
        g.drawImage (plate, getLocalBounds().toFloat());
    drawLiveReadouts (g);
}

void LongLandAudioProcessorEditor::resized()
{
    programBox.setBounds (scaleSourceRect ({ 708.0f, 68.0f, 292.0f, 50.0f }, getWidth(), getHeight()));
    detailButton.setBounds (scaleSourceRect ({ 1004.0f, 67.0f, 84.0f, 52.0f }, getWidth(), getHeight()));
    freezeButton.setBounds (scaleSourceRect ({ 1096.0f, 76.0f, 28.0f, 34.0f }, getWidth(), getHeight()));

    if (showingDetails)
    {
        for (std::size_t index = 0; index < controlCount; ++index)
        {
            const auto source = juce::Rectangle<float> (90.0f, 90.0f).withCentre (detailKnobCentres[index]);
            controls[index].setBounds (scaleSourceRect (source, getWidth(), getHeight()));
        }
    }
    else
    {
        for (std::size_t index = 0; index < primaryControlCount; ++index)
        {
            const auto source = juce::Rectangle<float> (224.0f, 224.0f).withCentre (primaryKnobCentres[index]);
            controls[index].setBounds (scaleSourceRect (source, getWidth(), getHeight()));
        }
    }
}
