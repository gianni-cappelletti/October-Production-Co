#include <gtest/gtest.h>
#include <juce_dsp/juce_dsp.h>

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

std::vector<float> directConvolve(const std::vector<float>& ir, const std::vector<float>& dry)
{
  const size_t outLen = dry.size();
  const size_t irLen = ir.size();
  std::vector<float> out(outLen, 0.0f);
  for (size_t n = 0; n < outLen; ++n)
  {
    float sum = 0.0f;
    const size_t kMax = std::min(irLen, n + 1);
    for (size_t k = 0; k < kMax; ++k)
      sum += ir[k] * dry[n - k];
    out[n] = sum;
  }
  return out;
}

unsigned int wavChannelCount(const std::string& path)
{
  drwav wav;
  if (!drwav_init_file(&wav, path.c_str(), nullptr))
    return 0;
  const unsigned int channels = wav.channels;
  drwav_uninit(&wav);
  return channels;
}

float peakMagnitude(const std::vector<float>& v)
{
  float pk = 0.0f;
  for (float s : v)
    pk = std::max(pk, std::abs(s));
  return pk;
}

// Magnitude spectrum (positive-frequency bins) via a zero-padded power-of-two FFT.
// Used to compare signals whose phase differs but whose magnitude response should
// match — e.g. an IR reloaded through IRLoader, which re-applies the minimum-phase
// transform (preserves magnitude, changes phase).
std::vector<float> magnitudeSpectrum(const std::vector<float>& signal)
{
  if (signal.empty())
    return {};

  int order = 0;
  while ((static_cast<size_t>(1) << order) < signal.size())
    ++order;
  const int fftSize = 1 << order;

  juce::dsp::FFT fft(order);
  std::vector<float> data(static_cast<size_t>(2 * fftSize), 0.0f);
  for (size_t i = 0; i < signal.size(); ++i)
    data[i] = signal[i];

  fft.performRealOnlyForwardTransform(data.data());

  std::vector<float> mag(static_cast<size_t>(fftSize / 2 + 1), 0.0f);
  for (int k = 0; k <= fftSize / 2; ++k)
  {
    const float re = data[static_cast<size_t>(2 * k)];
    const float im = data[static_cast<size_t>(2 * k + 1)];
    mag[static_cast<size_t>(k)] = std::sqrt(re * re + im * im);
  }
  return mag;
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

// Scenario: IR A loaded, slot B enabled but empty, blend fully toward slot B.
// The empty-but-enabled slot should act as dry passthrough — output must match the input.
TEST_F(PluginAudioTest, EmptyEnabledSlot_BlendFullB_OutputMatchesDry)
{
  OctobIRProcessor processor;
  processor.prepareToPlay(kSampleRate, kBlockSize);

  auto& apvts = processor.getAPVTS();
  apvts.getParameter("irAEnable")->setValueNotifyingHost(1.f);
  apvts.getParameter("irBEnable")->setValueNotifyingHost(1.f);
  auto* blendParam = apvts.getParameter("blend");
  blendParam->setValueNotifyingHost(blendParam->convertTo0to1(1.f));

  juce::String err;
  ASSERT_TRUE(processor.loadImpulseResponse1(kIrAPath, err)) << err;

  auto output = processAndAlign(processor, dryInput_);

  float peak = 0.0f;
  for (float s : output)
    peak = std::max(peak, std::abs(s));
  ASSERT_GT(peak, 1e-6f)
      << "Output is silent — empty-but-enabled slot B should produce dry passthrough, not silence";

  std::vector<float> dryRef(dryInput_.begin(),
                            dryInput_.begin() + static_cast<ptrdiff_t>(output.size()));
  std::vector<float> normalizedOut = output;

  auto normalizePeak = [](std::vector<float>& v)
  {
    float pk = 0.0f;
    for (float s : v)
      pk = std::max(pk, std::abs(s));
    if (pk > 0.0f)
      for (float& s : v)
        s /= pk;
  };
  normalizePeak(normalizedOut);
  normalizePeak(dryRef);

  double r = pearsonCorrelation(normalizedOut, dryRef);
  EXPECT_GT(r, 0.999) << "With blend fully toward empty-but-enabled slot B, "
                      << "output should match dry input (r=" << r << ")";
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

// Scenario: Export the static 50/50 blend to a WAV file, then convolve the exported
// IR directly against the same dry input and verify that it reproduces the live
// blended output sample-for-sample (up to peak normalisation).
//
// We deliberately do NOT reload the exported file through OctobIR's IRLoader for this
// validation. The loader applies MPT on every load, and the sum of two minimum-phase
// IRs is not itself minimum-phase — re-running MPT would change the time-domain
// signal (same magnitude spectrum, different phase). Direct convolution avoids that
// extra MPT pass and proves the export is a faithful representation of the live
// blend by linearity of convolution.
TEST_F(PluginAudioTest, ExportedIRMatchesStaticBlend)
{
  OctobIRProcessor blendProc;
  blendProc.prepareToPlay(kSampleRate, kBlockSize);

  auto& blendApvts = blendProc.getAPVTS();
  blendApvts.getParameter("irAEnable")->setValueNotifyingHost(1.f);
  blendApvts.getParameter("irBEnable")->setValueNotifyingHost(1.f);

  juce::String err;
  ASSERT_TRUE(blendProc.loadImpulseResponse1(kIrAPath, err)) << err;
  ASSERT_TRUE(blendProc.loadImpulseResponse2(kIrBPath, err)) << err;

  auto* blendParam = blendApvts.getParameter("blend");
  blendParam->setValueNotifyingHost(blendParam->convertTo0to1(0.f));
  blendApvts.getParameter("dynamicMode")->setValueNotifyingHost(0.f);

  std::vector<float> liveOutput = processAndAlign(blendProc, dryInput_);

  juce::File tempWav =
      juce::File::getSpecialLocation(juce::File::tempDirectory)
          .getChildFile("octobir_export_test_" +
                        juce::String(juce::Random::getSystemRandom().nextInt()) + ".wav");
  ASSERT_TRUE(blendProc.exportBlendedIR(tempWav, err)) << err;
  ASSERT_TRUE(tempWav.existsAsFile()) << "Export reported success but file is missing";

  unsigned int wavSampleRate = 0;
  std::vector<float> exportedIR =
      loadWavMono(tempWav.getFullPathName().toStdString(), wavSampleRate);
  ASSERT_FALSE(exportedIR.empty()) << "Failed to read exported WAV";
  EXPECT_EQ(wavSampleRate, static_cast<unsigned int>(kSampleRate))
      << "Exported file sample rate should match host sample rate";

  const size_t outLen = dryInput_.size();
  std::vector<float> convolvedOutput(outLen, 0.0f);
  const size_t irLen = exportedIR.size();
  for (size_t n = 0; n < outLen; ++n)
  {
    float sum = 0.0f;
    const size_t kMax = std::min(irLen, n + 1);
    for (size_t k = 0; k < kMax; ++k)
      sum += exportedIR[k] * dryInput_[n - k];
    convolvedOutput[n] = sum;
  }

  auto normalizePeak = [](std::vector<float>& v)
  {
    float pk = 0.0f;
    for (float s : v)
      pk = std::max(pk, std::abs(s));
    if (pk > 0.0f)
      for (float& s : v)
        s /= pk;
  };

  normalizePeak(liveOutput);
  normalizePeak(convolvedOutput);

  const double r = pearsonCorrelation(liveOutput, convolvedOutput);
  EXPECT_GT(r, 0.9999) << "Exported IR convolution diverged from live 50/50 blend (r=" << r << ")";

  tempWav.deleteFile();
}

namespace
{
// Builds a processor with both IRs loaded, both slots enabled and static mode, with
// the blend set to the given native value. Returns the temp WAV path used for export.
void prepareViableBlendProcessor(OctobIRProcessor& proc, float blendNative)
{
  proc.prepareToPlay(kSampleRate, kBlockSize);
  auto& apvts = proc.getAPVTS();
  apvts.getParameter("irAEnable")->setValueNotifyingHost(1.f);
  apvts.getParameter("irBEnable")->setValueNotifyingHost(1.f);
  apvts.getParameter("dynamicMode")->setValueNotifyingHost(0.f);

  juce::String err;
  ASSERT_TRUE(proc.loadImpulseResponse1(kIrAPath, err)) << err;
  ASSERT_TRUE(proc.loadImpulseResponse2(kIrBPath, err)) << err;

  auto* blendParam = apvts.getParameter("blend");
  blendParam->setValueNotifyingHost(blendParam->convertTo0to1(blendNative));
}

juce::File makeTempWav()
{
  return juce::File::getSpecialLocation(juce::File::tempDirectory)
      .getChildFile("octobir_export_test_" +
                    juce::String(juce::Random::getSystemRandom().nextInt()) + ".wav");
}
}  // namespace

// At blend = -1 the equal-power blend collapses to 100% IR A, so the exported IR
// convolved against the dry input must reproduce the live full-A output.
TEST_F(PluginAudioTest, Export_BlendFullA)
{
  OctobIRProcessor proc;
  prepareViableBlendProcessor(proc, -1.0f);

  const std::vector<float> liveOutput = processAndAlign(proc, dryInput_);

  juce::String err;
  juce::File tempWav = makeTempWav();
  ASSERT_TRUE(proc.exportBlendedIR(tempWav, err)) << err;

  unsigned int sr = 0;
  std::vector<float> exportedIR = loadWavMono(tempWav.getFullPathName().toStdString(), sr);
  ASSERT_FALSE(exportedIR.empty());

  std::vector<float> convolved = directConvolve(exportedIR, dryInput_);
  convolved.resize(liveOutput.size());

  std::vector<float> live = liveOutput;
  const float lp = peakMagnitude(live);
  const float cp = peakMagnitude(convolved);
  if (lp > 0.0f)
    for (float& s : live)
      s /= lp;
  if (cp > 0.0f)
    for (float& s : convolved)
      s /= cp;

  const double r = pearsonCorrelation(live, convolved);
  EXPECT_GT(r, 0.9999) << "Exported full-A IR diverged from live full-A output (r=" << r << ")";
  tempWav.deleteFile();
}

// At blend = +1 the equal-power blend collapses to 100% IR B.
TEST_F(PluginAudioTest, Export_BlendFullB)
{
  OctobIRProcessor proc;
  prepareViableBlendProcessor(proc, 1.0f);

  const std::vector<float> liveOutput = processAndAlign(proc, dryInput_);

  juce::String err;
  juce::File tempWav = makeTempWav();
  ASSERT_TRUE(proc.exportBlendedIR(tempWav, err)) << err;

  unsigned int sr = 0;
  std::vector<float> exportedIR = loadWavMono(tempWav.getFullPathName().toStdString(), sr);
  ASSERT_FALSE(exportedIR.empty());

  std::vector<float> convolved = directConvolve(exportedIR, dryInput_);
  convolved.resize(liveOutput.size());

  std::vector<float> live = liveOutput;
  const float lp = peakMagnitude(live);
  const float cp = peakMagnitude(convolved);
  if (lp > 0.0f)
    for (float& s : live)
      s /= lp;
  if (cp > 0.0f)
    for (float& s : convolved)
      s /= cp;

  const double r = pearsonCorrelation(live, convolved);
  EXPECT_GT(r, 0.9999) << "Exported full-B IR diverged from live full-B output (r=" << r << ")";
  tempWav.deleteFile();
}

// The IR trim gain must be baked into the exported kernel. At blend = -1 (full A)
// trimB is irrelevant; scaling trimA by a linear factor must scale the exported
// kernel peak by the same factor, provided neither export engages overload
// protection (peak stays under full scale).
TEST_F(PluginAudioTest, Export_TrimGainBakedIn)
{
  OctobIRProcessor proc;
  prepareViableBlendProcessor(proc, -1.0f);
  auto& irp = proc.getIRProcessor();

  octob::BlendedIRExport unity;
  std::string err;
  ASSERT_TRUE(irp.getStaticBlendedIR(-1.0f, 1.0f, 1.0f, unity, err)) << err;
  ASSERT_EQ(unity.normalizationScale, 1.0f) << "Unity-trim full-A export should not clip";

  const float peakUnity = peakMagnitude(unity.channels[0]);
  ASSERT_GT(peakUnity, 0.0f);
  constexpr float kFactor = 1.5f;
  ASSERT_LT(peakUnity * kFactor, 1.0f)
      << "Test IR too hot to isolate trim from overload protection";

  octob::BlendedIRExport scaled;
  ASSERT_TRUE(irp.getStaticBlendedIR(-1.0f, kFactor, 1.0f, scaled, err)) << err;
  ASSERT_EQ(scaled.normalizationScale, 1.0f) << "Scaled-trim export unexpectedly clipped";

  const float peakScaled = peakMagnitude(scaled.channels[0]);
  EXPECT_NEAR(peakScaled / peakUnity, kFactor, 0.01f)
      << "Trim gain was not baked in proportionally";
}

// When the blended kernel would exceed full scale, overload protection scales it
// down by a single global factor so the peak just reaches 1.0 — preserving shape.
TEST_F(PluginAudioTest, Export_OverloadProtection_PreventsClipping)
{
  OctobIRProcessor proc;
  prepareViableBlendProcessor(proc, 0.0f);
  auto& irp = proc.getIRProcessor();

  octob::BlendedIRExport hot;
  std::string err;
  ASSERT_TRUE(irp.getStaticBlendedIR(0.0f, 1000.0f, 1000.0f, hot, err)) << err;

  EXPECT_LT(hot.normalizationScale, 1.0f) << "Overload protection should have engaged";
  const float peak = peakMagnitude(hot.channels[0]);
  EXPECT_LE(peak, 1.0f + 1e-4f) << "Exported peak should not exceed full scale";
  EXPECT_GT(peak, 0.99f) << "Overload protection should normalise the peak to ~full scale";

  // Shape is preserved: a uniform scale relative to the un-protected blend.
  octob::BlendedIRExport ref;
  ASSERT_TRUE(irp.getStaticBlendedIR(0.0f, 1.0f, 1.0f, ref, err)) << err;
  ASSERT_EQ(ref.normalizationScale, 1.0f);

  std::vector<float> a = hot.channels[0];
  std::vector<float> b = ref.channels[0];
  const float ap = peakMagnitude(a);
  const float bp = peakMagnitude(b);
  for (float& s : a)
    s /= ap;
  for (float& s : b)
    s /= bp;
  const double r = pearsonCorrelation(a, b);
  EXPECT_GT(r, 0.9999) << "Overload protection altered the kernel shape (r=" << r << ")";
}

// A stereo IR in either slot must produce a two-channel exported WAV.
TEST_F(PluginAudioTest, Export_StereoProducesTwoChannels)
{
  OctobIRProcessor proc;
  proc.prepareToPlay(kSampleRate, kBlockSize);
  auto& apvts = proc.getAPVTS();
  apvts.getParameter("irAEnable")->setValueNotifyingHost(1.f);
  apvts.getParameter("irBEnable")->setValueNotifyingHost(1.f);
  apvts.getParameter("dynamicMode")->setValueNotifyingHost(0.f);

  juce::String err;
  ASSERT_TRUE(proc.loadImpulseResponse1(kIrStereoPath, err)) << err;
  ASSERT_TRUE(proc.loadImpulseResponse2(kIrBPath, err)) << err;

  auto* blendParam = apvts.getParameter("blend");
  blendParam->setValueNotifyingHost(blendParam->convertTo0to1(0.f));

  juce::File tempWav = makeTempWav();
  ASSERT_TRUE(proc.exportBlendedIR(tempWav, err)) << err;
  EXPECT_EQ(wavChannelCount(tempWav.getFullPathName().toStdString()), 2u)
      << "A stereo IR should export a two-channel WAV";
  tempWav.deleteFile();
}

// Export is blocked unless two IRs are loaded, both enabled, in static mode. Each
// non-viable state reports the corresponding reason and refuses to export.
TEST_F(PluginAudioTest, Export_Blocked_DynamicMode)
{
  OctobIRProcessor proc;
  prepareViableBlendProcessor(proc, 0.0f);
  proc.getAPVTS().getParameter("dynamicMode")->setValueNotifyingHost(1.f);

  EXPECT_EQ(proc.getBlendedIRExportInfo().viability, IRExportViability::DynamicModeActive);

  juce::File tempWav = makeTempWav();
  juce::String err;
  EXPECT_FALSE(proc.exportBlendedIR(tempWav, err));
  EXPECT_FALSE(tempWav.existsAsFile());
}

TEST_F(PluginAudioTest, Export_Blocked_SingleIR)
{
  OctobIRProcessor proc;
  proc.prepareToPlay(kSampleRate, kBlockSize);
  auto& apvts = proc.getAPVTS();
  apvts.getParameter("irAEnable")->setValueNotifyingHost(1.f);
  apvts.getParameter("irBEnable")->setValueNotifyingHost(1.f);
  apvts.getParameter("dynamicMode")->setValueNotifyingHost(0.f);

  juce::String err;
  ASSERT_TRUE(proc.loadImpulseResponse1(kIrAPath, err)) << err;

  EXPECT_EQ(proc.getBlendedIRExportInfo().viability, IRExportViability::NeedsTwoIRs);

  juce::File tempWav = makeTempWav();
  EXPECT_FALSE(proc.exportBlendedIR(tempWav, err));
  EXPECT_FALSE(tempWav.existsAsFile());
}

TEST_F(PluginAudioTest, Export_Blocked_DisabledSlot)
{
  OctobIRProcessor proc;
  prepareViableBlendProcessor(proc, 0.0f);
  proc.getAPVTS().getParameter("irBEnable")->setValueNotifyingHost(0.f);

  EXPECT_EQ(proc.getBlendedIRExportInfo().viability, IRExportViability::SlotDisabled);

  juce::File tempWav = makeTempWav();
  juce::String err;
  EXPECT_FALSE(proc.exportBlendedIR(tempWav, err));
  EXPECT_FALSE(tempWav.existsAsFile());
}

TEST_F(PluginAudioTest, Export_Blocked_NotPrepared)
{
  OctobIRProcessor proc;  // never prepared, no IRs
  EXPECT_NE(proc.getBlendedIRExportInfo().viability, IRExportViability::Ok);

  juce::File tempWav = makeTempWav();
  juce::String err;
  EXPECT_FALSE(proc.exportBlendedIR(tempWav, err));
  EXPECT_FALSE(tempWav.existsAsFile());
}

// User-requested round trip: export the live 50/50 blend, reload it through the
// plugin, and play it at 100% A. The reload re-applies IRLoader's unconditional
// minimum-phase transform (IRLoader.cpp), which preserves the magnitude spectrum
// but changes phase — so the two outputs match in MAGNITUDE, not sample-for-sample.
TEST_F(PluginAudioTest, Export_RoundTripMagnitudeMatchesLiveBlend)
{
  OctobIRProcessor blendProc;
  prepareViableBlendProcessor(blendProc, 0.0f);
  const std::vector<float> liveOutput = processAndAlign(blendProc, dryInput_);

  juce::String err;
  juce::File tempWav = makeTempWav();
  ASSERT_TRUE(blendProc.exportBlendedIR(tempWav, err)) << err;

  OctobIRProcessor reloadProc;
  reloadProc.prepareToPlay(kSampleRate, kBlockSize);
  auto& apvts = reloadProc.getAPVTS();
  apvts.getParameter("irAEnable")->setValueNotifyingHost(1.f);
  apvts.getParameter("irBEnable")->setValueNotifyingHost(0.f);
  apvts.getParameter("dynamicMode")->setValueNotifyingHost(0.f);
  ASSERT_TRUE(reloadProc.loadImpulseResponse1(tempWav.getFullPathName(), err)) << err;
  auto* blendParam = apvts.getParameter("blend");
  blendParam->setValueNotifyingHost(blendParam->convertTo0to1(-1.0f));  // full A

  const std::vector<float> roundTripOutput = processAndAlign(reloadProc, dryInput_);

  std::vector<float> liveMag = magnitudeSpectrum(liveOutput);
  std::vector<float> roundTripMag = magnitudeSpectrum(roundTripOutput);

  const double r = pearsonCorrelation(liveMag, roundTripMag);
  EXPECT_GT(r, 0.99) << "Reloaded exported IR magnitude spectrum diverged from the live 50/50 "
                     << "blend (r=" << r << ")";
  tempWav.deleteFile();
}
