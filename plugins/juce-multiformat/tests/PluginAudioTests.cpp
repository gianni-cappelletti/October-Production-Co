#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

// DR_WAV_IMPLEMENTATION is compiled into octobir-core via IRLoader.cpp.
// Including the header here without redefining the macro uses those symbols via linking.
#include "PluginProcessor.h"
#include "dr_wav.h"

static const std::string kIrAPath = std::string(TEST_DATA_DIR) + "/INPUT_ir_a.wav";
static const std::string kIrBPath = std::string(TEST_DATA_DIR) + "/INPUT_ir_b.wav";
static const std::string kIrStereoPath = std::string(TEST_DATA_DIR) + "/INPUT_ir_stereo.wav";
static const std::string kDryPath = std::string(TEST_DATA_DIR) + "/INPUT_amp_output_no_ir.wav";

static constexpr int kSampleRate = 44100;
static constexpr int kBlockSize = 512;

namespace
{

std::vector<float> loadWavMono(const std::string& path, unsigned int& outSampleRate)
{
  drwav_uint32 channels = 0;
  drwav_uint32 sampleRate = 0;
  drwav_uint64 totalFrames = 0;

  float* raw = drwav_open_file_and_read_pcm_frames_f32(path.c_str(), &channels, &sampleRate,
                                                       &totalFrames, nullptr);
  if (!raw)
    return {};

  outSampleRate = sampleRate;
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

double pearsonCorrelation(const std::vector<float>& a, const std::vector<float>& b)
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
    double da = a[i] - meanA;
    double db = b[i] - meanB;
    num += da * db;
    varA += da * da;
    varB += db * db;
  }

  double denom = std::sqrt(varA * varB);
  return (denom > 0.0) ? num / denom : 0.0;
}

// Processes dryInput through the plugin using a stereo buffer (kBlockSize frames per block).
// Input is copied to both channels; output is taken from channel 0 after latency trimming.
// Mirrors the processAndAlign() helper in ComponentTests.cpp.
std::vector<float> processAndAlign(OctobIRProcessor& processor, const std::vector<float>& dryInput)
{
  const size_t totalFrames = dryInput.size();
  std::vector<float> rawOutput;
  rawOutput.reserve(totalFrames + 2048);

  size_t framesConsumed = 0;
  while (framesConsumed < totalFrames)
  {
    juce::AudioBuffer<float> buf(2, kBlockSize);
    buf.clear();
    const size_t toCopy = std::min(static_cast<size_t>(kBlockSize), totalFrames - framesConsumed);
    for (size_t i = 0; i < toCopy; ++i)
    {
      buf.setSample(0, static_cast<int>(i), dryInput[framesConsumed + i]);
      buf.setSample(1, static_cast<int>(i), dryInput[framesConsumed + i]);
    }
    juce::MidiBuffer midi;
    processor.processBlock(buf, midi);
    for (int i = 0; i < kBlockSize; ++i)
      rawOutput.push_back(buf.getSample(0, i));
    framesConsumed += kBlockSize;
  }

  // Latency is valid after the first processBlock call (pending IR applied there).
  const int latency = processor.getLatencySamples();

  size_t tailFlushed = 0;
  while (tailFlushed < static_cast<size_t>(latency))
  {
    juce::AudioBuffer<float> buf(2, kBlockSize);
    buf.clear();
    juce::MidiBuffer midi;
    processor.processBlock(buf, midi);
    for (int i = 0; i < kBlockSize; ++i)
      rawOutput.push_back(buf.getSample(0, i));
    tailFlushed += kBlockSize;
  }

  std::vector<float> aligned(rawOutput.begin() + latency,
                             rawOutput.begin() + latency + static_cast<ptrdiff_t>(totalFrames));
  return aligned;
}

}  // namespace

class PluginAudioTest : public ::testing::Test
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

// Scenario: IR A loaded and enabled; verify the plugin produces non-silent output.
TEST_F(PluginAudioTest, ProcessBlock_NonSilentWithIRLoaded)
{
  OctobIRProcessor processor;
  processor.prepareToPlay(kSampleRate, kBlockSize);
  juce::String err;
  ASSERT_TRUE(processor.loadImpulseResponse1(kIrAPath, err)) << err;

  auto output = processAndAlign(processor, dryInput_);

  float peak = 0.0f;
  for (float s : output)
    peak = std::max(peak, std::abs(s));
  EXPECT_GT(peak, 1e-6f) << "Output is silent despite IR A being loaded and enabled";
}

