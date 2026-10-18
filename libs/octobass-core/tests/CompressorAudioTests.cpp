#include <gtest/gtest.h>

#include <cmath>
#include <vector>

#include "octobass-core/Compressor.hpp"

using namespace octob;

namespace
{

constexpr int kBlockSize = 512;

std::vector<float> generateSine(float freqHz, float sampleRate, size_t numSamples,
                                float amplitude = 0.5f)
{
  std::vector<float> buf(numSamples);
  for (size_t i = 0; i < numSamples; ++i)
    buf[i] = amplitude * static_cast<float>(std::sin(2.0 * Pi * freqHz * i / sampleRate));
  return buf;
}

float peakLevel(const std::vector<float>& buf)
{
  float peak = 0.0f;
  for (float s : buf)
    peak = std::max(peak, std::abs(s));
  return peak;
}

double computeRMS(const std::vector<float>& buf, size_t start, size_t len)
{
  double sum = 0.0;
  for (size_t i = start; i < start + len; ++i)
    sum += static_cast<double>(buf[i]) * buf[i];
  return std::sqrt(sum / static_cast<double>(len));
}

std::vector<float> processInBlocks(Compressor& comp, const std::vector<float>& input, int blockSize)
{
  std::vector<float> output(input.size());
  size_t pos = 0;
  while (pos < input.size())
  {
    size_t toProcess = std::min(static_cast<size_t>(blockSize), input.size() - pos);
    comp.process(input.data() + pos, output.data() + pos, toProcess);
    pos += toProcess;
  }
  return output;
}

}  // namespace

class CompressorAudioTest : public ::testing::Test
{
 protected:
  void SetUp() override { comp.setSampleRate(44100.0); }

  Compressor comp;
};

TEST_F(CompressorAudioTest, SquashFull_ReducesPeak)
{
  constexpr size_t kNumSamples = 16384;
  constexpr size_t kSkip = 4096;
  auto input = generateSine(80.0f, 44100.0f, kNumSamples, 0.5f);
  float inputPeak = peakLevel(input);

  for (int mode = 0; mode < NumCompressionModes; ++mode)
  {
    comp.reset();
    comp.setMode(mode);
    comp.setSquash(1.0f);

    auto output = processInBlocks(comp, input, kBlockSize);

    float outputPeak = 0.0f;
    for (size_t i = kSkip; i < kNumSamples; ++i)
      outputPeak = std::max(outputPeak, std::abs(output[i]));

    EXPECT_GT(outputPeak, 1e-6f) << "Mode " << mode << " should produce non-silent output";
    EXPECT_LT(outputPeak, inputPeak)
        << "Mode " << mode << " should reduce peak level at squash=1.0";
    EXPECT_LT(comp.getGainReductionDb(), -0.1f)
        << "Mode " << mode << " should have measurable gain reduction at squash=1.0";
  }
}

TEST_F(CompressorAudioTest, ModesHaveDifferentCharacteristics)
{
  constexpr size_t kNumSamples = 16384;
  constexpr size_t kSkip = 8192;
  auto input = generateSine(80.0f, 44100.0f, kNumSamples, 0.5f);

  std::vector<float> peaks(NumCompressionModes);
  std::vector<double> rmsValues(NumCompressionModes);

  for (int mode = 0; mode < NumCompressionModes; ++mode)
  {
    comp.reset();
    comp.setMode(mode);
    comp.setSquash(1.0f);

    auto output = processInBlocks(comp, input, kBlockSize);
    peaks[mode] = 0.0f;
    for (size_t i = kSkip; i < kNumSamples; ++i)
      peaks[mode] = std::max(peaks[mode], std::abs(output[i]));
    rmsValues[mode] = computeRMS(output, kSkip, kNumSamples - kSkip);
  }

  int distinctPairs = 0;
  for (int a = 0; a < NumCompressionModes; ++a)
  {
    for (int b = a + 1; b < NumCompressionModes; ++b)
    {
      double peakDiffDb =
          20.0 * std::log10(std::max(1e-10f, peaks[a]) / std::max(1e-10f, peaks[b]));
      double rmsDiffDb =
          20.0 * std::log10(std::max(1e-10, rmsValues[a]) / std::max(1e-10, rmsValues[b]));
      if (std::abs(peakDiffDb) > 0.5 || std::abs(rmsDiffDb) > 0.5)
        ++distinctPairs;
    }
  }
  EXPECT_GE(distinctPairs, 2) << "Modes should produce measurably distinct outputs";
}

TEST_F(CompressorAudioTest, GainReduction_IncreasesWithSquash)
{
  constexpr size_t kNumSamples = 16384;
  auto input = generateSine(80.0f, 44100.0f, kNumSamples, 0.5f);

  for (int mode = 0; mode < NumCompressionModes; ++mode)
  {
    float prevGR = 0.0f;
    for (float squash : {0.25f, 0.5f, 0.75f, 1.0f})
    {
      comp.reset();
      comp.setMode(mode);
      comp.setSquash(squash);

      processInBlocks(comp, input, kBlockSize);
      float gr = comp.getGainReductionDb();

      EXPECT_LE(gr, prevGR + 0.1f)
          << "Mode " << mode << " squash=" << squash
          << ": gain reduction should increase (become more negative) with squash";
      prevGR = gr;
    }
  }
}

