#include "LongLandDSP.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <numeric>
#include <string>
#include <vector>

namespace
{
int failures = 0;
void expect (bool condition, const std::string& message)
{
    std::cout << (condition ? "PASS " : "FAIL ") << message << '\n';
    failures += condition ? 0 : 1;
}

struct Render
{
    std::vector<float> l, r;
};

Render renderImpulse (double sampleRate, float seconds, longland::Parameters p, int blockSize)
{
    const auto count = static_cast<int> (sampleRate * seconds);
    Render result { std::vector<float> (count), std::vector<float> (count) };
    result.l[0] = result.r[0] = 1.0f;
    longland::ReverbEngine engine;
    engine.setParameters (p);
    engine.prepare (sampleRate, blockSize);
    for (int offset = 0; offset < count; offset += blockSize)
    {
        const auto n = std::min (blockSize, count - offset);
        engine.process (result.l.data() + offset, result.r.data() + offset, n);
    }
    return result;
}

double energy (const std::vector<float>& signal, int begin, int end)
{
    begin = std::clamp (begin, 0, static_cast<int> (signal.size()));
    end = std::clamp (end, begin, static_cast<int> (signal.size()));
    return std::inner_product (signal.begin() + begin, signal.begin() + end,
                               signal.begin() + begin, 0.0);
}
}

int main()
{
    longland::Parameters p;
    p.mix = 1.0f;
    p.preDelayMs = 25.0f;
    p.decaySeconds = 3.0f;
    p.size = 0.7f;
    const auto a = renderImpulse (48000.0, 7.0f, p, 64);
    const auto b = renderImpulse (48000.0, 7.0f, p, 511);

    expect (a.l == b.l && a.r == b.r, "render is bit-deterministic across host block sizes");
    const auto finite = std::all_of (a.l.begin(), a.l.end(), [] (float x) { return std::isfinite (x); })
                     && std::all_of (a.r.begin(), a.r.end(), [] (float x) { return std::isfinite (x); });
    expect (finite, "impulse response contains no NaN/Inf");
    const auto peak = std::max (*std::max_element (a.l.begin(), a.l.end()),
                                *std::max_element (a.r.begin(), a.r.end()));
    expect (peak < 2.0f, "full-scale impulse remains bounded below +6 dBFS");

    const auto predelaySilence = energy (a.l, 1, static_cast<int> (0.020 * 48000));
    expect (predelaySilence < 1.0e-10, "25 ms predelay has no early leakage before 20 ms");
    const auto earlyEnergy = energy (a.l, static_cast<int> (0.025 * 48000), static_cast<int> (0.25 * 48000));
    expect (earlyEnergy > 1.0e-5, "impulse excites an audible early/late field");
    const auto tailEarly = energy (a.l, 48000, 96000) + energy (a.r, 48000, 96000);
    const auto tailLate = energy (a.l, 240000, 288000) + energy (a.r, 240000, 288000);
    expect (tailEarly > tailLate * 30.0, "3 s decay loses at least 14.8 dB energy by the fifth second");
    expect (tailLate > 1.0e-12, "late tail remains numerically alive rather than hard-truncated");

    longland::Parameters frozen = p;
    longland::ReverbEngine freezeEngine;
    freezeEngine.setParameters (frozen);
    freezeEngine.prepare (48000.0, 128);
    Render f { std::vector<float> (288000), std::vector<float> (288000) };
    f.l[0] = f.r[0] = 1.0f;
    for (int offset = 0; offset < 288000; offset += 128)
    {
        if (offset == 96000) { frozen.freeze = true; freezeEngine.setParameters (frozen); }
        const auto n = std::min (128, 288000 - offset);
        freezeEngine.process (f.l.data() + offset, f.r.data() + offset, n);
    }
    const auto fEarly = energy (f.l, 96000, 144000) + energy (f.r, 96000, 144000);
    const auto fLate = energy (f.l, 240000, 288000) + energy (f.r, 240000, 288000);
    expect (fLate > fEarly * 0.45, "freeze retains at least -3.5 dB energy over three seconds");

    longland::ReverbEngine automation;
    automation.prepare (96000.0, 1);
    std::vector<float> l (96000, 0.08f), r (96000, -0.08f);
    for (int i = 0; i < 96000; ++i)
    {
        longland::Parameters automated = p;
        automated.mix = static_cast<float> (i) / 95999.0f;
        automated.size = 1.0f - automated.mix;
        automated.decaySeconds = 0.2f + 40.0f * automated.mix;
        automation.setParameters (automated);
        automation.process (&l[i], &r[i], 1);
    }
    float maxDelta = 0.0f;
    for (std::size_t i = 1; i < l.size(); ++i) maxDelta = std::max (maxDelta, std::abs (l[i] - l[i - 1]));
    expect (maxDelta < 0.25f, "extreme sample-rate automation remains click-free and bounded");

    std::cout << "METRIC peak=" << peak << " earlyEnergy=" << earlyEnergy
              << " tailDropDb=" << 10.0 * std::log10 (tailLate / tailEarly)
              << " automationMaxDelta=" << maxDelta << '\n';
    return failures == 0 ? 0 : 1;
}
