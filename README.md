# Long Land

Long Land is a 64-bit stereo VST3 reverb for ambient, lo-fi, and vaporwave production. Its engine combines a multi-tap early-reflection field, eight orthogonally mixed modulated feedback delay lines, frequency-dependent decay, input diffusion, tape-like age, and a seamless infinite freeze. The plug-in is built with JUCE 8 and exposes automatable parameters and host-managed state/presets.

## Build on Windows

Requirements: Visual Studio with Desktop C++, CMake/Ninja, and JUCE 8. On the supplied machine these are auto-detected at Visual Studio 2026 and `C:\SDK\JUCE`.

```powershell
.\build.ps1 -Configuration Release
```

The VST3 bundle is emitted beneath `plugin/build-plugin/LongLand_artefacts/Release/VST3/Long Land.vst3`. Copy that bundle to a directory scanned by Reason (normally `C:\Program Files\Common Files\VST3`) and rescan plug-ins. The standalone build is also emitted for host-independent checking.

For a fast DSP-only verification:

```powershell
.\build.ps1 -Configuration Release -TestsOnly
```

## Measurable targets

- Bit-identical offline rendering across host block sizes.
- No allocations, locks, file access, or parameter-tree access inside the sample loop.
- Stable at 8–192 kHz, mono-to-stereo or stereo, and block sizes 1–8192.
- Predelay 0–2000 ms; decay 0.15–60 s; click-smoothed automation.
- Full-scale impulse peak below +6 dBFS; finite output under extreme automation.
- Freeze retains at least −3.5 dB energy across a three-second measurement window.

Listening comparisons against proprietary references still require those plug-ins and licensed test renders; this repository intentionally does not claim a completed blind win without them.
