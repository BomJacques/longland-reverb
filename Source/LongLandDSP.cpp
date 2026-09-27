#include "LongLandDSP.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace longland
{
namespace
{
constexpr std::array<float, 8> baseDelayMs { 61.71f, 73.39f, 89.03f, 103.57f,
                                              117.19f, 131.87f, 149.23f, 167.63f };
constexpr std::array<float, 8> phaseOffsets { 0.00f, 0.37f, 0.71f, 0.19f,
                                               0.53f, 0.86f, 0.29f, 0.64f };
constexpr std::array<float, 8> injectionL { 1, -1, 1, 1, -1, 1, -1, -1 };
constexpr std::array<float, 8> injectionR { 1, 1, -1, 1, -1, -1, 1, -1 };
constexpr std::array<float, 8> outputL { 1, -1, 1, -1, 1, 1, -1, -1 };
constexpr std::array<float, 8> outputR { -1, 1, 1, -1, -1, 1, 1, -1 };
constexpr std::array<float, 8> decaySpread { 0.82f, 0.88f, 0.94f, 1.00f,
                                              1.06f, 1.12f, 1.20f, 1.30f };

// One fixed calibration for every source and sample rate.  Round-one's FDN
// normalization was intentionally conservative, but left the fully-wet return
// about 12 dB below the comparison halls.
constexpr float wetCalibration = 3.80f;

float clamp01 (float value) noexcept { return std::clamp (value, 0.0f, 1.0f); }
float onePoleCoefficient (float hz, double rate) noexcept
{
    return std::exp (-2.0f * std::numbers::pi_v<float> * std::clamp (hz, 10.0f, 0.45f * static_cast<float> (rate))
                     / static_cast<float> (rate));
}
} // namespace

void ReverbEngine::DelayLine::resize (std::size_t newSize)
{
    samples.assign (std::max<std::size_t> (newSize, 4), 0.0f);
    write = 0;
}

void ReverbEngine::DelayLine::clear() noexcept
{
    std::fill (samples.begin(), samples.end(), 0.0f);
    write = 0;
}

float ReverbEngine::DelayLine::read (float delaySamples) const noexcept
{
    if (samples.empty()) return 0.0f;
    const auto bufferSize = static_cast<float> (samples.size());
    float position = static_cast<float> (write) - std::clamp (delaySamples, 1.0f, bufferSize - 2.0f);
    while (position < 0.0f) position += bufferSize;
    const auto i0 = static_cast<std::size_t> (position) % samples.size();
    const auto i1 = (i0 + 1) % samples.size();
    const auto fraction = position - std::floor (position);
    return samples[i0] + fraction * (samples[i1] - samples[i0]);
}

void ReverbEngine::DelayLine::push (float value) noexcept
{
    samples[write] = value;
    write = (write + 1) % samples.size();
}

float ReverbEngine::Allpass::process (float input) noexcept
{
    const auto delayed = delay.read (static_cast<float> (delay.samples.size() - 2));
    const auto output = delayed - coefficient * input;
    delay.push (input + coefficient * output);
    return output;
}

void ReverbEngine::Smoothed::prepare (double rate, float milliseconds) noexcept
{
    coefficient = std::exp (-1.0f / static_cast<float> (rate * milliseconds * 0.001));
}

void ReverbEngine::Smoothed::reset (float value) noexcept { current = target = value; }
float ReverbEngine::Smoothed::next() noexcept
{
    current = target + coefficient * (current - target);
    return current;
}

void ReverbEngine::prepare (double newSampleRate, int)
{
    sampleRate = std::max (8000.0, newSampleRate);
    scatteringCoefficient = std::exp (-1.0f / static_cast<float> (sampleRate * 0.12));
    for (auto& line : preDelay)
        line.resize (static_cast<std::size_t> (sampleRate * 2.1));

    constexpr float diffuserMs[numDiffusers] { 4.73f, 7.31f, 10.09f, 13.61f };
    for (int channel = 0; channel < 2; ++channel)
        for (int i = 0; i < numDiffusers; ++i)
            diffusers[channel][i].delay.resize (static_cast<std::size_t> (
                sampleRate * 0.001 * diffuserMs[i] * (channel == 0 ? 1.0 : 1.071)) + 3);

    for (auto& line : tank)
        line.resize (static_cast<std::size_t> (sampleRate * 0.42) + 4);

    for (auto* smoother : { &mix, &preDelayTime, &decay, &size, &diffusion, &modDepth,
                            &modRate, &lowCut, &highCut, &width, &age, &earlyLate,
                            &outputGain, &freeze })
        smoother->prepare (sampleRate, smoother == &freeze ? 80.0f : 25.0f);

    mix.reset (parameters.mix);
    preDelayTime.reset (parameters.preDelayMs);
    decay.reset (parameters.decaySeconds);
    size.reset (parameters.size);
    diffusion.reset (parameters.diffusion);
    modDepth.reset (parameters.modDepthMs);
    modRate.reset (parameters.modRateHz);
    lowCut.reset (parameters.lowCutHz);
    highCut.reset (parameters.highCutHz);
    width.reset (parameters.width);
    age.reset (parameters.age);
    earlyLate.reset (parameters.earlyLate);
    outputGain.reset (dbToGain (parameters.outputDb));
    freeze.reset (parameters.freeze ? 1.0f : 0.0f);
    reset();
}

void ReverbEngine::reset() noexcept
{
    for (auto& line : preDelay) line.clear();
    for (auto& channel : diffusers) for (auto& allpass : channel) allpass.clear();
    for (auto& line : tank) line.clear();
    damping.fill (0.0f);
    dcState.fill (0.0f);
    std::fill (std::begin (inputLowpass), std::end (inputLowpass), 0.0f);
    std::fill (std::begin (inputHighpass), std::end (inputHighpass), 0.0f);
    std::fill (std::begin (previousInput), std::end (previousInput), 0.0f);
    phase = phaseOffsets;
    randomState = 0x6d2b79f5u;
    wetTone.fill (0.0f);
    constexpr float scatteringDepth = 0.52f;
    constexpr float scatteringNormalisation = 0.9578f;
    for (int i = 0; i < numLines; ++i)
    {
        scatteringGain[i] = scatteringTarget[i]
            = scatteringNormalisation * (1.0f + scatteringDepth * (2.0f * nextRandomUnit() - 1.0f));
        scatteringCountdown[i] = std::max<std::int32_t> (1, static_cast<std::int32_t> (
            sampleRate * (0.18 + 0.22 * nextRandomUnit()
                          + 0.10 / (0.08 + std::clamp (parameters.modRateHz, 0.01f, 5.0f)))));
    }
    sampleCounter = 0;
}

void ReverbEngine::setParameters (const Parameters& p) noexcept
{
    parameters = p;
    mix.set (clamp01 (p.mix));
    preDelayTime.set (std::clamp (p.preDelayMs, 0.0f, 2000.0f));
    decay.set (std::clamp (p.decaySeconds, 0.15f, 60.0f));
    size.set (clamp01 (p.size));
    diffusion.set (clamp01 (p.diffusion));
    modDepth.set (std::clamp (p.modDepthMs, 0.0f, 12.0f));
    modRate.set (std::clamp (p.modRateHz, 0.01f, 5.0f));
    lowCut.set (std::clamp (p.lowCutHz, 20.0f, 2000.0f));
    highCut.set (std::clamp (p.highCutHz, 800.0f, 20000.0f));
    width.set (std::clamp (p.width, 0.0f, 2.0f));
    age.set (clamp01 (p.age));
    earlyLate.set (clamp01 (p.earlyLate));
    outputGain.set (dbToGain (std::clamp (p.outputDb, -24.0f, 12.0f)));
    freeze.set (p.freeze ? 1.0f : 0.0f);
}

float ReverbEngine::processInputFilter (float input, int channel, float hpHz, float lpHz) noexcept
{
    const auto lpCoeff = onePoleCoefficient (lpHz, sampleRate);
    inputLowpass[channel] = (1.0f - lpCoeff) * input + lpCoeff * inputLowpass[channel];
    const auto hpCoeff = onePoleCoefficient (hpHz, sampleRate);
    inputHighpass[channel] = hpCoeff * (inputHighpass[channel] + inputLowpass[channel] - previousInput[channel]);
    previousInput[channel] = inputLowpass[channel];
    return inputHighpass[channel];
}

float ReverbEngine::softLimit (float value) noexcept
{
    const auto x = std::clamp (value, -3.0f, 3.0f);
    return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
}

float ReverbEngine::nextRandomUnit() noexcept
{
    randomState ^= randomState << 13;
    randomState ^= randomState >> 17;
    randomState ^= randomState << 5;
    return static_cast<float> (randomState >> 8) * (1.0f / 16777216.0f);
}

float ReverbEngine::dbToGain (float db) noexcept { return std::pow (10.0f, db * 0.05f); }

void ReverbEngine::process (float* left, float* right, int numSamples) noexcept
{
    if (left == nullptr || right == nullptr || numSamples <= 0) return;

    for (int sample = 0; sample < numSamples; ++sample, ++sampleCounter)
    {
        const auto dryL = left[sample];
        const auto dryR = right[sample];
        const auto currentMix = mix.next();
        const auto currentPreDelay = preDelayTime.next();
        const auto currentDecay = decay.next();
        const auto currentSize = size.next();
        const auto currentDiffusion = diffusion.next();
        const auto currentDepth = modDepth.next();
        const auto currentRate = modRate.next();
        const auto currentLowCut = lowCut.next();
        const auto currentHighCut = highCut.next();
        const auto currentWidth = width.next();
        const auto currentAge = age.next();
        const auto currentEarly = earlyLate.next();
        const auto gain = outputGain.next();
        const auto frozen = freeze.next();

        auto inL = processInputFilter (dryL, 0, currentLowCut, currentHighCut);
        auto inR = processInputFilter (dryR, 1, currentLowCut, currentHighCut);
        const auto delayInSamples = currentPreDelay * 0.001f * static_cast<float> (sampleRate) + 1.0f;
        const auto predL = preDelay[0].read (delayInSamples);
        const auto predR = preDelay[1].read (delayInSamples);
        preDelay[0].push (inL);
        preDelay[1].push (inR);

        // Six decorrelated early taps supply TrueVerb-like spatial definition.
        const auto scale = 0.52f + currentSize * 0.78f;
        auto earlyL = 0.34f * preDelay[0].read (delayInSamples + static_cast<float> (sampleRate * 0.0113 * scale))
                    - 0.24f * preDelay[1].read (delayInSamples + static_cast<float> (sampleRate * 0.0197 * scale))
                    + 0.19f * preDelay[0].read (delayInSamples + static_cast<float> (sampleRate * 0.0311 * scale));
        auto earlyR = 0.34f * preDelay[1].read (delayInSamples + static_cast<float> (sampleRate * 0.0127 * scale))
                    - 0.24f * preDelay[0].read (delayInSamples + static_cast<float> (sampleRate * 0.0229 * scale))
                    + 0.19f * preDelay[1].read (delayInSamples + static_cast<float> (sampleRate * 0.0349 * scale));

        auto diffL = predL;
        auto diffR = predR;
        for (int i = 0; i < numDiffusers; ++i)
        {
            diffusers[0][i].coefficient = 0.18f + 0.58f * currentDiffusion;
            diffusers[1][i].coefficient = 0.18f + 0.58f * currentDiffusion;
            diffL = diffusers[0][i].process (diffL);
            diffR = diffusers[1][i].process (diffR);
        }

        // Treat the diffuser output as the dense end of the early field.  The
        // discrete taps establish location; this low-level allpass return fills
        // the gaps before the tank reaches statistical density.
        earlyL += 0.22f * diffL;
        earlyR += 0.22f * diffR;

        std::array<float, numLines> delayed {};
        float sum = 0.0f;
        const auto rateRadians = 2.0f * std::numbers::pi_v<float> * currentRate / static_cast<float> (sampleRate);
        for (int i = 0; i < numLines; ++i)
        {
            phase[i] += rateRadians * (0.81f + 0.047f * static_cast<float> (i));
            if (phase[i] >= 2.0f * std::numbers::pi_v<float>) phase[i] -= 2.0f * std::numbers::pi_v<float>;
            const auto base = baseDelayMs[i] * (0.48f + currentSize * 1.16f);
            const auto wander = currentDepth * std::sin (phase[i]) * (0.63f + 0.05f * static_cast<float> (i));
            delayed[i] = tank[i].read ((base + wander) * 0.001f * static_cast<float> (sampleRate));

            const auto dampingHz = currentHighCut * (1.0f - 0.52f * currentAge);
            const auto dampCoeff = onePoleCoefficient (dampingHz, sampleRate);
            damping[i] = (1.0f - dampCoeff) * delayed[i] + dampCoeff * damping[i];
            // The wet input is already high-passed. Avoid another pole in the
            // loop so the requested RT60 remains predictable and unit-stable.
            delayed[i] = damping[i];
            sum += delayed[i];
        }

        float lateL = 0.0f, lateR = 0.0f;
        const auto monoInput = 0.5f * (diffL + diffR);
        const auto sideInput = 0.5f * (diffL - diffR);
        const auto sourceSide = 0.5f * (predL - predR);
        for (int i = 0; i < numLines; ++i)
        {
            const auto householder = delayed[i] - 0.25f * sum;
            const auto lineSeconds = baseDelayMs[i] * (0.48f + currentSize * 1.16f) * 0.001f;
            auto feedbackGain = std::pow (10.0f, -3.0f * lineSeconds / (currentDecay * decaySpread[i]));
            feedbackGain += frozen * (0.99975f - feedbackGain);
            const auto inputAmount = 1.0f - frozen;
            const auto injection = (monoInput * injectionL[i] + sideInput * injectionR[i] * 0.72f)
                                   * 0.31f * inputAmount;
            auto writeValue = householder * feedbackGain + injection;
            writeValue = writeValue * (1.0f - currentAge * 0.11f)
                       + softLimit (writeValue * (1.0f + currentAge * 2.7f))
                         * (currentAge * 0.11f);
            tank[i].push (writeValue);
            // Slowly changing independent mode weights mimic the irregular
            // energy exchange of a coupled hall.  Fixed-seed randomness keeps
            // renders reproducible without imposing a coherent cyclic pump.
            if (--scatteringCountdown[i] <= 0)
            {
                constexpr float scatteringDepth = 0.52f;
                constexpr float scatteringNormalisation = 0.9578f;
                scatteringTarget[i] = scatteringNormalisation
                    * (1.0f + scatteringDepth * (2.0f * nextRandomUnit() - 1.0f));
                const auto intervalSeconds = 0.18f + 0.22f * nextRandomUnit()
                                           + 0.10f / (0.08f + currentRate);
                scatteringCountdown[i] = std::max<std::int32_t> (
                    1, static_cast<std::int32_t> (sampleRate * intervalSeconds));
            }
            scatteringGain[i] = scatteringTarget[i]
                              + scatteringCoefficient * (scatteringGain[i] - scatteringTarget[i]);
            const auto motion = scatteringGain[i];
            lateL += delayed[i] * outputL[i] * motion;
            lateR += delayed[i] * outputR[i] * motion;
        }
        lateL *= 0.25f;
        lateR *= 0.25f;

        auto wetL = earlyL * (2.15f * currentEarly) + lateL * (1.0f - 0.42f * currentEarly);
        auto wetR = earlyR * (2.15f * currentEarly) + lateR * (1.0f - 0.42f * currentEarly);
        const auto mid = 0.5f * (wetL + wetR);
        // A modest direct early-side feed restores width for genuinely stereo
        // sources without affecting a mono impulse or detecting source type.
        const auto side = (0.5f * (wetL - wetR) + 0.205f * sourceSide) * currentWidth;

        // Vintage hall returns lose high-frequency energy most strongly in the
        // centre, while a lightly voiced side field preserves spaciousness.
        // This one-pole M/S calibration leaves low-frequency pad and vocal body
        // intact and avoids changing the tank's decay, density or modulation.
        const auto midTone = onePoleCoefficient (currentHighCut * 0.49f, sampleRate);
        const auto sideTone = onePoleCoefficient (std::min (20000.0f, currentHighCut * 2.2f), sampleRate);
        wetTone[0] = (1.0f - midTone) * mid + midTone * wetTone[0];
        wetTone[1] = (1.0f - sideTone) * side + sideTone * wetTone[1];
        wetL = wetTone[0] + wetTone[1];
        wetR = wetTone[0] - wetTone[1];

        // Equal-power mix keeps automated sweeps level-stable.
        const auto dryGain = std::cos (currentMix * 0.5f * std::numbers::pi_v<float>);
        const auto wetGain = std::sin (currentMix * 0.5f * std::numbers::pi_v<float>);
        left[sample] = (dryL * dryGain + wetL * (wetGain * wetCalibration)) * gain;
        right[sample] = (dryR * dryGain + wetR * (wetGain * wetCalibration)) * gain;
    }
}
} // namespace longland
