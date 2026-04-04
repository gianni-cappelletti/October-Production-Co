#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <vector>

// DR_WAV_IMPLEMENTATION is compiled into octobir-core via IRLoader.cpp.
// Include the header here without redefining the macro to use those symbols.
#include "dr_wav.h"
#include "opc-vcv-ir.hpp"

static const std::string kIrAPath = std::string(TEST_DATA_DIR) + "/INPUT_ir_a.wav";
static const std::string kIrBPath = std::string(TEST_DATA_DIR) + "/INPUT_ir_b.wav";
static const std::string kIrStereoPath = std::string(TEST_DATA_DIR) + "/INPUT_ir_stereo.wav";
static const std::string kDryPath = std::string(TEST_DATA_DIR) + "/INPUT_amp_output_no_ir.wav";

static constexpr float kSampleRate = 44100.f;
// Large enough to exceed any realistic convolution latency
static constexpr size_t kLatencyFlushFrames = 16384;

static std::vector<float> loadWavMono(const std::string& path, unsigned int& outSampleRate)
{
  drwav_uint32 channels = 0;
  drwav_uint32 sr = 0;
  drwav_uint64 totalFrames = 0;

  float* raw =
      drwav_open_file_and_read_pcm_frames_f32(path.c_str(), &channels, &sr, &totalFrames, nullptr);
  if (raw == nullptr)
    return {};

  outSampleRate = sr;
  std::vector<float> mono(static_cast<size_t>(totalFrames));
  for (drwav_uint64 i = 0; i < totalFrames; ++i)
  {
    float sum = 0.0f;
    for (drwav_uint32 ch = 0; ch < channels; ++ch)
      sum += raw[i * channels + ch];
    mono[static_cast<size_t>(i)] = sum / static_cast<float>(channels);
  }
  drwav_free(raw, nullptr);
  return mono;
}

static double pearsonCorrelation(const std::vector<float>& a, const std::vector<float>& b)
{
  const size_t n = std::min(a.size(), b.size());
  double meanA = 0.0, meanB = 0.0;
  for (size_t i = 0; i < n; ++i)
  {
    meanA += a[i];
    meanB += b[i];
  }
  meanA /= static_cast<double>(n);
  meanB /= static_cast<double>(n);

  double num = 0.0, varA = 0.0, varB = 0.0;
  for (size_t i = 0; i < n; ++i)
  {
    const double da = a[i] - meanA;
    const double db = b[i] - meanB;
    num += da * db;
    varA += da * da;
    varB += db * db;
  }
  const double denom = std::sqrt(varA * varB);
  return (denom > 0.0) ? num / denom : 0.0;
}

struct StereoOutput
{
  std::vector<float> L;
  std::vector<float> R;
};

// Drives the module with mono input (left channel), flushing latency first.
static StereoOutput processMonoInput(OpcVcvIr& module, const std::vector<float>& input)
{
  const int inL = static_cast<int>(OpcVcvIr::InputId::AudioInL);
  const int outL = static_cast<int>(OpcVcvIr::OutputId::OutputL);
  const int outR = static_cast<int>(OpcVcvIr::OutputId::OutputR);

  module.inputs[static_cast<size_t>(inL)].connected = true;
  module.inputs[static_cast<size_t>(inL)].voltage = 0.f;

  ProcessArgs args{kSampleRate, 1.f / kSampleRate};

  for (size_t i = 0; i < kLatencyFlushFrames; ++i)
    module.process(args);

  StereoOutput out;
  out.L.reserve(input.size());
  out.R.reserve(input.size());

  for (float sample : input)
  {
    module.inputs[static_cast<size_t>(inL)].voltage = sample;
    module.process(args);
    out.L.push_back(module.outputs[static_cast<size_t>(outL)].getVoltage());
    out.R.push_back(module.outputs[static_cast<size_t>(outR)].getVoltage());
  }

  return out;
}

// Drives the module with right channel only, flushing latency first.
static StereoOutput processRightOnlyInput(OpcVcvIr& module, const std::vector<float>& input)
{
  const int inR = static_cast<int>(OpcVcvIr::InputId::AudioInR);
  const int outL = static_cast<int>(OpcVcvIr::OutputId::OutputL);
  const int outR = static_cast<int>(OpcVcvIr::OutputId::OutputR);

  module.inputs[static_cast<size_t>(inR)].connected = true;
  module.inputs[static_cast<size_t>(inR)].voltage = 0.f;

  ProcessArgs args{kSampleRate, 1.f / kSampleRate};

  for (size_t i = 0; i < kLatencyFlushFrames; ++i)
    module.process(args);

  StereoOutput out;
  out.L.reserve(input.size());
  out.R.reserve(input.size());

  for (float sample : input)
  {
    module.inputs[static_cast<size_t>(inR)].voltage = sample;
    module.process(args);
    out.L.push_back(module.outputs[static_cast<size_t>(outL)].getVoltage());
    out.R.push_back(module.outputs[static_cast<size_t>(outR)].getVoltage());
  }

  return out;
}

// Drives the module with true stereo input (both channels), flushing latency first.
static StereoOutput processStereoChannels(OpcVcvIr& module, const std::vector<float>& inputL,
                                          const std::vector<float>& inputR)
{
  const int inL = static_cast<int>(OpcVcvIr::InputId::AudioInL);
  const int inR = static_cast<int>(OpcVcvIr::InputId::AudioInR);
  const int outL = static_cast<int>(OpcVcvIr::OutputId::OutputL);
  const int outR = static_cast<int>(OpcVcvIr::OutputId::OutputR);

  module.inputs[static_cast<size_t>(inL)].connected = true;
  module.inputs[static_cast<size_t>(inR)].connected = true;

  ProcessArgs args{kSampleRate, 1.f / kSampleRate};

  for (size_t i = 0; i < kLatencyFlushFrames; ++i)
    module.process(args);

  const size_t n = std::min(inputL.size(), inputR.size());
  StereoOutput out;
  out.L.reserve(n);
  out.R.reserve(n);

  for (size_t i = 0; i < n; ++i)
  {
    module.inputs[static_cast<size_t>(inL)].voltage = inputL[i];
    module.inputs[static_cast<size_t>(inR)].voltage = inputR[i];
    module.process(args);
    out.L.push_back(module.outputs[static_cast<size_t>(outL)].getVoltage());
    out.R.push_back(module.outputs[static_cast<size_t>(outR)].getVoltage());
  }

  return out;
}

