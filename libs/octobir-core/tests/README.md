# OctobIR Core Library Tests

Unit tests for the `octobir-core` library. These tests focus on testing OUR logic, not third-party libraries (WDL, dr_wav, etc.).

## Running Tests

```bash
# From repository root
make test
```

## Test Coverage

### IRProcessorTests.cpp
Tests for IRProcessor public interface and behavior:
- Parameter clamping (blend, thresholds, attack/release times, output gain, trim gains)
- Output gain dB-to-linear conversion and application
- Initial state and dynamic mode state transitions
- Passthrough when no IRs are loaded

### IRProcessorLogicTests.cpp
Blend gain calculation logic:
- Equal-power crossfade math (sqrt-based)
- Power sum verification across full blend range

### IRLoaderTests.cpp
IRLoader logic:
- Initial state verification
- Error handling for invalid/missing files
- Compensation gain calculation (-17dB)

### DynamicModeTests.cpp
Dynamic mode parameter and state management:
- Dynamic blend calculation across threshold/range/knee settings
- Detection mode switching (peak/RMS)
- Enable/disable state transitions

### DynamicModeAudioTests.cpp
End-to-end dynamic mode audio processing:
- Blend response to varying input levels
- Attack/release envelope behavior

### SidechainTests.cpp
Sidechain parameter and routing:
- Sidechain enable/disable state
- Sidechain routing configuration

### SidechainAudioTests.cpp
End-to-end sidechain audio processing:
- Envelope detection from sidechain input
- Blend driven by external signal

### ComponentTests.cpp
Component-level integration tests:
- IR loading and processing through full signal chain
- Dual IR blending behavior

### StereoComponentTests.cpp
Stereo-specific component tests:
- Stereo IR loading and channel handling
- Stereo processing modes

### LatencyCompensationTests.cpp
Delay alignment across IR slots:
- Latency reporting accuracy
- Compensation when IRs have different latencies

### ClearIRTests.cpp
IR slot clearing:
- Clear individual slots while other remains loaded
- State after clearing

### ResetTests.cpp
Processor reset behavior:
- State cleanup on reset
- Re-initialization after reset

### TrimGainTests.cpp
Per-IR trim gain:
- dB-to-linear conversion
- Trim applied independently to each IR slot

### BlendExtremeTests.cpp
Edge cases at blend boundaries:
- Behavior at blend = 0.0 and blend = 1.0
- Single IR loaded with various blend positions

### SampleRateChangeTests.cpp
Sample rate transitions:
- IR resampling on rate change
- State preservation across rate changes

## What's NOT Tested

Following best practices, we do NOT test:
- **WDL ConvolutionEngine**: Third-party library, assumed correct
- **WDL Resampler**: Third-party library, assumed correct
- **dr_wav file parsing**: Third-party library, assumed correct
- **FFT correctness**: Third-party library, assumed correct

## Adding New Tests

When adding new logic to the core library:

1. Test parameter validation/clamping
2. Test mathematical calculations (gain, blend, etc.)
3. Test state transitions
4. Test error handling paths

Do NOT test implementation details (private methods). Test PUBLIC behavior only.
