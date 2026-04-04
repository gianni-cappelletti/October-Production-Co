# octobir-core

Platform-agnostic impulse response convolution DSP library.

## Features

- Dual IR slot loading (WAV, mono/stereo)
- Automatic resampling to target sample rate
- FFT-based convolution via WDL ConvolutionEngine
- Static and dynamic blend between IR slots
- Dynamic mode with peak or RMS detection
- Sidechain input for external envelope control
- Per-IR enable/disable and trim gain
- Configurable attack/release envelope smoothing with soft-knee threshold
- Latency-compensated delay alignment across IR slots
- Multiple processing modes: mono, stereo, dual mono, mono-to-stereo
- IR slot swapping
- Zero VCV/JUCE dependencies

## Usage

```cpp
#include <octobir-core/IRProcessor.hpp>

octob::IRProcessor processor;
processor.setSampleRate(48000.0);
processor.setMaxBlockSize(512);

std::string error;
if (processor.loadImpulseResponse1("/path/to/cabinet.wav", error)) {
    // Optionally load a second IR for blending
    processor.loadImpulseResponse2("/path/to/reverb.wav", error);
    processor.setBlend(0.5f);

    float inL[512], inR[512], outL[512], outR[512];
    processor.processStereo(inL, inR, outL, outR, 512);
} else {
    std::cerr << "Failed to load IR: " << error << std::endl;
}
```

## API Reference

### IRProcessor

Main convolution processor class.

#### IR Loading

- `bool loadImpulseResponse1(const std::string& filepath, std::string& errorMessage)`
- `bool loadImpulseResponse2(const std::string& filepath, std::string& errorMessage)`
  - Load an impulse response into slot 1 or 2
  - Returns `true` on success; `errorMessage` contains details on failure

- `void clearImpulseResponse1()`
- `void clearImpulseResponse2()`
  - Remove the loaded IR from a slot

- `void swapIRSlots()`
  - Swap the contents of IR slot 1 and slot 2

#### Configuration

- `void setSampleRate(SampleRate sampleRate)` - Set processing sample rate (resamples loaded IRs as needed)
- `void setMaxBlockSize(FrameCount maxBlockSize)` - Set maximum expected buffer size
- `void setBlend(float blend)` - Set static blend position (0.0 = IR1 only, 1.0 = IR2 only)
- `void setOutputGain(float gainDb)` - Output gain in dB
- `void setIRATrimGain(float gainDb)` - Per-slot trim for IR A in dB
- `void setIRBTrimGain(float gainDb)` - Per-slot trim for IR B in dB
- `void setIRAEnabled(bool enabled)` - Enable/disable IR A
- `void setIRBEnabled(bool enabled)` - Enable/disable IR B

#### Dynamic Mode

- `void setDynamicModeEnabled(bool enabled)` - Enable dynamics-driven blending
- `void setSidechainEnabled(bool enabled)` - Use sidechain input for envelope detection
- `void setThreshold(float thresholdDb)` - Threshold in dB for dynamic blend mapping
- `void setRangeDb(float rangeDb)` - Range in dB over which blend sweeps
- `void setKneeWidthDb(float kneeDb)` - Soft knee width in dB
- `void setDetectionMode(int mode)` - 0 = peak, 1 = RMS
- `void setAttackTime(float attackTimeMs)` - Envelope attack in ms
- `void setReleaseTime(float releaseTimeMs)` - Envelope release in ms

#### Processing

- `void processMono(...)` - Mono in/out
- `void processStereo(...)` - Stereo in/out
- `void processDualMono(...)` - Independent L/R mono processing
- `void processMonoToStereo(...)` - Mono input to stereo output
- `void processMonoWithSidechain(...)` - Mono with sidechain envelope
- `void processMonoToStereoWithSidechain(...)` - Mono-to-stereo with sidechain
- `void processStereoWithSidechain(...)` - Stereo with sidechain envelope
- `void processDualMonoWithSidechain(...)` - Dual mono with sidechain

#### State Queries

- `bool isIR1Loaded() const` / `bool isIR2Loaded() const`
- `std::string getCurrentIR1Path() const` / `std::string getCurrentIR2Path() const`
- `int getLatencySamples() const`
- `float getCurrentInputLevel() const` - Current detected input level in dB
- `float getCurrentBlend() const` - Current blend position (after dynamic envelope)
- `float calculateDynamicBlend(float inputLevelDb) const` - Preview blend for a given input level

### IRLoader

Handles WAV file loading and resampling.

- `IRLoadResult loadFromFile(const std::string& filepath)` - Load WAV file
- `bool resampleAndInitialize(WDL_ImpulseBuffer& impulseBuffer, SampleRate targetSampleRate)` - Resample to target rate and initialize WDL buffer

### AudioBuffer

Simple audio buffer wrapper.

- `void resize(FrameCount numFrames)` - Resize buffer
- `void clear()` - Zero all samples
- `Sample* getData()` - Get raw data pointer
- `FrameCount getNumFrames() const` - Get buffer size

## Dependencies

- **WDL** (Winamp Developmental Library from Cockos)
  - Source: https://github.com/justinfrankel/WDL
  - ConvolutionEngine (`convoengine.cpp`) - FFT-based convolution
  - FFT (`fft.c`) - Fast Fourier Transform
  - Resampler (`resample.cpp`) - Sample rate conversion
  - License: zlib-style (permissive, GPL-compatible)
- **pffft** - Fast Fourier Transform
  - Source: https://github.com/marton78/pffft
  - License: BSD-style (permissive, GPL-compatible)
- **dr_wav** - WAV file loading (header-only)
  - License: Public Domain / MIT No Attribution

## License

GNU General Public License v3.0 (GPL-3.0)

This core library is part of the OctobIR project and is licensed under GPL-3.0 to ensure compatibility with JUCE (GPL-3.0) and VCV Rack (GPL-3.0+) requirements.