// Drives the module with mono left + sidechain input, flushing latency first.
// Requires dynamic mode and sidechain to already be enabled on the module.
static StereoOutput processMonoWithSidechain(OpcVcvIr& module, const std::vector<float>& input,
                                             const std::vector<float>& sidechain)
{
  const int inL = static_cast<int>(OpcVcvIr::InputId::AudioInL);
  const int scIn = static_cast<int>(OpcVcvIr::InputId::SidechainIn);
  const int outL = static_cast<int>(OpcVcvIr::OutputId::OutputL);
  const int outR = static_cast<int>(OpcVcvIr::OutputId::OutputR);

  module.inputs[static_cast<size_t>(inL)].connected = true;
  module.inputs[static_cast<size_t>(scIn)].connected = true;

  ProcessArgs args{kSampleRate, 1.f / kSampleRate};

  for (size_t i = 0; i < kLatencyFlushFrames; ++i)
    module.process(args);

  const size_t n = std::min(input.size(), sidechain.size());
  StereoOutput out;
  out.L.reserve(n);
  out.R.reserve(n);

  for (size_t i = 0; i < n; ++i)
  {
    module.inputs[static_cast<size_t>(inL)].voltage = input[i];
    module.inputs[static_cast<size_t>(scIn)].voltage = sidechain[i];
    module.process(args);
    out.L.push_back(module.outputs[static_cast<size_t>(outL)].getVoltage());
    out.R.push_back(module.outputs[static_cast<size_t>(outR)].getVoltage());
  }

  return out;
}

// Drives the module with stereo + sidechain input, flushing latency first.
// Requires dynamic mode and sidechain to already be enabled on the module.
static StereoOutput processStereoWithSidechain(OpcVcvIr& module, const std::vector<float>& inputL,
                                               const std::vector<float>& inputR,
                                               const std::vector<float>& sidechain)
{
  const int inL = static_cast<int>(OpcVcvIr::InputId::AudioInL);
  const int inR = static_cast<int>(OpcVcvIr::InputId::AudioInR);
  const int scIn = static_cast<int>(OpcVcvIr::InputId::SidechainIn);
  const int outL = static_cast<int>(OpcVcvIr::OutputId::OutputL);
  const int outR = static_cast<int>(OpcVcvIr::OutputId::OutputR);

  module.inputs[static_cast<size_t>(inL)].connected = true;
  module.inputs[static_cast<size_t>(inR)].connected = true;
  module.inputs[static_cast<size_t>(scIn)].connected = true;

  ProcessArgs args{kSampleRate, 1.f / kSampleRate};

  for (size_t i = 0; i < kLatencyFlushFrames; ++i)
    module.process(args);

  const size_t n = std::min({inputL.size(), inputR.size(), sidechain.size()});
  StereoOutput out;
  out.L.reserve(n);
  out.R.reserve(n);

  for (size_t i = 0; i < n; ++i)
  {
    module.inputs[static_cast<size_t>(inL)].voltage = inputL[i];
    module.inputs[static_cast<size_t>(inR)].voltage = inputR[i];
    module.inputs[static_cast<size_t>(scIn)].voltage = sidechain[i];
    module.process(args);
    out.L.push_back(module.outputs[static_cast<size_t>(outL)].getVoltage());
    out.R.push_back(module.outputs[static_cast<size_t>(outR)].getVoltage());
  }

  return out;
}

// ---------------------------------------------------------------------------

class VcvAudioTest : public ::testing::Test
{
 protected:
  void SetUp() override
  {
    unsigned int sr = 0;
    dryInput_ = loadWavMono(kDryPath, sr);
    ASSERT_FALSE(dryInput_.empty()) << "Failed to load dry input: " << kDryPath;
    ASSERT_EQ(sr, static_cast<unsigned int>(kSampleRate));
  }

  std::vector<float> dryInput_;
};

// ---------------------------------------------------------------------------
// Non-silent output
// ---------------------------------------------------------------------------

TEST_F(VcvAudioTest, NonSilentOutput_IrALoaded)
{
  OpcVcvIr module;
  SampleRateChangeEvent sr{kSampleRate};
  module.onSampleRateChange(sr);
  module.loadIR(kIrAPath);
  module.params[static_cast<int>(OpcVcvIr::ParamId::IrAEnableParam)].setValue(1.f);

  auto out = processMonoInput(module, dryInput_);

  float peak = 0.0f;
  for (float s : out.L)
    peak = std::max(peak, std::abs(s));
  EXPECT_GT(peak, 1e-6f) << "Output is silent despite IR A being loaded";
}

TEST_F(VcvAudioTest, NonSilentOutput_IrBLoaded)
{
  OpcVcvIr module;
  SampleRateChangeEvent sr{kSampleRate};
  module.onSampleRateChange(sr);
  module.loadIR2(kIrBPath);
  module.params[static_cast<int>(OpcVcvIr::ParamId::IrBEnableParam)].setValue(1.f);
  // Full-wet blend toward IR B: blend = 1.0
  module.params[static_cast<int>(OpcVcvIr::ParamId::BlendParam)].setValue(1.f);

  auto out = processMonoInput(module, dryInput_);

  float peak = 0.0f;
  for (float s : out.L)
    peak = std::max(peak, std::abs(s));
  EXPECT_GT(peak, 1e-6f) << "Output is silent despite IR B being loaded";
}

// ---------------------------------------------------------------------------
// Mono-to-stereo: both channels non-silent and differing
// ---------------------------------------------------------------------------