TEST_F(CompressorAudioTest, AllModes_StableAtExtremes)
{
  constexpr size_t kNumSamples = 44100;
  auto loud = generateSine(60.0f, 44100.0f, kNumSamples, 0.95f);

  for (int mode = 0; mode < NumCompressionModes; ++mode)
  {
    Compressor c;
    c.setSampleRate(44100.0);
    c.setMode(mode);
    c.setSquash(1.0f);

    auto output = processInBlocks(c, loud, kBlockSize);

    for (size_t i = 0; i < kNumSamples; ++i)
    {
      EXPECT_FALSE(std::isnan(output[i])) << "Mode " << mode << " produced NaN at " << i;
      EXPECT_FALSE(std::isinf(output[i])) << "Mode " << mode << " produced Inf at " << i;
    }

    float peak = peakLevel(output);
    EXPECT_LT(peak, 1.0f) << "Mode " << mode << " output should not exceed unity with loud input";
  }
}

TEST_F(CompressorAudioTest, FullSquash_ReducesOutputLevel)
{
  constexpr size_t kNumSamples = 88200;
  constexpr size_t kSkip = 44100;
  auto input = generateSine(80.0f, 44100.0f, kNumSamples, 0.5f);
  double inputRMS = computeRMS(input, kSkip, kNumSamples - kSkip);

  for (int mode = 0; mode < NumCompressionModes; ++mode)
  {
    comp.reset();
    comp.setMode(mode);
    comp.setSquash(1.0f);

    auto output = processInBlocks(comp, input, kBlockSize);
    double outputRMS = computeRMS(output, kSkip, kNumSamples - kSkip);

    double reductionDb = 20.0 * std::log10(outputRMS / inputRMS);

    // All modes should produce significant reduction with the aggressive tuning.
    // Feed-forward (VCA, Bus) will compress harder than feedback (Opto, FET),
    // but all should hit at least 3dB on a 0.5 amplitude signal.
    EXPECT_LT(reductionDb, -3.0) << "Mode " << mode
                                 << " at squash=1.0: output RMS should be significantly "
                                 << "lower than input (got " << reductionDb << " dB)";
  }
}

TEST_F(CompressorAudioTest, NoOvershoot_OnTransientReentry)
{
  constexpr float kSampleRate = 44100.0f;
  constexpr size_t kBurstLen = static_cast<size_t>(kSampleRate * 1.5f);
  constexpr size_t kSilenceLen = static_cast<size_t>(kSampleRate * 0.1f);
  constexpr size_t kReburst = static_cast<size_t>(kSampleRate * 0.5f);
  constexpr size_t kTotal = kBurstLen + kSilenceLen + kReburst;
  constexpr float kAmplitude = 0.6f;

  std::vector<float> input(kTotal, 0.0f);
  for (size_t i = 0; i < kBurstLen; ++i)
    input[i] = kAmplitude *
               static_cast<float>(std::sin(2.0 * Pi * 80.0 * static_cast<double>(i) / kSampleRate));
  for (size_t i = kBurstLen + kSilenceLen; i < kTotal; ++i)
  {
    size_t j = i - kBurstLen - kSilenceLen;
    input[i] = kAmplitude *
               static_cast<float>(std::sin(2.0 * Pi * 80.0 * static_cast<double>(j) / kSampleRate));
  }

  float inputPeak = peakLevel(input);

  for (int mode = 0; mode < NumCompressionModes; ++mode)
  {
    Compressor c;
    c.setSampleRate(static_cast<double>(kSampleRate));
    c.setMode(mode);
    c.setSquash(1.0f);

    auto output = processInBlocks(c, input, kBlockSize);
    float outputPeak = peakLevel(output);

    EXPECT_LE(outputPeak, inputPeak * 1.01f)
        << "Mode " << mode << " output peak (" << outputPeak << ") exceeds input peak ("
        << inputPeak << ") -- compressor should only attenuate";
  }
}

TEST_F(CompressorAudioTest, HalfSquash_ProducesModerateReduction)
{
  constexpr size_t kNumSamples = 44100;
  constexpr size_t kSkip = 22050;
  auto input = generateSine(80.0f, 44100.0f, kNumSamples, 0.5f);
  double inputRMS = computeRMS(input, kSkip, kNumSamples - kSkip);

  for (int mode = 0; mode < NumCompressionModes; ++mode)
  {
    comp.reset();
    comp.setMode(mode);
    comp.setSquash(0.5f);

    auto output = processInBlocks(comp, input, kBlockSize);
    double outputRMS = computeRMS(output, kSkip, kNumSamples - kSkip);

    double reductionDb = 20.0 * std::log10(outputRMS / inputRMS);

    // At half squash, all modes should produce at least some measurable reduction.
    EXPECT_LT(reductionDb, -0.5) << "Mode " << mode
                                 << " at squash=0.5: should produce at least moderate "
                                 << "compression (got " << reductionDb << " dB)";
  }
}
