#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace longland
{
struct Parameters
{
    float mix = 0.35f;
    float preDelayMs = 24.0f;
    float decaySeconds = 7.5f;
    float size = 0.72f;
    float diffusion = 0.82f;
    float modDepthMs = 1.8f;
    float modRateHz = 0.23f;
    float lowCutHz = 90.0f;
    float highCutHz = 7600.0f;
    float width = 1.15f;
    float age = 0.22f;
    float earlyLate = 0.27f;
    float outputDb = 0.0f;
    bool freeze = false;
};

class ReverbEngine
{
public:
    void prepare (double sampleRate, int maximumBlockSize);
    void reset() noexcept;
    void setParameters (const Parameters& newParameters) noexcept;
    void process (float* left, float* right, int numSamples) noexcept;
    [[nodiscard]] double getSampleRate() const noexcept { return sampleRate; }

private:
    static constexpr int numLines = 8;
    static constexpr int numDiffusers = 4;

    struct DelayLine
    {
        std::vector<float> samples;
        std::size_t write = 0;
        void resize (std::size_t newSize);
        void clear() noexcept;
        float read (float delaySamples) const noexcept;
        void push (float value) noexcept;
    };

    struct Allpass
    {
        DelayLine delay;
        float coefficient = 0.65f;
        float process (float input) noexcept;
        void clear() noexcept { delay.clear(); }
    };

    struct Smoothed
    {
        float current = 0.0f;
        float target = 0.0f;
        float coefficient = 0.0f;
        void prepare (double rate, float milliseconds) noexcept;
        void reset (float value) noexcept;
        void set (float value) noexcept { target = value; }
        float next() noexcept;
    };

    float processInputFilter (float input, int channel, float lowCut, float highCut) noexcept;
    float nextRandomUnit() noexcept;
    static float softLimit (float value) noexcept;
    static float dbToGain (float db) noexcept;

    double sampleRate = 48000.0;
    Parameters parameters;
    DelayLine preDelay[2];
    Allpass diffusers[2][numDiffusers];
    std::array<DelayLine, numLines> tank;
    std::array<float, numLines> damping {};
    std::array<float, numLines> dcState {};
    std::array<float, numLines> phase {};
    std::array<float, numLines> scatteringGain {};
    std::array<float, numLines> scatteringTarget {};
    std::array<std::int32_t, numLines> scatteringCountdown {};
    std::array<float, 2> wetTone {};
    std::uint32_t randomState = 0x6d2b79f5u;
    float scatteringCoefficient = 0.0f;
    float inputLowpass[2] {};
    float inputHighpass[2] {};
    float previousInput[2] {};
    std::uint64_t sampleCounter = 0;

    Smoothed mix, preDelayTime, decay, size, diffusion, modDepth, modRate;
    Smoothed lowCut, highCut, width, age, earlyLate, outputGain, freeze;
};
} // namespace longland