TEST_F(VcvAudioTest, MonoToStereo_BothChannelsNonSilent)
{
  OpcVcvIr module;
  SampleRateChangeEvent sr{kSampleRate};
  module.onSampleRateChange(sr);
  module.loadIR(kIrAPath);
  module.params[static_cast<int>(OpcVcvIr::ParamId::IrAEnableParam)].setValue(1.f);

  auto out = processMonoInput(module, dryInput_);

  float peakL = 0.0f, peakR = 0.0f;
  for (size_t i = 0; i < out.L.size(); ++i)
  {
    peakL = std::max(peakL, std::abs(out.L[i]));
    peakR = std::max(peakR, std::abs(out.R[i]));
  }
  EXPECT_GT(peakL, 1e-6f) << "Left channel silent in mono-to-stereo mode";
  EXPECT_GT(peakR, 1e-6f) << "Right channel silent in mono-to-stereo mode";
}

TEST_F(VcvAudioTest, MonoToStereo_ChannelsAreDifferent)
{
  OpcVcvIr module;
  SampleRateChangeEvent sr{kSampleRate};
  module.onSampleRateChange(sr);
  module.loadIR(kIrStereoPath);
  module.params[static_cast<int>(OpcVcvIr::ParamId::IrAEnableParam)].setValue(1.f);

  ProcessArgs flushArgs{kSampleRate, 1.f / kSampleRate};
  module.process(flushArgs);

  ASSERT_GE(module.irProcessor_.getNumIR1Channels(), 2u)
      << "Stereo IR fixture failed to load or was decoded as mono";

  auto out = processMonoInput(module, dryInput_);
  const double r = pearsonCorrelation(out.L, out.R);
  EXPECT_LT(r, 0.9999) << "L and R are identical (r=" << r
                       << ") — processMonoToStereo may be mirroring instead of spreading";
}

// ---------------------------------------------------------------------------
// CV modulation
// ---------------------------------------------------------------------------

TEST_F(VcvAudioTest, ThresholdCv_ClampedToMax)
{
  // +5V at threshold CV with knob at -30 dB -> effective threshold = 0 dB (clamped)
  OpcVcvIr module;
  SampleRateChangeEvent sr{kSampleRate};
  module.onSampleRateChange(sr);

  module.params[static_cast<int>(OpcVcvIr::ParamId::ThresholdParam)].setValue(-30.f);
  const int threshCvIn = static_cast<int>(OpcVcvIr::InputId::ThresholdCvIn);
  module.inputs[static_cast<size_t>(threshCvIn)].connected = true;
  module.inputs[static_cast<size_t>(threshCvIn)].voltage = 5.0f;

  ProcessArgs args{kSampleRate, 1.f / kSampleRate};
  module.process(args);

  EXPECT_NEAR(module.irProcessor_.getThreshold(), 0.f, 0.01f)
      << "Threshold CV at +5V should clamp to 0 dB";
}

TEST_F(VcvAudioTest, BlendCv_ShiftsBlendToMax)
{
  // +5V at blend CV with knob at 0.0 -> effective blend = 1.0 (5V * 0.2)
  OpcVcvIr module;
  SampleRateChangeEvent sr{kSampleRate};
  module.onSampleRateChange(sr);

  module.params[static_cast<int>(OpcVcvIr::ParamId::BlendParam)].setValue(0.f);
  const int blendCvIn = static_cast<int>(OpcVcvIr::InputId::BlendCvIn);
  module.inputs[static_cast<size_t>(blendCvIn)].connected = true;
  module.inputs[static_cast<size_t>(blendCvIn)].voltage = 5.0f;

  ProcessArgs args{kSampleRate, 1.f / kSampleRate};
  module.process(args);

  EXPECT_NEAR(module.irProcessor_.getBlend(), 1.f, 0.01f)
      << "Blend CV at +5V should set blend to 1.0";
}

TEST_F(VcvAudioTest, ThresholdCv_MidRangeScaling)
{
  // +2.5V at threshold CV with knob at -30 dB -> -30 + 15 = -15 dB
  OpcVcvIr module;
  SampleRateChangeEvent sr{kSampleRate};
  module.onSampleRateChange(sr);

  module.params[static_cast<int>(OpcVcvIr::ParamId::ThresholdParam)].setValue(-30.f);
  const int threshCvIn = static_cast<int>(OpcVcvIr::InputId::ThresholdCvIn);
  module.inputs[static_cast<size_t>(threshCvIn)].connected = true;
  module.inputs[static_cast<size_t>(threshCvIn)].voltage = 2.5f;

  ProcessArgs args{kSampleRate, 1.f / kSampleRate};
  module.process(args);

  EXPECT_NEAR(module.irProcessor_.getThreshold(), -15.f, 0.01f)
      << "Threshold CV at +2.5V should add 15 dB to knob value";
}

TEST_F(VcvAudioTest, ThresholdCv_AddsToNonZeroBase)
{
  // -2V at threshold CV with knob at -10 dB -> -10 + (-12) = -22 dB
  OpcVcvIr module;
  SampleRateChangeEvent sr{kSampleRate};
  module.onSampleRateChange(sr);

  module.params[static_cast<int>(OpcVcvIr::ParamId::ThresholdParam)].setValue(-10.f);
  const int threshCvIn = static_cast<int>(OpcVcvIr::InputId::ThresholdCvIn);
  module.inputs[static_cast<size_t>(threshCvIn)].connected = true;
  module.inputs[static_cast<size_t>(threshCvIn)].voltage = -2.0f;

  ProcessArgs args{kSampleRate, 1.f / kSampleRate};
  module.process(args);

  EXPECT_NEAR(module.irProcessor_.getThreshold(), -22.f, 0.01f)
      << "Threshold CV at -2V should subtract 12 dB from knob value of -10";
}

