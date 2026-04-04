#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

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

class BlendExtremeTest : public ::testing::Test
{
 protected:
  void SetUp() override
  {
    irAPath_ = std::string(TEST_DATA_DIR) + "/INPUT_ir_a.wav";
    irBPath_ = std::string(TEST_DATA_DIR) + "/INPUT_ir_b.wav";
    dryPath_ = std::string(TEST_DATA_DIR) + "/INPUT_amp_output_no_ir.wav";
  }

  std::string irAPath_;
  std::string irBPath_;
  std::string dryPath_;
};

// blend=-1.0 with both IRs loaded: gain_A=1.0, gain_B=0.0.
// Output should match what you get from the single-IR-A path (IR B disabled).
// This is a self-consistency test — no external reference needed.
TEST_F(BlendExtremeTest, FullyIRA_MatchesSingleIRPath)
{
  unsigned int drySampleRate = 0;
  drwav_uint64 dryFrameCount = 0;
  std::vector<float> dryInput = loadWavMono(dryPath_, drySampleRate, dryFrameCount);
  ASSERT_FALSE(dryInput.empty());
  ASSERT_EQ(drySampleRate, 44100u);

  constexpr int kBlockSize = 512;

  // Single-IR path: only IR A loaded, B disabled.
  std::vector<float> singleIROutput;
  {
    IRProcessor p;
    p.setSampleRate(44100.0);
    p.setMaxBlockSize(kBlockSize);
    p.setIRBEnabled(false);
    std::string err;
    ASSERT_TRUE(p.loadImpulseResponse1(irAPath_, err)) << err;
    singleIROutput = processAndAlign(p, dryInput, kBlockSize);
  }

  // Dual-IR path: both loaded, blend=-1.0 fully to IR A.
  std::vector<float> dualIROutput;
  {
    IRProcessor p;
    p.setSampleRate(44100.0);
    p.setMaxBlockSize(kBlockSize);
    p.setBlend(-1.0f);
    std::string err;
    ASSERT_TRUE(p.loadImpulseResponse1(irAPath_, err)) << err;
    ASSERT_TRUE(p.loadImpulseResponse2(irBPath_, err)) << err;
    dualIROutput = processAndAlign(p, dryInput, kBlockSize);
  }

  float peakSingle = 0.0f, peakDual = 0.0f;
  for (float s : singleIROutput)
    peakSingle = std::max(peakSingle, std::abs(s));
  for (float s : dualIROutput)
    peakDual = std::max(peakDual, std::abs(s));
  ASSERT_GT(peakSingle, 1e-6f) << "Single-IR output is silent";
  ASSERT_GT(peakDual, 1e-6f) << "Dual-IR output is silent";

  size_t compareLen = std::min(singleIROutput.size(), dualIROutput.size());
  singleIROutput.resize(compareLen);
  dualIROutput.resize(compareLen);

  normalizeToUnitPeak(singleIROutput);
  normalizeToUnitPeak(dualIROutput);

  double r = pearsonCorrelation(singleIROutput, dualIROutput);

  std::cout << "[BlendFullA] Pearson r (single-IR vs dual-IR blend=-1): " << r << "\n";

  // Both paths should produce nearly identical output. The small tolerance accounts
  // for the dual-IR path running a second convolution engine whose output is
  // multiplied by 0 (no mathematical contribution, but engine state differs).
  EXPECT_GT(r, 0.999) << "At blend=-1.0, dual-IR output should match single-IR-A output (r=" << r
                      << "). IR B is not being fully muted.";
}

// IR A loaded, slot B enabled but empty, blend=+1.0 (fully toward empty slot).
// The empty-but-enabled slot should act as dry passthrough, so the output must
// match the latency-compensated dry input.
TEST_F(BlendExtremeTest, IRALoaded_SlotBEnabledEmpty_BlendFullB_OutputMatchesDry)
{
  unsigned int drySampleRate = 0;
  drwav_uint64 dryFrameCount = 0;
  std::vector<float> dryInput = loadWavMono(dryPath_, drySampleRate, dryFrameCount);
  ASSERT_FALSE(dryInput.empty());
  ASSERT_EQ(drySampleRate, 44100u);

  constexpr int kBlockSize = 512;

  IRProcessor p;
  p.setSampleRate(44100.0);
  p.setMaxBlockSize(kBlockSize);
  p.setIRAEnabled(true);
  p.setIRBEnabled(true);
  p.setBlend(1.0f);
  std::string err;
  ASSERT_TRUE(p.loadImpulseResponse1(irAPath_, err)) << err;

  std::vector<float> output = processAndAlign(p, dryInput, kBlockSize);

  float peakOutput = 0.0f;
  for (float s : output)
    peakOutput = std::max(peakOutput, std::abs(s));
  ASSERT_GT(peakOutput, 1e-6f)
      << "Output is silent — empty-but-enabled slot should produce dry passthrough, not silence";

  size_t compareLen = std::min(output.size(), dryInput.size());
  output.resize(compareLen);
  std::vector<float> dryRef(dryInput.begin(),
                            dryInput.begin() + static_cast<ptrdiff_t>(compareLen));

  normalizeToUnitPeak(output);
  normalizeToUnitPeak(dryRef);

  double r = pearsonCorrelation(output, dryRef);

  std::cout << "[EmptySlotB] Pearson r (output vs dry input): " << r << "\n";

  EXPECT_GT(r, 0.999)
      << "With blend fully toward empty-but-enabled slot B, output should match dry input (r=" << r
      << ")";
}

