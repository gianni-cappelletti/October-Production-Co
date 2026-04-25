# octobass-core

Platform-agnostic bass processing DSP library.

## Features

- Two-band crossover filtering (Linkwitz-Riley topology)
- Four compressor modes: VCA, FET, Opto, Bus
- Unified "squash" control for compression intensity across modes
- Noise gate with hysteresis and peak detection
- 24-band graphic EQ (ISO 1/3-octave spacing, ~28 Hz to 12.6 kHz)
- Neural Amp Modeler (NAM) integration for amp modeling in the high-frequency band
- IR convolution in the high-frequency chain (via octobir-core)
- Low/high band solo with mutually exclusive selection
- Per-band level control and high-band input/output gain
- Dry/wet mix and high-band mix controls
- Latency-compensated delay alignment between crossover bands
- Zero JUCE dependencies

## Usage

```cpp
#include <octobass-core/BassProcessor.hpp>

octob::BassProcessor processor;
processor.setSampleRate(48000.0);
processor.setMaxBlockSize(512);

processor.setCrossoverFrequency(250.0f);
processor.setSquash(0.5f);
processor.setCompressionMode(0); // VCA

std::string error;
processor.loadNamModel("/path/to/model.nam", error);
processor.loadImpulseResponse("/path/to/cabinet.wav", error);

float input[512], output[512];
processor.processMono(input, output, 512);
```

## API Reference

### BassProcessor

Main bass processing class.

#### Configuration

- `void setSampleRate(SampleRate sampleRate)` - Set processing sample rate
- `void setMaxBlockSize(FrameCount maxBlockSize)` - Set maximum expected buffer size

#### IR Loading

- `bool loadImpulseResponse(const std::string& filepath, std::string& errorMessage)` - Load IR into the high-frequency chain
- `void clearImpulseResponse()` - Remove loaded IR
- `bool isIRLoaded() const` / `std::string getCurrentIRPath() const`

#### NAM Model Loading

- `bool loadNamModel(const std::string& filepath, std::string& errorMessage)` - Load a Neural Amp Model
- `void clearNamModel()` - Remove loaded model
- `bool isNamModelLoaded() const` / `std::string getCurrentNamModelPath() const`

#### Crossover

- `void setCrossoverFrequency(float frequencyHz)` - Set crossover split frequency (50-800 Hz)
- `float getCrossoverFrequency() const`

#### Compressor

- `void setSquash(float amount)` - Set compression intensity (0.0-1.0)
- `void setCompressionMode(int mode)` - 0=VCA, 1=FET, 2=Opto, 3=Bus
- `float getSquash() const` / `int getCompressionMode() const`

#### Noise Gate

- `void setGateThreshold(float thresholdDb)` - Set noise gate threshold
- `float getGateThreshold() const`

#### Band Control

- `void setHighBandMix(float mix)` - High-frequency band wet/dry mix
- `void setLowBandSolo(bool solo)` - Solo the low band
- `void setHighBandSolo(bool solo)` - Solo the high band

#### Graphic EQ

- `void setGraphicEQBandGain(int bandIndex, float gainDb)` - Set gain for a specific band
- `float getGraphicEQBandGain(int bandIndex) const` - Get gain for a specific band

#### Levels

- `void setLowBandLevel(float levelDb)` - Low band output level
- `void setHighInputGain(float gainDb)` - High band input gain
- `void setHighOutputGain(float gainDb)` - High band output gain
- `void setOutputGain(float gainDb)` - Master output gain
- `void setDryWetMix(float mix)` - Overall dry/wet mix

#### Processing

- `void processMono(const Sample* input, Sample* output, FrameCount numFrames)` - Process a mono buffer
- `void reset()` - Reset all internal state

#### State Queries

- `int getLatencySamples() const` - Current processing latency

## Dependencies

- **octobir-core** - IR convolution for the high-frequency chain (GPL-3.0)
- **NeuralAmpModelerCore** - Neural amp modeling - MIT
  - Source: https://github.com/sdatkinson/NeuralAmpModelerCore
- **Eigen** - Linear algebra (via NeuralAmpModelerCore) - MPL-2.0
- **nlohmann/json** - JSON parsing (via NeuralAmpModelerCore) - MIT

## License

GNU General Public License v3.0 (GPL-3.0)

This core library is part of the October Production Co. monorepo. Licensed under GPL-3.0 to ensure compatibility with JUCE (GPL-3.0) requirements.