TEST_F(VcvAudioTest, BlendCv_MidRangeScaling)
{
  // +2.5V at blend CV with knob at 0.0 -> 0.0 + 0.5 = 0.5
  OpcVcvIr module;
  SampleRateChangeEvent sr{kSampleRate};
  module.onSampleRateChange(sr);

  module.params[static_cast<int>(OpcVcvIr::ParamId::BlendParam)].setValue(0.f);
  const int blendCvIn = static_cast<int>(OpcVcvIr::InputId::BlendCvIn);
  module.inputs[static_cast<size_t>(blendCvIn)].connected = true;
  module.inputs[static_cast<size_t>(blendCvIn)].voltage = 2.5f;

  ProcessArgs args{kSampleRate, 1.f / kSampleRate};
  module.process(args);

  EXPECT_NEAR(module.irProcessor_.getBlend(), 0.5f, 0.01f)
      << "Blend CV at +2.5V should set blend to 0.5";
}

TEST_F(VcvAudioTest, BlendCv_AddsToNonZeroBase)
{
  // +2V at blend CV with knob at -0.5 -> -0.5 + 0.4 = -0.1
  OpcVcvIr module;
  SampleRateChangeEvent sr{kSampleRate};
  module.onSampleRateChange(sr);

  module.params[static_cast<int>(OpcVcvIr::ParamId::BlendParam)].setValue(-0.5f);
  const int blendCvIn = static_cast<int>(OpcVcvIr::InputId::BlendCvIn);
  module.inputs[static_cast<size_t>(blendCvIn)].connected = true;
  module.inputs[static_cast<size_t>(blendCvIn)].voltage = 2.0f;

  ProcessArgs args{kSampleRate, 1.f / kSampleRate};
  module.process(args);

  EXPECT_NEAR(module.irProcessor_.getBlend(), -0.1f, 0.01f)
      << "Blend CV at +2V should add 0.4 to knob value of -0.5";
}

TEST_F(VcvAudioTest, BlendCv_ClampWhenBaseAndCvExceedRange)
{
  // +5V at blend CV with knob at 0.5 -> 0.5 + 1.0 = 1.5, clamped to 1.0
  OpcVcvIr module;
  SampleRateChangeEvent sr{kSampleRate};
  module.onSampleRateChange(sr);

  module.params[static_cast<int>(OpcVcvIr::ParamId::BlendParam)].setValue(0.5f);
  const int blendCvIn = static_cast<int>(OpcVcvIr::InputId::BlendCvIn);
  module.inputs[static_cast<size_t>(blendCvIn)].connected = true;
  module.inputs[static_cast<size_t>(blendCvIn)].voltage = 5.0f;

  ProcessArgs args{kSampleRate, 1.f / kSampleRate};
  module.process(args);

  EXPECT_NEAR(module.irProcessor_.getBlend(), 1.0f, 0.01f)
      << "Blend CV should clamp to 1.0 when base + CV exceeds range";
}

TEST_F(VcvAudioTest, DynamicsEnableGate_HighOverridesParamOff)
{
  OpcVcvIr module;
  SampleRateChangeEvent sr{kSampleRate};
  module.onSampleRateChange(sr);

  module.params[static_cast<int>(OpcVcvIr::ParamId::DynamicModeParam)].setValue(0.f);
  const int dynGateIn = static_cast<int>(OpcVcvIr::InputId::DynamicsEnableCvIn);
  module.inputs[static_cast<size_t>(dynGateIn)].connected = true;
  module.inputs[static_cast<size_t>(dynGateIn)].voltage = 5.0f;

  ProcessArgs args{kSampleRate, 1.f / kSampleRate};
  module.process(args);

  EXPECT_TRUE(module.irProcessor_.getDynamicModeEnabled())
      << "Gate at +5V should enable dynamic mode when param is OFF";
}

TEST_F(VcvAudioTest, DynamicsEnableGate_LowOverridesParamOn)
{
  OpcVcvIr module;
  SampleRateChangeEvent sr{kSampleRate};
  module.onSampleRateChange(sr);

  module.params[static_cast<int>(OpcVcvIr::ParamId::DynamicModeParam)].setValue(1.f);
  const int dynGateIn = static_cast<int>(OpcVcvIr::InputId::DynamicsEnableCvIn);
  module.inputs[static_cast<size_t>(dynGateIn)].connected = true;
  module.inputs[static_cast<size_t>(dynGateIn)].voltage = 0.0f;

  ProcessArgs args{kSampleRate, 1.f / kSampleRate};
  module.process(args);

  EXPECT_FALSE(module.irProcessor_.getDynamicModeEnabled())
      << "Gate at 0V should disable dynamic mode even when param is ON";
}

TEST_F(VcvAudioTest, DynamicsEnableGate_JustBelowThreshold)
{
  OpcVcvIr module;
  SampleRateChangeEvent sr{kSampleRate};
  module.onSampleRateChange(sr);

  module.params[static_cast<int>(OpcVcvIr::ParamId::DynamicModeParam)].setValue(0.f);
  const int dynGateIn = static_cast<int>(OpcVcvIr::InputId::DynamicsEnableCvIn);
  module.inputs[static_cast<size_t>(dynGateIn)].connected = true;
  module.inputs[static_cast<size_t>(dynGateIn)].voltage = 0.9f;

  ProcessArgs args{kSampleRate, 1.f / kSampleRate};
  module.process(args);

  EXPECT_FALSE(module.irProcessor_.getDynamicModeEnabled())
      << "Gate at 0.9V (below 1.0V threshold) should not enable dynamic mode";
}

TEST_F(VcvAudioTest, DynamicsEnableGate_JustAboveThreshold)
{
  OpcVcvIr module;
  SampleRateChangeEvent sr{kSampleRate};
  module.onSampleRateChange(sr);

  module.params[static_cast<int>(OpcVcvIr::ParamId::DynamicModeParam)].setValue(0.f);
  const int dynGateIn = static_cast<int>(OpcVcvIr::InputId::DynamicsEnableCvIn);
  module.inputs[static_cast<size_t>(dynGateIn)].connected = true;
  module.inputs[static_cast<size_t>(dynGateIn)].voltage = 1.1f;

  ProcessArgs args{kSampleRate, 1.f / kSampleRate};
  module.process(args);

  EXPECT_TRUE(module.irProcessor_.getDynamicModeEnabled())
      << "Gate at 1.1V (above 1.0V threshold) should enable dynamic mode";
}