// Scenario: Mono input processing — verify the plugin produces non-silent output
// when configured with a mono bus layout (1 in, 1 out).
TEST_F(PluginAudioTest, ProcessBlock_MonoNonSilentWithIRLoaded)
{
  OctobIRProcessor processor;

  juce::AudioProcessor::BusesLayout monoLayout;
  monoLayout.inputBuses.add(juce::AudioChannelSet::mono());
  monoLayout.inputBuses.add(juce::AudioChannelSet::disabled());
  monoLayout.outputBuses.add(juce::AudioChannelSet::mono());
  ASSERT_TRUE(processor.checkBusesLayoutSupported(monoLayout))
      << "Mono layout not supported — this will prevent the plugin from appearing "
         "on mono tracks in Logic Pro";
  processor.setBusesLayout(monoLayout);

  processor.prepareToPlay(kSampleRate, kBlockSize);
  juce::String err;
  ASSERT_TRUE(processor.loadImpulseResponse1(kIrAPath, err)) << err;

  const size_t totalFrames = dryInput_.size();
  std::vector<float> rawOutput;
  rawOutput.reserve(totalFrames + 2048);

  size_t framesConsumed = 0;
  while (framesConsumed < totalFrames)
  {
    juce::AudioBuffer<float> buf(1, kBlockSize);
    buf.clear();
    const size_t toCopy = std::min(static_cast<size_t>(kBlockSize), totalFrames - framesConsumed);
    for (size_t i = 0; i < toCopy; ++i)
      buf.setSample(0, static_cast<int>(i), dryInput_[framesConsumed + i]);
    juce::MidiBuffer midi;
    processor.processBlock(buf, midi);
    for (int i = 0; i < kBlockSize; ++i)
      rawOutput.push_back(buf.getSample(0, i));
    framesConsumed += kBlockSize;
  }

  const int latency = processor.getLatencySamples();
  size_t tailFlushed = 0;
  while (tailFlushed < static_cast<size_t>(latency))
  {
    juce::AudioBuffer<float> buf(1, kBlockSize);
    buf.clear();
    juce::MidiBuffer midi;
    processor.processBlock(buf, midi);
    for (int i = 0; i < kBlockSize; ++i)
      rawOutput.push_back(buf.getSample(0, i));
    tailFlushed += kBlockSize;
  }

  std::vector<float> aligned(rawOutput.begin() + latency,
                             rawOutput.begin() + latency + static_cast<ptrdiff_t>(totalFrames));

  float peak = 0.0f;
  for (float s : aligned)
    peak = std::max(peak, std::abs(s));
  EXPECT_GT(peak, 1e-6f) << "Mono output is silent despite IR A being loaded and enabled";
}