// IR B loaded, slot A enabled but empty, blend=-1.0 (fully toward empty slot).
// Same as above but mirrored: empty slot A should act as dry passthrough.
TEST_F(BlendExtremeTest, IRBLoaded_SlotAEnabledEmpty_BlendFullA_OutputMatchesDry)
{
  unsigned int drySampleRate = 0;
  drwav_uint64 dryFrameCount = 0;
  std::vector<float> dryInput = loadWavMono(dryPath_, drySampleRate, dryFrameCount);
  ASSERT_FALSE(dryInput.empty());
  ASSERT_EQ(drySampleRate, 44100u);

  constexpr int kBlockSize = 512;

  IRProcessor p;
  p.setSampleRate(44100.0);
  p.setMaxBlockSize(kBlockSize);
  p.setIRAEnabled(true);
  p.setIRBEnabled(true);
  p.setBlend(-1.0f);
  std::string err;
  ASSERT_TRUE(p.loadImpulseResponse2(irBPath_, err)) << err;

  std::vector<float> output = processAndAlign(p, dryInput, kBlockSize);

  float peakOutput = 0.0f;
  for (float s : output)
    peakOutput = std::max(peakOutput, std::abs(s));
  ASSERT_GT(peakOutput, 1e-6f)
      << "Output is silent — empty-but-enabled slot should produce dry passthrough, not silence";

  size_t compareLen = std::min(output.size(), dryInput.size());
  output.resize(compareLen);
  std::vector<float> dryRef(dryInput.begin(),
                            dryInput.begin() + static_cast<ptrdiff_t>(compareLen));

  normalizeToUnitPeak(output);
  normalizeToUnitPeak(dryRef);

  double r = pearsonCorrelation(output, dryRef);

  std::cout << "[EmptySlotA] Pearson r (output vs dry input): " << r << "\n";

  EXPECT_GT(r, 0.999)
      << "With blend fully toward empty-but-enabled slot A, output should match dry input (r=" << r
      << ")";
}

// blend=+1.0: gain_A=0.0, gain_B=1.0.
// Output should match what you get from the single-IR-B path (IR A disabled).
TEST_F(BlendExtremeTest, FullyIRB_MatchesSingleIRBPath)
{
  unsigned int drySampleRate = 0;
  drwav_uint64 dryFrameCount = 0;
  std::vector<float> dryInput = loadWavMono(dryPath_, drySampleRate, dryFrameCount);
  ASSERT_FALSE(dryInput.empty());

  constexpr int kBlockSize = 512;

  // Single-IR path: only IR B loaded, A disabled.
  std::vector<float> singleIROutput;
  {
    IRProcessor p;
    p.setSampleRate(44100.0);
    p.setMaxBlockSize(kBlockSize);
    p.setIRAEnabled(false);
    std::string err;
    ASSERT_TRUE(p.loadImpulseResponse2(irBPath_, err)) << err;
    singleIROutput = processAndAlign(p, dryInput, kBlockSize);
  }

  // Dual-IR path: both loaded, blend=+1.0 fully to IR B.
  std::vector<float> dualIROutput;
  {
    IRProcessor p;
    p.setSampleRate(44100.0);
    p.setMaxBlockSize(kBlockSize);
    p.setBlend(1.0f);
    std::string err;
    ASSERT_TRUE(p.loadImpulseResponse1(irAPath_, err)) << err;
    ASSERT_TRUE(p.loadImpulseResponse2(irBPath_, err)) << err;
    dualIROutput = processAndAlign(p, dryInput, kBlockSize);
  }

  float peakSingle = 0.0f, peakDual = 0.0f;
  for (float s : singleIROutput)
    peakSingle = std::max(peakSingle, std::abs(s));
  for (float s : dualIROutput)
    peakDual = std::max(peakDual, std::abs(s));
  ASSERT_GT(peakSingle, 1e-6f) << "Single-IR-B output is silent";
  ASSERT_GT(peakDual, 1e-6f) << "Dual-IR output is silent";

  size_t compareLen = std::min(singleIROutput.size(), dualIROutput.size());
  singleIROutput.resize(compareLen);
  dualIROutput.resize(compareLen);

  normalizeToUnitPeak(singleIROutput);
  normalizeToUnitPeak(dualIROutput);

  double r = pearsonCorrelation(singleIROutput, dualIROutput);

  std::cout << "[BlendFullB] Pearson r (single-IR-B vs dual-IR blend=+1): " << r << "\n";

  EXPECT_GT(r, 0.999) << "At blend=+1.0, dual-IR output should match single-IR-B output (r=" << r
                      << "). IR A is not being fully muted.";
}