TEST_F(VcvAudioTest, DynamicsEnableGate_DisconnectedFallsBackToParam)
{
  OpcVcvIr module;
  SampleRateChangeEvent sr{kSampleRate};
  module.onSampleRateChange(sr);

  module.params[static_cast<int>(OpcVcvIr::ParamId::DynamicModeParam)].setValue(1.f);
  const int dynGateIn = static_cast<int>(OpcVcvIr::InputId::DynamicsEnableCvIn);
  module.inputs[static_cast<size_t>(dynGateIn)].connected = false;

  ProcessArgs args{kSampleRate, 1.f / kSampleRate};
  module.process(args);

  EXPECT_TRUE(module.irProcessor_.getDynamicModeEnabled())
      << "With gate disconnected, dynamic mode should follow the param";
}

// ---------------------------------------------------------------------------
// Swap symmetry
// ---------------------------------------------------------------------------

TEST_F(VcvAudioTest, SwapSymmetry_AudioOutput)
{
  // At blend=0.0 (equal-power center), output is slot-order invariant.
  // Pearson r between AB and BA orderings must be > 0.9999.

  StereoOutput outAB;
  {
    OpcVcvIr module;
    SampleRateChangeEvent sr{kSampleRate};
    module.onSampleRateChange(sr);
    module.loadIR(kIrAPath);
    module.loadIR2(kIrBPath);
    module.params[static_cast<int>(OpcVcvIr::ParamId::IrAEnableParam)].setValue(1.f);
    module.params[static_cast<int>(OpcVcvIr::ParamId::IrBEnableParam)].setValue(1.f);
    module.params[static_cast<int>(OpcVcvIr::ParamId::BlendParam)].setValue(0.f);
    outAB = processMonoInput(module, dryInput_);
    float peak = 0.0f;
    for (float s : outAB.L)
      peak = std::max(peak, std::abs(s));
    ASSERT_GT(peak, 1e-6f) << "Pre-swap output is silent";
  }

  StereoOutput outBA;
  {
    OpcVcvIr module;
    SampleRateChangeEvent sr{kSampleRate};
    module.onSampleRateChange(sr);
    module.loadIR(kIrBPath);
    module.loadIR2(kIrAPath);
    module.params[static_cast<int>(OpcVcvIr::ParamId::IrAEnableParam)].setValue(1.f);
    module.params[static_cast<int>(OpcVcvIr::ParamId::IrBEnableParam)].setValue(1.f);
    module.params[static_cast<int>(OpcVcvIr::ParamId::BlendParam)].setValue(0.f);
    outBA = processMonoInput(module, dryInput_);
  }

  const double rL = pearsonCorrelation(outAB.L, outBA.L);
  const double rR = pearsonCorrelation(outAB.R, outBA.R);
  EXPECT_GT(rL, 0.9999) << "Swap symmetry failed on L: r=" << rL;
  EXPECT_GT(rR, 0.9999) << "Swap symmetry failed on R: r=" << rR;
}

// ---------------------------------------------------------------------------
// Right-channel-only mono path
// ---------------------------------------------------------------------------

TEST_F(VcvAudioTest, RightOnlyMono_NonSilentOutput_IrALoaded)
{
  OpcVcvIr module;
  SampleRateChangeEvent sr{kSampleRate};
  module.onSampleRateChange(sr);
  module.loadIR(kIrAPath);
  module.params[static_cast<int>(OpcVcvIr::ParamId::IrAEnableParam)].setValue(1.f);

  auto out = processRightOnlyInput(module, dryInput_);

  float peak = 0.0f;
  for (float s : out.L)
    peak = std::max(peak, std::abs(s));
  EXPECT_GT(peak, 1e-6f) << "Output is silent when only right channel is connected";
}

// ---------------------------------------------------------------------------
// True stereo path (both channels)
// ---------------------------------------------------------------------------

TEST_F(VcvAudioTest, TrueStereo_NonSilentOutput_IrALoaded)
{
  OpcVcvIr module;
  SampleRateChangeEvent sr{kSampleRate};
  module.onSampleRateChange(sr);
  module.loadIR(kIrAPath);
  module.params[static_cast<int>(OpcVcvIr::ParamId::IrAEnableParam)].setValue(1.f);

  auto out = processStereoChannels(module, dryInput_, dryInput_);

  float peakL = 0.0f, peakR = 0.0f;
  for (size_t i = 0; i < out.L.size(); ++i)
  {
    peakL = std::max(peakL, std::abs(out.L[i]));
    peakR = std::max(peakR, std::abs(out.R[i]));
  }
  EXPECT_GT(peakL, 1e-6f) << "Left output is silent in true stereo mode";
  EXPECT_GT(peakR, 1e-6f) << "Right output is silent in true stereo mode";
}

// ---------------------------------------------------------------------------
// Sidechain dispatch paths
// ---------------------------------------------------------------------------

TEST_F(VcvAudioTest, SidechainMono_NonSilentOutput)
{
  OpcVcvIr module;
  SampleRateChangeEvent sr{kSampleRate};
  module.onSampleRateChange(sr);
  module.loadIR(kIrAPath);
  module.params[static_cast<int>(OpcVcvIr::ParamId::IrAEnableParam)].setValue(1.f);
  module.params[static_cast<int>(OpcVcvIr::ParamId::DynamicModeParam)].setValue(1.f);
  module.params[static_cast<int>(OpcVcvIr::ParamId::SidechainEnableParam)].setValue(1.f);
  // Set threshold to max so dynamics don't attenuate the signal
  module.params[static_cast<int>(OpcVcvIr::ParamId::ThresholdParam)].setValue(0.f);

  auto out = processMonoWithSidechain(module, dryInput_, dryInput_);

  float peak = 0.0f;
  for (float s : out.L)
    peak = std::max(peak, std::abs(s));
  EXPECT_GT(peak, 1e-6f) << "Output is silent in sidechain mono path";
}