// Scenario: Mono-to-stereo processing — verify the plugin produces non-silent output
// on both channels when configured with mono input and stereo output.
TEST_F(PluginAudioTest, ProcessBlock_MonoToStereoNonSilentWithIRLoaded)
{
  OctobIRProcessor processor;

  juce::AudioProcessor::BusesLayout m2sLayout;
  m2sLayout.inputBuses.add(juce::AudioChannelSet::mono());
  m2sLayout.inputBuses.add(juce::AudioChannelSet::disabled());
  m2sLayout.outputBuses.add(juce::AudioChannelSet::stereo());
  ASSERT_TRUE(processor.checkBusesLayoutSupported(m2sLayout))
      << "Mono-to-stereo layout not supported — this will prevent the plugin from appearing "
         "as 'Mono -> Stereo' on mono tracks in Logic Pro";
  processor.setBusesLayout(m2sLayout);

  processor.prepareToPlay(kSampleRate, kBlockSize);
  juce::String err;
  ASSERT_TRUE(processor.loadImpulseResponse1(kIrAPath, err)) << err;

  const size_t totalFrames = dryInput_.size();
  std::vector<float> rawOutputL, rawOutputR;
  rawOutputL.reserve(totalFrames + 2048);
  rawOutputR.reserve(totalFrames + 2048);

  size_t framesConsumed = 0;
  while (framesConsumed < totalFrames)
  {
    // 2-channel buffer: ch0 is mono input bus, ch1 is stereo output's R channel
    juce::AudioBuffer<float> buf(2, kBlockSize);
    buf.clear();
    const size_t toCopy = std::min(static_cast<size_t>(kBlockSize), totalFrames - framesConsumed);
    for (size_t i = 0; i < toCopy; ++i)
      buf.setSample(0, static_cast<int>(i), dryInput_[framesConsumed + i]);
    juce::MidiBuffer midi;
    processor.processBlock(buf, midi);
    for (int i = 0; i < kBlockSize; ++i)
    {
      rawOutputL.push_back(buf.getSample(0, i));
      rawOutputR.push_back(buf.getSample(1, i));
    }
    framesConsumed += kBlockSize;
  }

  const int latency = processor.getLatencySamples();
  size_t tailFlushed = 0;
  while (tailFlushed < static_cast<size_t>(latency))
  {
    juce::AudioBuffer<float> buf(2, kBlockSize);
    buf.clear();
    juce::MidiBuffer midi;
    processor.processBlock(buf, midi);
    for (int i = 0; i < kBlockSize; ++i)
    {
      rawOutputL.push_back(buf.getSample(0, i));
      rawOutputR.push_back(buf.getSample(1, i));
    }
    tailFlushed += kBlockSize;
  }

  std::vector<float> alignedL(rawOutputL.begin() + latency,
                              rawOutputL.begin() + latency + static_cast<ptrdiff_t>(totalFrames));
  std::vector<float> alignedR(rawOutputR.begin() + latency,
                              rawOutputR.begin() + latency + static_cast<ptrdiff_t>(totalFrames));

  float peakL = 0.0f, peakR = 0.0f;
  for (size_t i = 0; i < alignedL.size(); ++i)
  {
    peakL = std::max(peakL, std::abs(alignedL[i]));
    peakR = std::max(peakR, std::abs(alignedR[i]));
  }
  EXPECT_GT(peakL, 1e-6f) << "Left channel is silent in mono-to-stereo mode";
  EXPECT_GT(peakR, 1e-6f) << "Right channel is silent in mono-to-stereo mode";
}

// Scenario: Equal-power blend (blend=0.0f) is commutative — swapping IR slots must not
// change the plugin output.
//
// Preconditions:
//   - Pre-swap processor: IR A in slot 1, IR B in slot 2, blend=0.0f (default).
//   - Post-swap processor: IR B in slot 1, IR A in slot 2, blend=0.0f (default).
//   - At blend=0.0f: gain_A = gain_B = sqrt(0.5). The output is
//     sqrt(0.5)*conv(x,A) + sqrt(0.5)*conv(x,B), which is slot-order invariant.
//
// Verifies:
//   Pearson r between the two outputs is > 0.9999. This threshold is tighter than the
//   reference-comparison tests (0.99) because this is a mathematical identity under a
//   correct implementation — any meaningful divergence points directly at the plugin layer.
TEST_F(PluginAudioTest, SwapSymmetry_AudioOutput)
{
  std::vector<float> outputAB;
  {
    OctobIRProcessor p;
    p.prepareToPlay(kSampleRate, kBlockSize);
    juce::String err;
    ASSERT_TRUE(p.loadImpulseResponse1(kIrAPath, err)) << err;
    ASSERT_TRUE(p.loadImpulseResponse2(kIrBPath, err)) << err;
    outputAB = processAndAlign(p, dryInput_);
    float peak = 0.0f;
    for (float s : outputAB)
      peak = std::max(peak, std::abs(s));
    ASSERT_GT(peak, 1e-6f) << "Pre-swap output is silent";
  }

  std::vector<float> outputBA;
  {
    OctobIRProcessor p;
    p.prepareToPlay(kSampleRate, kBlockSize);
    juce::String err;
    ASSERT_TRUE(p.loadImpulseResponse1(kIrBPath, err)) << err;
    ASSERT_TRUE(p.loadImpulseResponse2(kIrAPath, err)) << err;
    outputBA = processAndAlign(p, dryInput_);
    float peak = 0.0f;
    for (float s : outputBA)
      peak = std::max(peak, std::abs(s));
    ASSERT_GT(peak, 1e-6f) << "Post-swap output is silent";
  }

  const size_t compareLen = std::min(outputAB.size(), outputBA.size());
  outputAB.resize(compareLen);
  outputBA.resize(compareLen);

  const double r = pearsonCorrelation(outputAB, outputBA);

  std::cout << "[PluginSwapSymmetry] Pearson r: " << r << "\n";
  std::cout << "[PluginSwapSymmetry] Pre-swap latency used for alignment\n";

  EXPECT_GT(r, 0.9999) << "Swap symmetry violated at the plugin layer (r=" << r << "). "
                       << "The output differs based on IR slot assignment. "
                       << "Bug is in OctobIRProcessor, not octobir-core "
                       << "(octobir-core passes the equivalent ComponentTest).";
}

