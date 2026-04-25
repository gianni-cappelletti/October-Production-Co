# OctoBASS Core Library Tests

Unit tests for the `octobass-core` library. These tests focus on testing OUR logic, not third-party libraries (NeuralAmpModelerCore, Eigen, etc.).

## Running Tests

```bash
# From repository root
make test-octobass
```

## Test Coverage

### BassProcessorTests.cpp
BassProcessor parameter defaults, clamping, and signal routing:
- Initial state and default values
- Parameter clamping (crossover frequency, squash, gain, mix)
- IR loading/clearing and latency reporting
- Dry/wet mixing behavior
- Solo mode isolation (low/high band)
- Reset clears all state

### BassProcessorAudioTests.cpp
End-to-end audio processing with real bass track files:
- Non-silent, non-clipping output verification
- Magnitude preservation through signal chain
- IR convolution effect on output
- Squash/compression with static makeup gain (no overshoot)
- All four compression modes produce valid output

### CompressorTests.cpp
Compressor parameter and state management:
- Initial defaults and silence passthrough
- Squash at zero produces no change
- Mode switching resets state; redundant set does not
- Block boundary continuity
- Multiple sample rate support

### CompressorAudioTests.cpp
Compressor audio characteristics:
- Peak reduction at full squash
- Mode differentiation (VCA/FET/Opto/Bus produce different results)
- Gain reduction scales with squash amount
- Stability at extreme settings
- No overshoot on transient reentry

### CrossoverTests.cpp
LR4 crossover filter behavior:
- All-pass magnitude-flat reconstruction (low + high = input)
- Low band passes below crossover, high band passes above
- Frequency clamping to valid range
- Reset clears filter state
- Multiple frequencies and sample rates

### GraphicEQTests.cpp
24-band graphic EQ:
- Unity pass-through with all bands flat
- Single and multiple band boost/cut
- Out-of-band rejection (frequency selectivity)
- Proportional Q: narrow at high gain, wide at low gain
- Gain clamping and invalid band index handling
- Magnitude response stability at DC

### NamProcessorTests.cpp
Neural Amp Modeler integration:
- WaveNet model loading without error
- Graceful failure on missing files
- Loaded model produces non-silent output
- Clean bypass when no model is loaded
- Clear model resets state

### NoiseGateTests.cpp
Noise gate threshold and envelope behavior:
- Disabled by default, enabled when threshold set above disable point
- Passes signal when key above threshold, attenuates when below
- Opens instantly on transient
- Hysteresis prevents chattering (6 dB gap)
- Threshold clamped to valid range
- Reset clears state, in-place processing works

## What's NOT Tested

Following best practices, we do NOT test:
- **NeuralAmpModelerCore**: Third-party library, assumed correct
- **Eigen**: Third-party library, assumed correct
- **nlohmann/json**: Third-party library, assumed correct
- **octobir-core internals**: Tested in its own test suite

## Adding New Tests

When adding new logic to the core library:

1. Test parameter validation/clamping
2. Test mathematical calculations (gain, compression ratios, etc.)
3. Test state transitions
4. Test error handling paths

Do NOT test implementation details (private methods). Test PUBLIC behavior only.
