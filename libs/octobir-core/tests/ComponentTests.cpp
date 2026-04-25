#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numeric>
#include <string>
#include <vector>

// DR_WAV_IMPLEMENTATION is compiled into octobir-core via IRLoader.cpp.
// Including the header here without redefining the macro uses those symbols via linking.
#include "dr_wav.h"
#include "octobir-core/IRProcessor.hpp"

using namespace octob;

namespace
{

std::vector<float> loadWavMono(const std::string& path, unsigned int& outSampleRate,
                               drwav_uint64& outFrameCount)
{
  drwav_uint32 channels = 0;
  drwav_uint32 sampleRate = 0;
  drwav_uint64 totalFrames = 0;

  float* raw = drwav_open_file_and_read_pcm_frames_f32(path.c_str(), &channels, &sampleRate,
                                                       &totalFrames, nullptr);
  if (!raw)
    return {};

  outSampleRate = sampleRate;
  outFrameCount = totalFrames;

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

void normalizeToUnitPeak(std::vector<float>& samples)
{
  float peak = 0.0f;
  for (float s : samples)
    peak = std::max(peak, std::abs(s));
  if (peak > 0.0f)
    for (float& s : samples)
      s /= peak;
}

// Returns the lag (in samples) within [-maxLag, +maxLag] at which Pearson correlation
// between a and b is maximised. Positive lag means b leads a (b is ahead).
int findAlignmentLag(const std::vector<float>& a, const std::vector<float>& b, int maxLag);

double pearsonCorrelation(const std::vector<float>& a, const std::vector<float>& b)
{
  const size_t n = a.size();

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

double computeSnrDb(const std::vector<float>& reference, const std::vector<float>& actual)
{
  double sigPower = 0.0, errPower = 0.0;
  for (size_t i = 0; i < reference.size(); ++i)
  {
    sigPower += static_cast<double>(reference[i]) * reference[i];
    double err = reference[i] - actual[i];
    errPower += err * err;
  }
  if (errPower == 0.0)
    return std::numeric_limits<double>::infinity();
  return 10.0 * std::log10(sigPower / errPower);
}

int findAlignmentLag(const std::vector<float>& a, const std::vector<float>& b, int maxLag)
{
  const size_t n = std::min(a.size(), b.size());
  int bestLag = 0;
  double bestR = -2.0;

  for (int lag = -maxLag; lag <= maxLag; ++lag)
  {
    const size_t skip = static_cast<size_t>(std::abs(lag));
    if (skip >= n)
      continue;
    const size_t len = n - skip;

    std::vector<float> aSlice, bSlice;
    if (lag >= 0)
    {
      aSlice.assign(a.begin(), a.begin() + static_cast<ptrdiff_t>(len));
      bSlice.assign(b.begin() + static_cast<ptrdiff_t>(lag),
                    b.begin() + static_cast<ptrdiff_t>(lag + len));
    }
    else
    {
      aSlice.assign(a.begin() + static_cast<ptrdiff_t>(-lag),
                    a.begin() + static_cast<ptrdiff_t>(-lag + len));
      bSlice.assign(b.begin(), b.begin() + static_cast<ptrdiff_t>(len));
    }

    double r = pearsonCorrelation(aSlice, bSlice);
    if (r > bestR)
    {
      bestR = r;
      bestLag = lag;
    }
  }
  return bestLag;
}

bool writeWavMono(const std::string& path, const std::vector<float>& samples,
                  unsigned int sampleRate)
{
  drwav_data_format format;
  format.container = drwav_container_riff;
  format.format = DR_WAVE_FORMAT_IEEE_FLOAT;
  format.channels = 1;
  format.sampleRate = sampleRate;
  format.bitsPerSample = 32;

  drwav wav;
  if (!drwav_init_file_write(&wav, path.c_str(), &format, nullptr))
    return false;

  drwav_write_pcm_frames(&wav, samples.size(), samples.data());
  drwav_uninit(&wav);
  return true;
}

// Processes dryInput through processor in kBlockSize blocks, flushes the convolution tail,
// trims the reported latency prefix, and returns the time-aligned output.
std::vector<float> processAndAlign(IRProcessor& processor, const std::vector<float>& dryInput,
                                   int kBlockSize)
{
  const size_t totalInputFrames = dryInput.size();
  std::vector<float> rawOutput;
  rawOutput.reserve(totalInputFrames + 2048);

  std::vector<float> inputBlock(kBlockSize, 0.0f);
  std::vector<float> outputBlock(kBlockSize, 0.0f);

  size_t framesConsumed = 0;
  while (framesConsumed < totalInputFrames)
  {
    size_t toCopy = std::min(static_cast<size_t>(kBlockSize), totalInputFrames - framesConsumed);
    std::fill(inputBlock.begin(), inputBlock.end(), 0.0f);
    std::copy(dryInput.begin() + static_cast<ptrdiff_t>(framesConsumed),
              dryInput.begin() + static_cast<ptrdiff_t>(framesConsumed + toCopy),
              inputBlock.begin());
    processor.processMono(inputBlock.data(), outputBlock.data(), kBlockSize);
    rawOutput.insert(rawOutput.end(), outputBlock.begin(), outputBlock.end());
    framesConsumed += kBlockSize;
  }

  // Latency is valid after the first processMono() call (pending IR swap applied there).
  const int latency = processor.getLatencySamples();

  size_t tailFlushed = 0;
  while (tailFlushed < static_cast<size_t>(latency))
  {
    std::fill(inputBlock.begin(), inputBlock.end(), 0.0f);
    processor.processMono(inputBlock.data(), outputBlock.data(), kBlockSize);
    rawOutput.insert(rawOutput.end(), outputBlock.begin(), outputBlock.end());
    tailFlushed += kBlockSize;
  }

  std::vector<float> aligned(
      rawOutput.begin() + latency,
      rawOutput.begin() + latency + static_cast<ptrdiff_t>(totalInputFrames));
  return aligned;
}

}  // namespace

class ComponentTest : public ::testing::Test
{
 protected:
  void SetUp() override
  {
    irAPath_ = std::string(TEST_DATA_DIR) + "/INPUT_ir_a.wav";
    irBPath_ = std::string(TEST_DATA_DIR) + "/INPUT_ir_b.wav";
    dryPath_ = std::string(TEST_DATA_DIR) + "/INPUT_amp_output_no_ir.wav";
    controlAOnlyPath_ = std::string(TEST_DATA_DIR) + "/CONTROL_amp_output_with_ir_a_only.wav";
    controlEvenBlendPath_ =
        std::string(TEST_DATA_DIR) + "/CONTROL_amp_output_with_ir_a_b_even_blend.wav";
  }

  std::string irAPath_;
  std::string irBPath_;
  std::string dryPath_;
  std::string controlAOnlyPath_;
  std::string controlEvenBlendPath_;
};

// Scenario: IR A loaded and enabled, IR B not loaded and disabled, static mode.
//
// Input:   INPUT_amp_output_no_ir.wav (44.1 kHz mono, dry amp signal)
// IR A:    INPUT_ir_a.wav (96 kHz mono, resampled to 44.1 kHz internally)
// Control: CONTROL_amp_output_with_ir_a_only.wav (44.1 kHz mono, reference from a
//          known-good IR loader, 100% wet signal)
//
// Preconditions:
//   - IR A is loaded and enabled. IR B is not loaded and disabled.
//   - blend is left at its default (0.0f). When IR B is disabled, blend is irrelevant
//     because slot B is out of the signal path entirely; the processor must output
//     100% IR A wet regardless of blend position.
//   - Output gain is 0 dB (default). Trim gains are 0 dB (default).
//
// Verifies:
//   1. IR loading and 96 kHz -> 44.1 kHz resampling produce non-silent output.
//   2. Convolved output matches the reference (Pearson r > 0.99 after normalizing
//      both to unit peak to remove the -18 dB IR compensation gain offset).
//   3. Blend is correctly ignored when IR B is disabled.
TEST_F(ComponentTest, ConvolutionMatchesReference_IrAOnly)
{
  unsigned int drySampleRate = 0;
  drwav_uint64 dryFrameCount = 0;
  std::vector<float> dryInput = loadWavMono(dryPath_, drySampleRate, dryFrameCount);
  ASSERT_FALSE(dryInput.empty()) << "Failed to load dry input: " << dryPath_;
  ASSERT_EQ(drySampleRate, 44100u);

  unsigned int controlSampleRate = 0;
  drwav_uint64 controlFrameCount = 0;
  std::vector<float> controlOutput =
      loadWavMono(controlAOnlyPath_, controlSampleRate, controlFrameCount);
  ASSERT_FALSE(controlOutput.empty()) << "Failed to load control: " << controlAOnlyPath_;

  IRProcessor processor;
  processor.setSampleRate(44100.0);
  processor.setMaxBlockSize(512);
  // blend left at default (0.0f); with IR B disabled, blend must be irrelevant.
  processor.setIRBEnabled(false);

  std::string errMsg;
  ASSERT_TRUE(processor.loadImpulseResponse1(irAPath_, errMsg)) << "IR A load failed: " << errMsg;

  constexpr int kBlockSize = 512;
  std::vector<float> actualOutput = processAndAlign(processor, dryInput, kBlockSize);

  writeWavMono(::testing::TempDir() + "octobir_component_ir_a_only.wav", actualOutput,
               drySampleRate);

  float peak = 0.0f;
  for (float s : actualOutput)
    peak = std::max(peak, std::abs(s));
  ASSERT_GT(peak, 1e-6f) << "Output is silent after IR convolution; likely a resampling failure. "
                         << "Latency reported: " << processor.getLatencySamples() << " samples.";

  size_t compareLen = std::min(actualOutput.size(), controlOutput.size());
  actualOutput.resize(compareLen);
  controlOutput.resize(compareLen);

  normalizeToUnitPeak(actualOutput);
  normalizeToUnitPeak(controlOutput);

  constexpr int kMaxLagSamples = 200;
  const int lag = findAlignmentLag(actualOutput, controlOutput, kMaxLagSamples);

  std::vector<float> aAligned, bAligned;
  if (lag >= 0)
  {
    const size_t skip = static_cast<size_t>(lag);
    const size_t len = std::min(actualOutput.size(), controlOutput.size() - skip);
    aAligned.assign(actualOutput.begin(), actualOutput.begin() + static_cast<ptrdiff_t>(len));
    bAligned.assign(controlOutput.begin() + static_cast<ptrdiff_t>(lag),
                    controlOutput.begin() + static_cast<ptrdiff_t>(lag + len));
  }
  else
  {
    const size_t skip = static_cast<size_t>(-lag);
    const size_t len = std::min(actualOutput.size() - skip, controlOutput.size());
    aAligned.assign(actualOutput.begin() + static_cast<ptrdiff_t>(-lag),
                    actualOutput.begin() + static_cast<ptrdiff_t>(-lag + len));
    bAligned.assign(controlOutput.begin(), controlOutput.begin() + static_cast<ptrdiff_t>(len));
  }

  double snrDb = computeSnrDb(bAligned, aAligned);
  double r = pearsonCorrelation(aAligned, bAligned);

  std::cout << "[IrAOnly] Latency: " << processor.getLatencySamples() << " samples\n";
  std::cout << "[IrAOnly] Alignment lag: " << lag << " samples\n";
  std::cout << "[IrAOnly] SNR (at best lag): " << snrDb << " dB\n";
  std::cout << "[IrAOnly] Pearson r (at best lag): " << r << "\n";
  std::cout << "[IrAOnly] Diagnostic output: " << ::testing::TempDir()
            << "octobir_component_ir_a_only.wav\n";

  EXPECT_GT(r, 0.99) << "Correlation too low (r=" << r << ", SNR=" << snrDb << " dB, lag=" << lag
                     << " samples). "
                     << "Listen to " << ::testing::TempDir()
                     << "octobir_component_ir_a_only.wav for diagnosis.";
}

// Scenario: IR A and IR B both loaded and enabled, equal-power blend (blend=0.0f), static mode.
//
// Input:   INPUT_amp_output_no_ir.wav (44.1 kHz mono, dry amp signal)
// IR A:    INPUT_ir_a.wav (resampled to 44.1 kHz internally)
// IR B:    INPUT_ir_b.wav (resampled to 44.1 kHz internally)
// Control: CONTROL_amp_output_with_ir_a_b_even_blend.wav (44.1 kHz mono, reference from a
//          known-good IR loader, 50/50 equal-power blend of IR A and IR B)
//
// Preconditions:
//   - IR A and IR B are both loaded and enabled (default state).
//   - blend is left at its default (0.0f), which produces a 50/50 equal-power mix:
//     gain_a = gain_b = sqrt(0.5).
//   - Output gain is 0 dB (default). Trim gains are 0 dB (default).
//
// Verifies:
//   1. Both IRs load, resample, and convolve to produce non-silent output.
//   2. The blended output matches the reference (Pearson r > 0.99 after normalizing
//      both to unit peak to remove the -18 dB IR compensation gain offset).
TEST_F(ComponentTest, ConvolutionMatchesReference_EvenBlend)
{
  unsigned int drySampleRate = 0;
  drwav_uint64 dryFrameCount = 0;
  std::vector<float> dryInput = loadWavMono(dryPath_, drySampleRate, dryFrameCount);
  ASSERT_FALSE(dryInput.empty()) << "Failed to load dry input: " << dryPath_;
  ASSERT_EQ(drySampleRate, 44100u);

  unsigned int controlSampleRate = 0;
  drwav_uint64 controlFrameCount = 0;
  std::vector<float> controlOutput =
      loadWavMono(controlEvenBlendPath_, controlSampleRate, controlFrameCount);
  ASSERT_FALSE(controlOutput.empty()) << "Failed to load control: " << controlEvenBlendPath_;

  IRProcessor processor;
  processor.setSampleRate(44100.0);
  processor.setMaxBlockSize(512);
  // Both slots enabled (default); blend=0.0f (default) = 50/50 equal-power.

  std::string errMsg;
  ASSERT_TRUE(processor.loadImpulseResponse1(irAPath_, errMsg)) << "IR A load failed: " << errMsg;
  ASSERT_TRUE(processor.loadImpulseResponse2(irBPath_, errMsg)) << "IR B load failed: " << errMsg;

  constexpr int kBlockSize = 512;
  std::vector<float> actualOutput = processAndAlign(processor, dryInput, kBlockSize);

  writeWavMono(::testing::TempDir() + "octobir_component_even_blend.wav", actualOutput,
               drySampleRate);

  float peak = 0.0f;
  for (float s : actualOutput)
    peak = std::max(peak, std::abs(s));
  ASSERT_GT(peak, 1e-6f) << "Output is silent after IR convolution; likely a resampling failure. "
                         << "Latency reported: " << processor.getLatencySamples() << " samples.";

  size_t compareLen = std::min(actualOutput.size(), controlOutput.size());
  actualOutput.resize(compareLen);
  controlOutput.resize(compareLen);

  normalizeToUnitPeak(actualOutput);
  normalizeToUnitPeak(controlOutput);

  constexpr int kMaxLagSamples = 200;
  const int lag = findAlignmentLag(actualOutput, controlOutput, kMaxLagSamples);

  std::vector<float> aAligned, bAligned;
  if (lag >= 0)
  {
    const size_t skip = static_cast<size_t>(lag);
    const size_t len = std::min(actualOutput.size(), controlOutput.size() - skip);
    aAligned.assign(actualOutput.begin(), actualOutput.begin() + static_cast<ptrdiff_t>(len));
    bAligned.assign(controlOutput.begin() + static_cast<ptrdiff_t>(lag),
                    controlOutput.begin() + static_cast<ptrdiff_t>(lag + len));
  }
  else
  {
    const size_t skip = static_cast<size_t>(-lag);
    const size_t len = std::min(actualOutput.size() - skip, controlOutput.size());
    aAligned.assign(actualOutput.begin() + static_cast<ptrdiff_t>(-lag),
                    actualOutput.begin() + static_cast<ptrdiff_t>(-lag + len));
    bAligned.assign(controlOutput.begin(), controlOutput.begin() + static_cast<ptrdiff_t>(len));
  }

  double snrDb = computeSnrDb(bAligned, aAligned);
  double r = pearsonCorrelation(aAligned, bAligned);

  std::cout << "[EvenBlend] Latency: " << processor.getLatencySamples() << " samples\n";
  std::cout << "[EvenBlend] Alignment lag: " << lag << " samples\n";
  std::cout << "[EvenBlend] SNR (at best lag): " << snrDb << " dB\n";
  std::cout << "[EvenBlend] Pearson r (at best lag): " << r << "\n";
  std::cout << "[EvenBlend] Diagnostic output: " << ::testing::TempDir()
            << "octobir_component_even_blend.wav\n";

  EXPECT_GT(r, 0.99) << "Correlation too low (r=" << r << ", SNR=" << snrDb << " dB, lag=" << lag
                     << " samples). "
                     << "Listen to " << ::testing::TempDir()
                     << "octobir_component_even_blend.wav for diagnosis.";
}

// Scenario: Equal-power blend is commutative — swapping which IR occupies slot 1 vs slot 2
// must not change the output when blend=0.0f.
//
// Input:   INPUT_amp_output_no_ir.wav (44.1 kHz mono, dry amp signal)
// IR A:    INPUT_ir_a.wav
// IR B:    INPUT_ir_b.wav
//
// Preconditions:
//   - Pre-swap processor: IR A in slot 1, IR B in slot 2, blend=0.0f.
//   - Post-swap processor: IR B in slot 1, IR A in slot 2, blend=0.0f.
//   - At blend=0.0f: gain_A = gain_B = sqrt(0.5). The math reduces to
//     sqrt(0.5)*conv(x,A) + sqrt(0.5)*conv(x,B), which is slot-order invariant.
//
// Verifies:
//   1. Both configurations produce non-silent output.
//   2. Pearson r between the two outputs is > 0.9999. This threshold is intentionally
//      tight: the outputs are a mathematical identity under a correct implementation,
//      so any meaningful divergence indicates a bug in the dual-IR signal path.
TEST_F(ComponentTest, ConvolutionIsSymmetric_AfterIrSwap)
{
  unsigned int drySampleRate = 0;
  drwav_uint64 dryFrameCount = 0;
  std::vector<float> dryInput = loadWavMono(dryPath_, drySampleRate, dryFrameCount);
  ASSERT_FALSE(dryInput.empty()) << "Failed to load dry input: " << dryPath_;
  ASSERT_EQ(drySampleRate, 44100u);

  constexpr int kBlockSize = 512;

  std::vector<float> outputAB;
  {
    IRProcessor p;
    p.setSampleRate(44100.0);
    p.setMaxBlockSize(kBlockSize);
    std::string err;
    ASSERT_TRUE(p.loadImpulseResponse1(irAPath_, err)) << "IR A load failed: " << err;
    ASSERT_TRUE(p.loadImpulseResponse2(irBPath_, err)) << "IR B load failed: " << err;
    outputAB = processAndAlign(p, dryInput, kBlockSize);
    writeWavMono(::testing::TempDir() + "octobir_swap_ab.wav", outputAB, drySampleRate);
    float peak = 0.0f;
    for (float s : outputAB)
      peak = std::max(peak, std::abs(s));
    ASSERT_GT(peak, 1e-6f) << "Pre-swap output is silent. Latency: " << p.getLatencySamples();
  }

  std::vector<float> outputBA;
  {
    IRProcessor p;
    p.setSampleRate(44100.0);
    p.setMaxBlockSize(kBlockSize);
    std::string err;
    ASSERT_TRUE(p.loadImpulseResponse1(irBPath_, err)) << "IR B load failed: " << err;
    ASSERT_TRUE(p.loadImpulseResponse2(irAPath_, err)) << "IR A load failed: " << err;
    outputBA = processAndAlign(p, dryInput, kBlockSize);
    writeWavMono(::testing::TempDir() + "octobir_swap_ba.wav", outputBA, drySampleRate);
    float peak = 0.0f;
    for (float s : outputBA)
      peak = std::max(peak, std::abs(s));
    ASSERT_GT(peak, 1e-6f) << "Post-swap output is silent. Latency: " << p.getLatencySamples();
  }

  const size_t compareLen = std::min(outputAB.size(), outputBA.size());
  outputAB.resize(compareLen);
  outputBA.resize(compareLen);

  const double snrDb = computeSnrDb(outputAB, outputBA);
  const double r = pearsonCorrelation(outputAB, outputBA);

  std::cout << "[SwapSymmetry] Pearson r: " << r << "\n";
  std::cout << "[SwapSymmetry] SNR: " << snrDb << " dB\n";
  std::cout << "[SwapSymmetry] Pre-swap:  " << ::testing::TempDir() << "octobir_swap_ab.wav\n";
  std::cout << "[SwapSymmetry] Post-swap: " << ::testing::TempDir() << "octobir_swap_ba.wav\n";

  EXPECT_GT(r, 0.9999) << "Swap symmetry violated (r=" << r << ", SNR=" << snrDb
                       << " dB). Output differs based on IR slot assignment. "
                       << "Compare " << ::testing::TempDir() << "octobir_swap_ab.wav and "
                       << ::testing::TempDir() << "octobir_swap_ba.wav.";
}

// Diagnostic: decomposes the blend into individual convolutions to isolate whether
// the mismatch is in the dual-IR processing path or in the blending formula vs the reference.
// Run manually with --gtest_also_run_disabled_tests when investigating blend discrepancies.
// Outputs (to ::testing::TempDir()): octobir_diag_A.wav, _B.wav, _manual_blend.wav
TEST_F(ComponentTest, DISABLED_DiagnoseBlendDecomposition)
{
  unsigned int drySampleRate = 0;
  drwav_uint64 dryFrameCount = 0;
  std::vector<float> dryInput = loadWavMono(dryPath_, drySampleRate, dryFrameCount);
  ASSERT_FALSE(dryInput.empty());

  unsigned int controlSampleRate = 0;
  drwav_uint64 controlFrameCount = 0;
  std::vector<float> controlOutput =
      loadWavMono(controlEvenBlendPath_, controlSampleRate, controlFrameCount);
  ASSERT_FALSE(controlOutput.empty());

  constexpr int kBlockSize = 512;

  // IR A convolution via single-IR path.
  std::vector<float> aOutput;
  {
    IRProcessor p;
    p.setSampleRate(44100.0);
    p.setMaxBlockSize(kBlockSize);
    p.setIRBEnabled(false);
    std::string err;
    ASSERT_TRUE(p.loadImpulseResponse1(irAPath_, err)) << err;
    aOutput = processAndAlign(p, dryInput, kBlockSize);
    writeWavMono(::testing::TempDir() + "octobir_diag_A.wav", aOutput, drySampleRate);
  }

  // IR B convolution via single-IR path.
  std::vector<float> bOutput;
  {
    IRProcessor p;
    p.setSampleRate(44100.0);
    p.setMaxBlockSize(kBlockSize);
    p.setIRAEnabled(false);
    std::string err;
    ASSERT_TRUE(p.loadImpulseResponse2(irBPath_, err)) << err;
    bOutput = processAndAlign(p, dryInput, kBlockSize);
    writeWavMono(::testing::TempDir() + "octobir_diag_B.wav", bOutput, drySampleRate);
  }

  // Manual equal-power blend of the two single-IR outputs.
  const size_t blendLen = std::min(aOutput.size(), bOutput.size());
  std::vector<float> manualBlend(blendLen);
  for (size_t i = 0; i < blendLen; ++i)
    manualBlend[i] = std::sqrt(0.5f) * aOutput[i] + std::sqrt(0.5f) * bOutput[i];
  writeWavMono(::testing::TempDir() + "octobir_diag_manual_blend.wav", manualBlend, drySampleRate);

  // Dual-IR path output (same as ConvolutionMatchesReference_EvenBlend).
  std::vector<float> dualOutput;
  {
    IRProcessor p;
    p.setSampleRate(44100.0);
    p.setMaxBlockSize(kBlockSize);
    std::string err;
    ASSERT_TRUE(p.loadImpulseResponse1(irAPath_, err)) << err;
    ASSERT_TRUE(p.loadImpulseResponse2(irBPath_, err)) << err;
    dualOutput = processAndAlign(p, dryInput, kBlockSize);
  }

  auto reportCorrelation =
      [](const std::string& label, std::vector<float> sig, std::vector<float> ref)
  {
    size_t len = std::min(sig.size(), ref.size());
    sig.resize(len);
    ref.resize(len);
    normalizeToUnitPeak(sig);
    normalizeToUnitPeak(ref);
    const int lag = findAlignmentLag(sig, ref, 200);

    std::vector<float> a, b;
    if (lag >= 0)
    {
      const size_t skip = static_cast<size_t>(lag);
      const size_t l = std::min(sig.size(), ref.size() - skip);
      a.assign(sig.begin(), sig.begin() + static_cast<ptrdiff_t>(l));
      b.assign(ref.begin() + static_cast<ptrdiff_t>(lag),
               ref.begin() + static_cast<ptrdiff_t>(lag + l));
    }
    else
    {
      const size_t skip = static_cast<size_t>(-lag);
      const size_t l = std::min(sig.size() - skip, ref.size());
      a.assign(sig.begin() + static_cast<ptrdiff_t>(-lag),
               sig.begin() + static_cast<ptrdiff_t>(-lag + l));
      b.assign(ref.begin(), ref.begin() + static_cast<ptrdiff_t>(l));
    }

    double r = pearsonCorrelation(a, b);
    double snr = computeSnrDb(b, a);
    std::cout << "[Diagnose] " << label << ": lag=" << lag << ", r=" << r << ", SNR=" << snr
              << " dB\n";
  };

  reportCorrelation("manualBlend vs control", manualBlend, controlOutput);
  reportCorrelation("dualOutput  vs control", dualOutput, controlOutput);
  reportCorrelation("dualOutput  vs manualBlend", dualOutput, manualBlend);
  reportCorrelation("aOutput     vs control", aOutput, controlOutput);
  reportCorrelation("bOutput     vs control", bOutput, controlOutput);
}