// Scenario: Latency reported by the plugin should be the same regardless of which IR
// occupies which slot (assuming the same two IRs are loaded).
TEST_F(PluginAudioTest, LatencyConsistentAfterSwap)
{
  OctobIRProcessor processor;
  processor.prepareToPlay(kSampleRate, kBlockSize);
  juce::String err;
  ASSERT_TRUE(processor.loadImpulseResponse1(kIrAPath, err));
  ASSERT_TRUE(processor.loadImpulseResponse2(kIrBPath, err));
  const int latencyBefore = processor.getLatencySamples();

  processor.swapImpulseResponses();
  const int latencyAfter = processor.getLatencySamples();

  EXPECT_EQ(latencyBefore, latencyAfter)
      << "Latency changed after swap despite the same pair of IRs being loaded";
}

// Scenario: Mono-to-stereo with a stereo IR — verify L and R outputs are meaningfully
// different. A mono IR produces L == R; a stereo IR must produce distinct L and R.
TEST_F(PluginAudioTest, MonoToStereo_ChannelsAreDifferent_StereoIR)
{
  OctobIRProcessor processor;

  juce::AudioProcessor::BusesLayout m2sLayout;
  m2sLayout.inputBuses.add(juce::AudioChannelSet::mono());
  m2sLayout.inputBuses.add(juce::AudioChannelSet::disabled());
  m2sLayout.outputBuses.add(juce::AudioChannelSet::stereo());
  ASSERT_TRUE(processor.checkBusesLayoutSupported(m2sLayout));
  processor.setBusesLayout(m2sLayout);

  processor.prepareToPlay(kSampleRate, kBlockSize);
  juce::String err;
  ASSERT_TRUE(processor.loadImpulseResponse1(kIrStereoPath, err)) << err;

  const size_t totalFrames = dryInput_.size();
  std::vector<float> rawL, rawR;
  rawL.reserve(totalFrames + 2048);
  rawR.reserve(totalFrames + 2048);

  size_t framesConsumed = 0;
  while (framesConsumed < totalFrames)
  {
    juce::AudioBuffer<float> buf(2, kBlockSize);
    buf.clear();
    const size_t toCopy = std::min(static_cast<size_t>(kBlockSize), totalFrames - framesConsumed);
    for (size_t i = 0; i < toCopy; ++i)
      buf.setSample(0, static_cast<int>(i), dryInput_[framesConsumed + i]);
    juce::MidiBuffer midi;
    processor.processBlock(buf, midi);
    for (int i = 0; i < kBlockSize; ++i)
    {
      rawL.push_back(buf.getSample(0, i));
      rawR.push_back(buf.getSample(1, i));
    }
    framesConsumed += kBlockSize;
  }

  const int latency = processor.getLatencySamples();
  size_t tailFlushed = 0;
  while (tailFlushed < static_cast<size_t>(latency))
  {
    juce::AudioBuffer<float> buf(2, kBlockSize);
    buf.clear();
    juce::MidiBuffer midi;
    processor.processBlock(buf, midi);
    for (int i = 0; i < kBlockSize; ++i)
    {
      rawL.push_back(buf.getSample(0, i));
      rawR.push_back(buf.getSample(1, i));
    }
    tailFlushed += kBlockSize;
  }

  std::vector<float> alignedL(rawL.begin() + latency,
                              rawL.begin() + latency + static_cast<ptrdiff_t>(totalFrames));
  std::vector<float> alignedR(rawR.begin() + latency,
                              rawR.begin() + latency + static_cast<ptrdiff_t>(totalFrames));

  const double r = pearsonCorrelation(alignedL, alignedR);
  EXPECT_LT(r, 0.9999) << "L and R are identical (r=" << r
                       << ") despite loading a stereo IR — processMonoToStereo may "
                       << "be ignoring the right IR channel";
}