TEST_F(VcvAudioTest, SidechainStereo_NonSilentOutput)
{
  OpcVcvIr module;
  SampleRateChangeEvent sr{kSampleRate};
  module.onSampleRateChange(sr);
  module.loadIR(kIrAPath);
  module.params[static_cast<int>(OpcVcvIr::ParamId::IrAEnableParam)].setValue(1.f);
  module.params[static_cast<int>(OpcVcvIr::ParamId::DynamicModeParam)].setValue(1.f);
  module.params[static_cast<int>(OpcVcvIr::ParamId::SidechainEnableParam)].setValue(1.f);
  module.params[static_cast<int>(OpcVcvIr::ParamId::ThresholdParam)].setValue(0.f);

  auto out = processStereoWithSidechain(module, dryInput_, dryInput_, dryInput_);

  float peakL = 0.0f, peakR = 0.0f;
  for (size_t i = 0; i < out.L.size(); ++i)
  {
    peakL = std::max(peakL, std::abs(out.L[i]));
    peakR = std::max(peakR, std::abs(out.R[i]));
  }
  EXPECT_GT(peakL, 1e-6f) << "Left output is silent in sidechain stereo path";
  EXPECT_GT(peakR, 1e-6f) << "Right output is silent in sidechain stereo path";
}

// ---------------------------------------------------------------------------
// Dynamic mode without sidechain
// ---------------------------------------------------------------------------

TEST_F(VcvAudioTest, DynamicModeNoSidechain_NonSilentOutput)
{
  OpcVcvIr module;
  SampleRateChangeEvent sr{kSampleRate};
  module.onSampleRateChange(sr);
  module.loadIR(kIrAPath);
  module.params[static_cast<int>(OpcVcvIr::ParamId::IrAEnableParam)].setValue(1.f);
  module.params[static_cast<int>(OpcVcvIr::ParamId::DynamicModeParam)].setValue(1.f);
  module.params[static_cast<int>(OpcVcvIr::ParamId::SidechainEnableParam)].setValue(0.f);
  module.params[static_cast<int>(OpcVcvIr::ParamId::ThresholdParam)].setValue(0.f);

  auto out = processMonoInput(module, dryInput_);

  float peak = 0.0f;
  for (float s : out.L)
    peak = std::max(peak, std::abs(s));
  EXPECT_GT(peak, 1e-6f) << "Output is silent in dynamic mode without sidechain";
}

// ---------------------------------------------------------------------------
// Sidechain button / connection interaction with dynamic mode
// ---------------------------------------------------------------------------

TEST_F(VcvAudioTest, DynamicMode_ButtonOff_InputDrivesBlend)
{
  OpcVcvIr module;
  SampleRateChangeEvent sr{kSampleRate};
  module.onSampleRateChange(sr);
  module.loadIR(kIrAPath);
  module.loadIR2(kIrBPath);
  module.params[static_cast<int>(OpcVcvIr::ParamId::IrAEnableParam)].setValue(1.f);
  module.params[static_cast<int>(OpcVcvIr::ParamId::IrBEnableParam)].setValue(1.f);
  module.params[static_cast<int>(OpcVcvIr::ParamId::DynamicModeParam)].setValue(1.f);
  module.params[static_cast<int>(OpcVcvIr::ParamId::SidechainEnableParam)].setValue(0.f);
  module.params[static_cast<int>(OpcVcvIr::ParamId::BlendParam)].setValue(0.f);
  module.params[static_cast<int>(OpcVcvIr::ParamId::ThresholdParam)].setValue(-60.f);
  module.params[static_cast<int>(OpcVcvIr::ParamId::RangeDbParam)].setValue(20.f);
  module.params[static_cast<int>(OpcVcvIr::ParamId::KneeWidthDbParam)].setValue(0.f);
  module.params[static_cast<int>(OpcVcvIr::ParamId::AttackTimeParam)].setValue(1.f);
  module.params[static_cast<int>(OpcVcvIr::ParamId::ReleaseTimeParam)].setValue(1.f);

  auto out = processMonoInput(module, dryInput_);

  float blend = module.currentBlend_.load(std::memory_order_relaxed);
  EXPECT_GT(blend, 0.1f)
      << "With sidechain button OFF, loud input should drive dynamic blend away from center";
}

TEST_F(VcvAudioTest, DynamicMode_ButtonOn_SCConnected_SidechainDrivesBlend)
{
  OpcVcvIr module;
  SampleRateChangeEvent sr{kSampleRate};
  module.onSampleRateChange(sr);
  module.loadIR(kIrAPath);
  module.loadIR2(kIrBPath);
  module.params[static_cast<int>(OpcVcvIr::ParamId::IrAEnableParam)].setValue(1.f);
  module.params[static_cast<int>(OpcVcvIr::ParamId::IrBEnableParam)].setValue(1.f);
  module.params[static_cast<int>(OpcVcvIr::ParamId::DynamicModeParam)].setValue(1.f);
  module.params[static_cast<int>(OpcVcvIr::ParamId::SidechainEnableParam)].setValue(1.f);
  module.params[static_cast<int>(OpcVcvIr::ParamId::BlendParam)].setValue(0.f);
  module.params[static_cast<int>(OpcVcvIr::ParamId::ThresholdParam)].setValue(-60.f);
  module.params[static_cast<int>(OpcVcvIr::ParamId::RangeDbParam)].setValue(20.f);
  module.params[static_cast<int>(OpcVcvIr::ParamId::KneeWidthDbParam)].setValue(0.f);
  module.params[static_cast<int>(OpcVcvIr::ParamId::AttackTimeParam)].setValue(1.f);
  module.params[static_cast<int>(OpcVcvIr::ParamId::ReleaseTimeParam)].setValue(1.f);

  // Silent main input, loud sidechain -- blend should move from sidechain signal
  std::vector<float> silence(dryInput_.size(), 0.0f);
  auto out = processMonoWithSidechain(module, silence, dryInput_);

  float blend = module.currentBlend_.load(std::memory_order_relaxed);
  EXPECT_GT(blend, 0.1f)
      << "With sidechain button ON and SC connected, loud sidechain should drive blend";
}

