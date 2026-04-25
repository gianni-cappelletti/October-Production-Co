#include <gtest/gtest.h>

#include <cmath>
#include <vector>

#include "octobass-core/GraphicEQ.hpp"

using namespace octob;

namespace
{

constexpr double kPi = 3.14159265358979323846;

std::vector<float> generateSine(float freqHz, float sampleRate, size_t numSamples,
                                float amplitude = 1.0f)
{
  std::vector<float> buf(numSamples);
  for (size_t i = 0; i < numSamples; ++i)
    buf[i] = amplitude * static_cast<float>(std::sin(2.0 * kPi * freqHz * i / sampleRate));
  return buf;
}

double computeRMS(const std::vector<float>& buf, size_t start = 0, size_t len = 0)
{
  if (len == 0)
    len = buf.size() - start;
  double sum = 0.0;
  for (size_t i = start; i < start + len; ++i)
    sum += static_cast<double>(buf[i]) * buf[i];
  return std::sqrt(sum / static_cast<double>(len));
}

}  // namespace

class GraphicEQTest : public ::testing::Test
{
 protected:
  void SetUp() override { eq.setSampleRate(44100.0); }

  GraphicEQ eq;

  // Band 14 has center frequency 1000 Hz
  static constexpr int kTestBand = 14;
  static constexpr float kTestBandFreq = 1000.0f;
};

TEST_F(GraphicEQTest, UnityPassThrough)
{
  constexpr size_t kNumSamples = 8192;
  constexpr size_t kSkip = 1024;

  auto input = generateSine(1000.0f, 44100.0f, kNumSamples);
  std::vector<float> output(kNumSamples);
  eq.process(input.data(), output.data(), kNumSamples);

  double inputRMS = computeRMS(input, kSkip, kNumSamples - kSkip);
  double outputRMS = computeRMS(output, kSkip, kNumSamples - kSkip);
  double ratioDb = 20.0 * std::log10(outputRMS / inputRMS);

  EXPECT_NEAR(ratioDb, 0.0, 0.01) << "All bands at 0dB should produce unity gain";
}

TEST_F(GraphicEQTest, SingleBandBoost)
{
  constexpr size_t kNumSamples = 16384;
  constexpr size_t kSkip = 4096;

  eq.setBandGain(kTestBand, 6.0f);

  auto input = generateSine(kTestBandFreq, 44100.0f, kNumSamples);
  std::vector<float> output(kNumSamples);
  eq.process(input.data(), output.data(), kNumSamples);

  double inputRMS = computeRMS(input, kSkip, kNumSamples - kSkip);
  double outputRMS = computeRMS(output, kSkip, kNumSamples - kSkip);
  double ratioDb = 20.0 * std::log10(outputRMS / inputRMS);

  EXPECT_NEAR(ratioDb, 6.0, 1.0) << "Band 14 at +6dB should boost 1kHz by ~6dB";
}

TEST_F(GraphicEQTest, SingleBandCut)
{
  constexpr size_t kNumSamples = 16384;
  constexpr size_t kSkip = 4096;

  eq.setBandGain(kTestBand, -6.0f);

  auto input = generateSine(kTestBandFreq, 44100.0f, kNumSamples);
  std::vector<float> output(kNumSamples);
  eq.process(input.data(), output.data(), kNumSamples);

  double inputRMS = computeRMS(input, kSkip, kNumSamples - kSkip);
  double outputRMS = computeRMS(output, kSkip, kNumSamples - kSkip);
  double ratioDb = 20.0 * std::log10(outputRMS / inputRMS);

  EXPECT_NEAR(ratioDb, -6.0, 1.0) << "Band 14 at -6dB should cut 1kHz by ~6dB";
}

TEST_F(GraphicEQTest, OutOfBandRejection)
{
  constexpr size_t kNumSamples = 16384;
  constexpr size_t kSkip = 4096;

  // Boost band 14 (1kHz) by 12dB, measure at 100Hz
  eq.setBandGain(kTestBand, 12.0f);

  auto input = generateSine(100.0f, 44100.0f, kNumSamples);
  std::vector<float> output(kNumSamples);
  eq.process(input.data(), output.data(), kNumSamples);

  double inputRMS = computeRMS(input, kSkip, kNumSamples - kSkip);
  double outputRMS = computeRMS(output, kSkip, kNumSamples - kSkip);
  double ratioDb = 20.0 * std::log10(outputRMS / inputRMS);

  EXPECT_NEAR(ratioDb, 0.0, 1.5) << "100Hz should be minimally affected by a 1kHz boost, got "
                                 << ratioDb << " dB";
}