TEST_F(VcvAudioTest, DynamicMode_ButtonOn_SCNotConnected_InputStillDrivesBlend)
{
  OpcVcvIr module;
  SampleRateChangeEvent sr{kSampleRate};
  module.onSampleRateChange(sr);
  module.loadIR(kIrAPath);
  module.loadIR2(kIrBPath);
  module.params[static_cast<int>(OpcVcvIr::ParamId::IrAEnableParam)].setValue(1.f);
  module.params[static_cast<int>(OpcVcvIr::ParamId::IrBEnableParam)].setValue(1.f);
  module.params[static_cast<int>(OpcVcvIr::ParamId::DynamicModeParam)].setValue(1.f);
  module.params[static_cast<int>(OpcVcvIr::ParamId::SidechainEnableParam)].setValue(1.f);
  module.params[static_cast<int>(OpcVcvIr::ParamId::BlendParam)].setValue(0.f);
  module.params[static_cast<int>(OpcVcvIr::ParamId::ThresholdParam)].setValue(-60.f);
  module.params[static_cast<int>(OpcVcvIr::ParamId::RangeDbParam)].setValue(20.f);
  module.params[static_cast<int>(OpcVcvIr::ParamId::KneeWidthDbParam)].setValue(0.f);
  module.params[static_cast<int>(OpcVcvIr::ParamId::AttackTimeParam)].setValue(1.f);
  module.params[static_cast<int>(OpcVcvIr::ParamId::ReleaseTimeParam)].setValue(1.f);

  // Sidechain button ON but nothing connected -- input signal should still drive blend
  auto out = processMonoInput(module, dryInput_);

  EXPECT_FALSE(module.irProcessor_.getSidechainEnabled())
      << "Core should see sidechain as disabled when SC input is not connected";
  float blend = module.currentBlend_.load(std::memory_order_relaxed);
  EXPECT_GT(blend, 0.1f)
      << "With SC button ON but not connected, loud input should still drive dynamic blend";
}

TEST_F(VcvAudioTest, SCConnected_ButtonOff_ConnectionDoesNotActivateSidechain)
{
  OpcVcvIr module;
  SampleRateChangeEvent sr{kSampleRate};
  module.onSampleRateChange(sr);
  module.loadIR(kIrAPath);
  module.loadIR2(kIrBPath);
  module.params[static_cast<int>(OpcVcvIr::ParamId::IrAEnableParam)].setValue(1.f);
  module.params[static_cast<int>(OpcVcvIr::ParamId::IrBEnableParam)].setValue(1.f);
  module.params[static_cast<int>(OpcVcvIr::ParamId::DynamicModeParam)].setValue(1.f);
  module.params[static_cast<int>(OpcVcvIr::ParamId::SidechainEnableParam)].setValue(0.f);
  module.params[static_cast<int>(OpcVcvIr::ParamId::BlendParam)].setValue(0.f);
  module.params[static_cast<int>(OpcVcvIr::ParamId::ThresholdParam)].setValue(-60.f);
  module.params[static_cast<int>(OpcVcvIr::ParamId::RangeDbParam)].setValue(20.f);
  module.params[static_cast<int>(OpcVcvIr::ParamId::KneeWidthDbParam)].setValue(0.f);
  module.params[static_cast<int>(OpcVcvIr::ParamId::AttackTimeParam)].setValue(1.f);
  module.params[static_cast<int>(OpcVcvIr::ParamId::ReleaseTimeParam)].setValue(1.f);

  // SC connected + button OFF: loud main, silent sidechain -- blend driven by main input
  std::vector<float> silence(dryInput_.size(), 0.0f);
  auto out = processMonoWithSidechain(module, dryInput_, silence);

  EXPECT_FALSE(module.irProcessor_.getSidechainEnabled())
      << "Core should see sidechain as disabled when button is OFF, even if SC is connected";
  float blend = module.currentBlend_.load(std::memory_order_relaxed);
  EXPECT_GT(blend, 0.1f)
      << "With button OFF, loud main input should drive blend regardless of SC connection";
}

TEST_F(VcvAudioTest, DynamicModeOff_NoDynamicBlendRegardless)
{
  OpcVcvIr module;
  SampleRateChangeEvent sr{kSampleRate};
  module.onSampleRateChange(sr);
  module.loadIR(kIrAPath);
  module.loadIR2(kIrBPath);
  module.params[static_cast<int>(OpcVcvIr::ParamId::IrAEnableParam)].setValue(1.f);
  module.params[static_cast<int>(OpcVcvIr::ParamId::IrBEnableParam)].setValue(1.f);
  module.params[static_cast<int>(OpcVcvIr::ParamId::DynamicModeParam)].setValue(0.f);
  module.params[static_cast<int>(OpcVcvIr::ParamId::SidechainEnableParam)].setValue(1.f);
  module.params[static_cast<int>(OpcVcvIr::ParamId::BlendParam)].setValue(0.3f);
  module.params[static_cast<int>(OpcVcvIr::ParamId::ThresholdParam)].setValue(-60.f);

  auto out = processMonoWithSidechain(module, dryInput_, dryInput_);

  float blend = module.currentBlend_.load(std::memory_order_relaxed);
  EXPECT_NEAR(blend, 0.3f, 0.05f)
      << "With dynamic mode OFF, blend should stay at static value regardless of sidechain";
}

// ---------------------------------------------------------------------------
// Sample rate change after IR loaded
// ---------------------------------------------------------------------------