TEST_F(GraphicEQTest, ProportionalQ_NarrowAtHighGain)
{
  constexpr size_t kNumSamples = 16384;
  constexpr size_t kSkip = 4096;

  eq.setBandGain(kTestBand, 12.0f);

  auto inputCenter = generateSine(kTestBandFreq, 44100.0f, kNumSamples);
  std::vector<float> outputCenter(kNumSamples);
  eq.process(inputCenter.data(), outputCenter.data(), kNumSamples);
  double centerBoost =
      20.0 * std::log10(computeRMS(outputCenter, kSkip) / computeRMS(inputCenter, kSkip));

  eq.reset();

  auto inputOctave = generateSine(2000.0f, 44100.0f, kNumSamples);
  std::vector<float> outputOctave(kNumSamples);
  eq.process(inputOctave.data(), outputOctave.data(), kNumSamples);
  double octaveBoost =
      20.0 * std::log10(computeRMS(outputOctave, kSkip) / computeRMS(inputOctave, kSkip));

  EXPECT_GT(centerBoost - octaveBoost, 6.0)
      << "At +12dB (narrow Q), 1 octave away should be >6dB less than center. "
      << "Center=" << centerBoost << "dB, Octave=" << octaveBoost << "dB";
}

TEST_F(GraphicEQTest, ProportionalQ_WideAtLowGain)
{
  constexpr size_t kNumSamples = 16384;
  constexpr size_t kSkip = 4096;

  eq.setBandGain(kTestBand, 2.0f);

  auto inputCenter = generateSine(kTestBandFreq, 44100.0f, kNumSamples);
  std::vector<float> outputCenter(kNumSamples);
  eq.process(inputCenter.data(), outputCenter.data(), kNumSamples);
  double centerBoost =
      20.0 * std::log10(computeRMS(outputCenter, kSkip) / computeRMS(inputCenter, kSkip));

  eq.reset();

  auto inputOctave = generateSine(2000.0f, 44100.0f, kNumSamples);
  std::vector<float> outputOctave(kNumSamples);
  eq.process(inputOctave.data(), outputOctave.data(), kNumSamples);
  double octaveBoost =
      20.0 * std::log10(computeRMS(outputOctave, kSkip) / computeRMS(inputOctave, kSkip));

  EXPECT_LT(centerBoost - octaveBoost, 2.5)
      << "At +2dB (wide Q), 1 octave away should be within 2.5dB of center. "
      << "Center=" << centerBoost << "dB, Octave=" << octaveBoost << "dB";
}

TEST_F(GraphicEQTest, ResetClearsState)
{
  constexpr size_t kNumSamples = 512;
  auto input = generateSine(kTestBandFreq, 44100.0f, kNumSamples);
  std::vector<float> output(kNumSamples);

  eq.setBandGain(kTestBand, 6.0f);
  eq.process(input.data(), output.data(), kNumSamples);

  eq.reset();

  std::vector<float> silence(kNumSamples, 0.0f);
  eq.process(silence.data(), output.data(), kNumSamples);

  for (size_t i = 0; i < kNumSamples; ++i)
    EXPECT_FLOAT_EQ(output[i], 0.0f) << "Output not zero at sample " << i << " after reset";
}

TEST_F(GraphicEQTest, GainClamping)
{
  eq.setBandGain(0, 20.0f);
  EXPECT_FLOAT_EQ(eq.getBandGain(0), MaxGraphicEQGainDb);

  eq.setBandGain(0, -20.0f);
  EXPECT_FLOAT_EQ(eq.getBandGain(0), MinGraphicEQGainDb);

  eq.setBandGain(0, 5.0f);
  EXPECT_FLOAT_EQ(eq.getBandGain(0), 5.0f);
}

TEST_F(GraphicEQTest, InvalidBandIndex)
{
  eq.setBandGain(-1, 6.0f);
  eq.setBandGain(kGraphicEQNumBands, 6.0f);
  EXPECT_FLOAT_EQ(eq.getBandGain(-1), DefaultGraphicEQGainDb);
  EXPECT_FLOAT_EQ(eq.getBandGain(kGraphicEQNumBands), DefaultGraphicEQGainDb);
}

TEST_F(GraphicEQTest, SampleRateChange)
{
  constexpr size_t kNumSamples = 16384;
  constexpr size_t kSkip = 4096;

  eq.setSampleRate(96000.0);
  eq.setBandGain(kTestBand, 6.0f);

  auto input = generateSine(kTestBandFreq, 96000.0f, kNumSamples);
  std::vector<float> output(kNumSamples);
  eq.process(input.data(), output.data(), kNumSamples);

  double inputRMS = computeRMS(input, kSkip, kNumSamples - kSkip);
  double outputRMS = computeRMS(output, kSkip, kNumSamples - kSkip);
  double ratioDb = 20.0 * std::log10(outputRMS / inputRMS);

  EXPECT_NEAR(ratioDb, 6.0, 1.0)
      << "Band 14 at +6dB should boost 1kHz by ~6dB at 96kHz sample rate";
}

TEST_F(GraphicEQTest, InPlaceProcessing)
{
  constexpr size_t kNumSamples = 8192;
  constexpr size_t kSkip = 2048;

  eq.setBandGain(kTestBand, 6.0f);

  auto buffer = generateSine(kTestBandFreq, 44100.0f, kNumSamples);
  double inputRMS = computeRMS(buffer, kSkip, kNumSamples - kSkip);

  eq.process(buffer.data(), buffer.data(), kNumSamples);

  double outputRMS = computeRMS(buffer, kSkip, kNumSamples - kSkip);
  double ratioDb = 20.0 * std::log10(outputRMS / inputRMS);

  EXPECT_NEAR(ratioDb, 6.0, 1.0) << "In-place processing should work correctly";
}