TEST_F(VcvAudioTest, SampleRateChange_AfterIRLoaded_StillProducesOutput)
{
  OpcVcvIr module;
  SampleRateChangeEvent sr1{kSampleRate};
  module.onSampleRateChange(sr1);
  module.loadIR(kIrAPath);
  module.params[static_cast<int>(OpcVcvIr::ParamId::IrAEnableParam)].setValue(1.f);

  SampleRateChangeEvent sr2{48000.f};
  module.onSampleRateChange(sr2);

  const int inL = static_cast<int>(OpcVcvIr::InputId::AudioInL);
  const int outL = static_cast<int>(OpcVcvIr::OutputId::OutputL);
  module.inputs[static_cast<size_t>(inL)].connected = true;

  ProcessArgs args{48000.f, 1.f / 48000.f};

  // Flush latency — also promotes the pending IR after sample rate change
  for (size_t i = 0; i < kLatencyFlushFrames; ++i)
    module.process(args);

  ASSERT_TRUE(module.irProcessor_.isIR1Loaded())
      << "IR should be loaded after sample rate change and processing";

  float peak = 0.0f;
  for (float sample : dryInput_)
  {
    module.inputs[static_cast<size_t>(inL)].voltage = sample;
    module.process(args);
    peak = std::max(peak, std::abs(module.outputs[static_cast<size_t>(outL)].getVoltage()));
  }
  EXPECT_GT(peak, 1e-6f) << "Output is silent after sample rate change";
}

// ---------------------------------------------------------------------------
// IR enable switch OFF
// ---------------------------------------------------------------------------

TEST_F(VcvAudioTest, IrADisabled_OutputMatchesDryPassthrough)
{
  // With IR loaded but disabled, output should be dry passthrough —
  // identical to having no IR loaded at all.
  OpcVcvIr module;
  SampleRateChangeEvent sr{kSampleRate};
  module.onSampleRateChange(sr);
  module.loadIR(kIrAPath);
  module.params[static_cast<int>(OpcVcvIr::ParamId::IrAEnableParam)].setValue(0.f);
  module.params[static_cast<int>(OpcVcvIr::ParamId::IrBEnableParam)].setValue(0.f);

  auto outDisabled = processMonoInput(module, dryInput_);

  OpcVcvIr moduleNoIR;
  moduleNoIR.onSampleRateChange(sr);
  auto outNoIR = processMonoInput(moduleNoIR, dryInput_);

  const double r = pearsonCorrelation(outDisabled.L, outNoIR.L);
  EXPECT_GT(r, 0.9999) << "Disabled IR output should match no-IR passthrough (r=" << r << ")";
}

// ---------------------------------------------------------------------------
// Negative CV direction
// ---------------------------------------------------------------------------

TEST_F(VcvAudioTest, ThresholdCv_NegativeClampedToMin)
{
  OpcVcvIr module;
  SampleRateChangeEvent sr{kSampleRate};
  module.onSampleRateChange(sr);

  module.params[static_cast<int>(OpcVcvIr::ParamId::ThresholdParam)].setValue(-30.f);
  const int threshCvIn = static_cast<int>(OpcVcvIr::InputId::ThresholdCvIn);
  module.inputs[static_cast<size_t>(threshCvIn)].connected = true;
  module.inputs[static_cast<size_t>(threshCvIn)].voltage = -5.0f;

  ProcessArgs args{kSampleRate, 1.f / kSampleRate};
  module.process(args);

  EXPECT_NEAR(module.irProcessor_.getThreshold(), -60.f, 0.01f)
      << "Threshold CV at -5V should clamp to -60 dB";
}

TEST_F(VcvAudioTest, BlendCv_NegativeClampedToMin)
{
  OpcVcvIr module;
  SampleRateChangeEvent sr{kSampleRate};
  module.onSampleRateChange(sr);

  module.params[static_cast<int>(OpcVcvIr::ParamId::BlendParam)].setValue(0.f);
  const int blendCvIn = static_cast<int>(OpcVcvIr::InputId::BlendCvIn);
  module.inputs[static_cast<size_t>(blendCvIn)].connected = true;
  module.inputs[static_cast<size_t>(blendCvIn)].voltage = -5.0f;

  ProcessArgs args{kSampleRate, 1.f / kSampleRate};
  module.process(args);

  EXPECT_NEAR(module.irProcessor_.getBlend(), -1.f, 0.01f)
      << "Blend CV at -5V should clamp to -1.0";
}

// ---------------------------------------------------------------------------
// Metering atomics
// ---------------------------------------------------------------------------

TEST_F(VcvAudioTest, MeteringAtomics_UpdatedAfterProcessing)
{
  OpcVcvIr module;
  SampleRateChangeEvent sr{kSampleRate};
  module.onSampleRateChange(sr);
  module.loadIR(kIrAPath);
  module.loadIR2(kIrBPath);
  module.params[static_cast<int>(OpcVcvIr::ParamId::IrAEnableParam)].setValue(1.f);
  module.params[static_cast<int>(OpcVcvIr::ParamId::IrBEnableParam)].setValue(1.f);
  module.params[static_cast<int>(OpcVcvIr::ParamId::BlendParam)].setValue(0.3f);

  const int inL = static_cast<int>(OpcVcvIr::InputId::AudioInL);
  module.inputs[static_cast<size_t>(inL)].connected = true;

  ProcessArgs args{kSampleRate, 1.f / kSampleRate};
  for (size_t i = 0; i < kLatencyFlushFrames; ++i)
    module.process(args);

  module.inputs[static_cast<size_t>(inL)].voltage = 1.0f;
  module.process(args);

  EXPECT_GT(module.currentInputLevelDb_.load(std::memory_order_relaxed), -96.f)
      << "Input level metering should reflect non-silent input";
  EXPECT_NEAR(module.currentBlend_.load(std::memory_order_relaxed), 0.3f, 0.05f)
      << "Blend metering should reflect the blend param";
}