TEST_F(GraphicEQTest, MultipleBands)
{
  constexpr size_t kNumSamples = 16384;
  constexpr size_t kSkip = 4096;

  eq.setBandGain(4, 6.0f);           // 100 Hz
  eq.setBandGain(kTestBand, -6.0f);  // 1 kHz

  auto input100 = generateSine(100.0f, 44100.0f, kNumSamples);
  std::vector<float> output100(kNumSamples);
  eq.process(input100.data(), output100.data(), kNumSamples);
  double boost100 = 20.0 * std::log10(computeRMS(output100, kSkip) / computeRMS(input100, kSkip));

  eq.reset();

  auto input1k = generateSine(kTestBandFreq, 44100.0f, kNumSamples);
  std::vector<float> output1k(kNumSamples);
  eq.process(input1k.data(), output1k.data(), kNumSamples);
  double boost1k = 20.0 * std::log10(computeRMS(output1k, kSkip) / computeRMS(input1k, kSkip));

  EXPECT_GT(boost100, 3.0) << "100Hz should be boosted, got " << boost100 << "dB";
  EXPECT_LT(boost1k, -3.0) << "1kHz should be cut, got " << boost1k << "dB";
}

TEST_F(GraphicEQTest, MagnitudeResponse_LowFreqCut_StableAtDC)
{
  // A cut on a low band should not affect DC response significantly.
  // This catches catastrophic float cancellation in the magnitude formula.
  float gainsDb[kGraphicEQNumBands] = {};
  gainsDb[3] = -18.0f;  // Max cut at 80 Hz

  float dcResponse = GraphicEQ::computeMagnitudeResponseDb(gainsDb, 1.0f, 44100.0);
  EXPECT_NEAR(dcResponse, 0.0, 2.0)
      << "DC response with 80Hz cut should be near 0dB, got " << dcResponse;
}

TEST_F(GraphicEQTest, MagnitudeResponse_LowFreqCut_SmoothCurve)
{
  // The magnitude response should vary smoothly across low frequencies.
  // Wild jumps indicate numerical instability (not normal filter shape).
  // With Q=6 at max cut, the filter is narrow, so we use 1Hz steps and
  // a generous threshold -- we're testing for float instability, not shape.
  float gainsDb[kGraphicEQNumBands] = {};
  gainsDb[3] = -12.0f;  // Max cut at 80 Hz

  float prevDb = GraphicEQ::computeMagnitudeResponseDb(gainsDb, 20.0f, 44100.0);
  int largeJumps = 0;

  for (float freq = 21.0f; freq <= 200.0f; freq += 1.0f)
  {
    float db = GraphicEQ::computeMagnitudeResponseDb(gainsDb, freq, 44100.0);
    float delta = std::fabs(db - prevDb);
    if (delta > 4.0f)
      ++largeJumps;
    prevDb = db;
  }

  EXPECT_EQ(largeJumps, 0)
      << "Magnitude response should not have jumps >4dB between 1Hz-spaced points";
}

TEST_F(GraphicEQTest, MagnitudeResponse_LowFreqBoost_AtCenter)
{
  // Magnitude at the band center should closely match the gain setting
  float gainsDb[kGraphicEQNumBands] = {};
  gainsDb[3] = 12.0f;  // 12dB boost at 80 Hz

  float response = GraphicEQ::computeMagnitudeResponseDb(gainsDb, 79.69f, 44100.0);
  EXPECT_NEAR(response, 12.0, 1.0) << "Response at 80Hz center should be ~12dB, got " << response;
}

TEST_F(GraphicEQTest, MagnitudeResponse_AwayFromBand_NearUnity)
{
  // Far from the adjusted band, response should be near 0dB
  float gainsDb[kGraphicEQNumBands] = {};
  gainsDb[3] = -18.0f;  // Max cut at 80 Hz

  float response5k = GraphicEQ::computeMagnitudeResponseDb(gainsDb, 5000.0f, 44100.0);
  EXPECT_NEAR(response5k, 0.0, 1.0) << "5kHz should be unaffected by 80Hz cut, got " << response5k;
}

TEST_F(GraphicEQTest, MagnitudeResponseMatchesProcessing)
{
  constexpr size_t kNumSamples = 16384;
  constexpr size_t kSkip = 4096;

  eq.setBandGain(kTestBand, 8.0f);

  auto input = generateSine(kTestBandFreq, 44100.0f, kNumSamples);
  std::vector<float> output(kNumSamples);
  eq.process(input.data(), output.data(), kNumSamples);

  double measuredDb = 20.0 * std::log10(computeRMS(output, kSkip) / computeRMS(input, kSkip));

  float gainsDb[kGraphicEQNumBands] = {};
  gainsDb[kTestBand] = 8.0f;
  float predictedDb = GraphicEQ::computeMagnitudeResponseDb(gainsDb, kTestBandFreq, 44100.0);

  EXPECT_NEAR(measuredDb, static_cast<double>(predictedDb), 0.5)
      << "Magnitude response function should match actual processing";
}
