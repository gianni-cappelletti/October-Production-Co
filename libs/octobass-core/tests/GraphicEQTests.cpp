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

struct NodeArrays
{
  GraphicEQNode nodes[kGraphicEQNumNodes] = {};
  GraphicEQCut lowCut;
  GraphicEQCut highCut;
};

float magnitudeDb(const NodeArrays& config, float freqHz, SampleRate sampleRate = 44100.0)
{
  return GraphicEQ::computeMagnitudeResponseDb(config.nodes, kGraphicEQNumNodes, config.lowCut,
                                               config.highCut, freqHz, sampleRate);
}

}  // namespace

class GraphicEQTest : public ::testing::Test
{
 protected:
  void SetUp() override { eq.setSampleRate(44100.0); }

  GraphicEQ eq;
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

  EXPECT_NEAR(ratioDb, 0.0, 0.01) << "No active nodes should produce unity gain";
}

TEST_F(GraphicEQTest, SingleNodeBoostAtArbitraryFreq)
{
  constexpr size_t kNumSamples = 16384;
  constexpr size_t kSkip = 4096;

  // 440 Hz is not on the legacy fixed-band grid
  eq.setNode(0, true, 440.0f, 6.0f);

  auto input = generateSine(440.0f, 44100.0f, kNumSamples);
  std::vector<float> output(kNumSamples);
  eq.process(input.data(), output.data(), kNumSamples);

  double inputRMS = computeRMS(input, kSkip, kNumSamples - kSkip);
  double outputRMS = computeRMS(output, kSkip, kNumSamples - kSkip);
  double ratioDb = 20.0 * std::log10(outputRMS / inputRMS);

  EXPECT_NEAR(ratioDb, 6.0, 1.0) << "Node at 440Hz +6dB should boost 440Hz by ~6dB";
}

TEST_F(GraphicEQTest, SingleNodeCutAtArbitraryFreq)
{
  constexpr size_t kNumSamples = 16384;
  constexpr size_t kSkip = 4096;

  eq.setNode(0, true, 440.0f, -6.0f);

  auto input = generateSine(440.0f, 44100.0f, kNumSamples);
  std::vector<float> output(kNumSamples);
  eq.process(input.data(), output.data(), kNumSamples);

  double inputRMS = computeRMS(input, kSkip, kNumSamples - kSkip);
  double outputRMS = computeRMS(output, kSkip, kNumSamples - kSkip);
  double ratioDb = 20.0 * std::log10(outputRMS / inputRMS);

  EXPECT_NEAR(ratioDb, -6.0, 1.0) << "Node at 440Hz -6dB should cut 440Hz by ~6dB";
}

TEST_F(GraphicEQTest, OutOfBandRejection)
{
  constexpr size_t kNumSamples = 16384;
  constexpr size_t kSkip = 4096;

  eq.setNode(0, true, 1000.0f, 12.0f);

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

  eq.setNode(0, true, 1000.0f, 12.0f);

  auto inputCenter = generateSine(1000.0f, 44100.0f, kNumSamples);
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

  eq.setNode(0, true, 1000.0f, 2.0f);

  auto inputCenter = generateSine(1000.0f, 44100.0f, kNumSamples);
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

TEST_F(GraphicEQTest, FrequencyChangeWhileRunning)
{
  constexpr size_t kNumSamples = 16384;
  constexpr size_t kSkip = 4096;

  eq.setNode(0, true, 500.0f, 12.0f);

  auto warmup = generateSine(500.0f, 44100.0f, kNumSamples);
  std::vector<float> output(kNumSamples);
  eq.process(warmup.data(), output.data(), kNumSamples);

  // Move the node mid-stream without resetting, as a UI drag would
  eq.setNodeFrequency(0, 2000.0f);

  auto input2k = generateSine(2000.0f, 44100.0f, kNumSamples);
  eq.process(input2k.data(), output.data(), kNumSamples);
  double boost2k = 20.0 * std::log10(computeRMS(output, kSkip) / computeRMS(input2k, kSkip));

  for (size_t i = 0; i < kNumSamples; ++i)
    ASSERT_TRUE(std::isfinite(output[i])) << "Non-finite output at sample " << i;

  EXPECT_NEAR(boost2k, 12.0, 1.5) << "After moving the node to 2kHz, 2kHz should be boosted";

  eq.reset();

  auto input500 = generateSine(500.0f, 44100.0f, kNumSamples);
  eq.process(input500.data(), output.data(), kNumSamples);
  double boost500 = 20.0 * std::log10(computeRMS(output, kSkip) / computeRMS(input500, kSkip));

  EXPECT_LT(boost500, 3.0) << "500Hz should no longer be at full boost after the move";
}

TEST_F(GraphicEQTest, InactiveNodeBypass)
{
  constexpr size_t kNumSamples = 8192;
  constexpr size_t kSkip = 1024;

  eq.setNode(0, false, 1000.0f, 12.0f);

  auto input = generateSine(1000.0f, 44100.0f, kNumSamples);
  std::vector<float> output(kNumSamples);
  eq.process(input.data(), output.data(), kNumSamples);

  double ratioDb = 20.0 * std::log10(computeRMS(output, kSkip) / computeRMS(input, kSkip));
  EXPECT_NEAR(ratioDb, 0.0, 0.01) << "Inactive node with stored gain must not affect the signal";
}

TEST_F(GraphicEQTest, ActiveZeroGainIsUnity)
{
  constexpr size_t kNumSamples = 8192;
  constexpr size_t kSkip = 1024;

  eq.setNode(0, true, 1000.0f, 0.0f);

  auto input = generateSine(1000.0f, 44100.0f, kNumSamples);
  std::vector<float> output(kNumSamples);
  eq.process(input.data(), output.data(), kNumSamples);

  double ratioDb = 20.0 * std::log10(computeRMS(output, kSkip) / computeRMS(input, kSkip));
  EXPECT_NEAR(ratioDb, 0.0, 0.01) << "Active node at 0dB must be unity";
}

TEST_F(GraphicEQTest, DeactivatingNodeRestoresUnity)
{
  constexpr size_t kNumSamples = 8192;
  constexpr size_t kSkip = 1024;

  eq.setNode(0, true, 1000.0f, 12.0f);
  eq.setNodeActive(0, false);
  eq.reset();

  auto input = generateSine(1000.0f, 44100.0f, kNumSamples);
  std::vector<float> output(kNumSamples);
  eq.process(input.data(), output.data(), kNumSamples);

  double ratioDb = 20.0 * std::log10(computeRMS(output, kSkip) / computeRMS(input, kSkip));
  EXPECT_NEAR(ratioDb, 0.0, 0.01) << "Deactivated node must stop affecting the signal";
}

TEST_F(GraphicEQTest, GainClamping)
{
  eq.setNodeGain(0, 20.0f);
  EXPECT_FLOAT_EQ(eq.getNodeGain(0), MaxGraphicEQGainDb);

  eq.setNodeGain(0, -20.0f);
  EXPECT_FLOAT_EQ(eq.getNodeGain(0), MinGraphicEQGainDb);

  eq.setNodeGain(0, 5.0f);
  EXPECT_FLOAT_EQ(eq.getNodeGain(0), 5.0f);
}

TEST_F(GraphicEQTest, FrequencyClamping)
{
  eq.setNodeFrequency(0, 5.0f);
  EXPECT_FLOAT_EQ(eq.getNodeFrequency(0), MinGraphicEQFreqHz);

  eq.setNodeFrequency(0, 30000.0f);
  EXPECT_FLOAT_EQ(eq.getNodeFrequency(0), MaxGraphicEQFreqHz);

  eq.setNodeFrequency(0, 440.0f);
  EXPECT_FLOAT_EQ(eq.getNodeFrequency(0), 440.0f);
}

TEST_F(GraphicEQTest, InvalidSlotIndex)
{
  eq.setNode(-1, true, 1000.0f, 6.0f);
  eq.setNode(kGraphicEQNumNodes, true, 1000.0f, 6.0f);

  EXPECT_FALSE(eq.getNodeActive(-1));
  EXPECT_FALSE(eq.getNodeActive(kGraphicEQNumNodes));
  EXPECT_FLOAT_EQ(eq.getNodeGain(-1), DefaultGraphicEQGainDb);
  EXPECT_FLOAT_EQ(eq.getNodeGain(kGraphicEQNumNodes), DefaultGraphicEQGainDb);
  EXPECT_FLOAT_EQ(eq.getNodeFrequency(-1), DefaultGraphicEQFreqHz);
  EXPECT_FLOAT_EQ(eq.getNodeFrequency(kGraphicEQNumNodes), DefaultGraphicEQFreqHz);
}

TEST_F(GraphicEQTest, ResetClearsState)
{
  constexpr size_t kNumSamples = 512;
  auto input = generateSine(1000.0f, 44100.0f, kNumSamples);
  std::vector<float> output(kNumSamples);

  eq.setNode(0, true, 1000.0f, 6.0f);
  eq.process(input.data(), output.data(), kNumSamples);

  eq.reset();

  std::vector<float> silence(kNumSamples, 0.0f);
  eq.process(silence.data(), output.data(), kNumSamples);

  for (size_t i = 0; i < kNumSamples; ++i)
    EXPECT_FLOAT_EQ(output[i], 0.0f) << "Output not zero at sample " << i << " after reset";
}

TEST_F(GraphicEQTest, SampleRateChange)
{
  constexpr size_t kNumSamples = 16384;
  constexpr size_t kSkip = 4096;

  eq.setSampleRate(96000.0);
  eq.setNode(0, true, 1000.0f, 6.0f);

  auto input = generateSine(1000.0f, 96000.0f, kNumSamples);
  std::vector<float> output(kNumSamples);
  eq.process(input.data(), output.data(), kNumSamples);

  double inputRMS = computeRMS(input, kSkip, kNumSamples - kSkip);
  double outputRMS = computeRMS(output, kSkip, kNumSamples - kSkip);
  double ratioDb = 20.0 * std::log10(outputRMS / inputRMS);

  EXPECT_NEAR(ratioDb, 6.0, 1.0)
      << "Node at 1kHz +6dB should boost 1kHz by ~6dB at 96kHz sample rate";
}

TEST_F(GraphicEQTest, InPlaceProcessing)
{
  constexpr size_t kNumSamples = 8192;
  constexpr size_t kSkip = 2048;

  eq.setNode(0, true, 1000.0f, 6.0f);

  auto buffer = generateSine(1000.0f, 44100.0f, kNumSamples);
  double inputRMS = computeRMS(buffer, kSkip, kNumSamples - kSkip);

  eq.process(buffer.data(), buffer.data(), kNumSamples);

  double outputRMS = computeRMS(buffer, kSkip, kNumSamples - kSkip);
  double ratioDb = 20.0 * std::log10(outputRMS / inputRMS);

  EXPECT_NEAR(ratioDb, 6.0, 1.0) << "In-place processing should work correctly";
}

TEST_F(GraphicEQTest, MultipleNodes)
{
  constexpr size_t kNumSamples = 16384;
  constexpr size_t kSkip = 4096;

  eq.setNode(0, true, 100.0f, 6.0f);
  eq.setNode(1, true, 1000.0f, -6.0f);

  auto input100 = generateSine(100.0f, 44100.0f, kNumSamples);
  std::vector<float> output100(kNumSamples);
  eq.process(input100.data(), output100.data(), kNumSamples);
  double boost100 = 20.0 * std::log10(computeRMS(output100, kSkip) / computeRMS(input100, kSkip));

  eq.reset();

  auto input1k = generateSine(1000.0f, 44100.0f, kNumSamples);
  std::vector<float> output1k(kNumSamples);
  eq.process(input1k.data(), output1k.data(), kNumSamples);
  double boost1k = 20.0 * std::log10(computeRMS(output1k, kSkip) / computeRMS(input1k, kSkip));

  EXPECT_GT(boost100, 3.0) << "100Hz should be boosted, got " << boost100 << "dB";
  EXPECT_LT(boost1k, -3.0) << "1kHz should be cut, got " << boost1k << "dB";
}

TEST_F(GraphicEQTest, MagnitudeResponse_LowFreqCut_StableAtDC)
{
  // A cut on a low-frequency node should not affect DC response significantly.
  // This catches catastrophic float cancellation in the magnitude formula.
  NodeArrays nodes;
  nodes.nodes[0].active = true;
  nodes.nodes[0].freqHz = 80.0f;
  nodes.nodes[0].gainDb = -12.0f;

  float dcResponse = magnitudeDb(nodes, 1.0f);
  EXPECT_NEAR(dcResponse, 0.0, 2.0)
      << "DC response with 80Hz cut should be near 0dB, got " << dcResponse;
}

TEST_F(GraphicEQTest, MagnitudeResponse_LowFreqCut_SmoothCurve)
{
  // The magnitude response should vary smoothly across low frequencies,
  // including a node at the new 20 Hz lower bound. Wild jumps indicate
  // numerical instability (not normal filter shape). At max cut the Q is 8,
  // so the notch at 20 Hz is only ~2.5 Hz wide; 0.1 Hz steps keep each
  // legitimate step well under the jump threshold.
  NodeArrays nodes;
  nodes.nodes[0].active = true;
  nodes.nodes[0].freqHz = MinGraphicEQFreqHz;
  nodes.nodes[0].gainDb = -12.0f;

  float prevDb = magnitudeDb(nodes, 10.0f);
  int largeJumps = 0;

  for (float freq = 10.1f; freq <= 60.0f; freq += 0.1f)
  {
    float db = magnitudeDb(nodes, freq);
    float delta = std::fabs(db - prevDb);
    if (delta > 4.0f)
      ++largeJumps;
    prevDb = db;
  }

  EXPECT_EQ(largeJumps, 0)
      << "Magnitude response should not have jumps >4dB between 0.1Hz-spaced points";
}

TEST_F(GraphicEQTest, MagnitudeResponse_BoostAtArbitraryCenter)
{
  // Magnitude at the node center should closely match the gain setting
  NodeArrays nodes;
  nodes.nodes[0].active = true;
  nodes.nodes[0].freqHz = 440.0f;
  nodes.nodes[0].gainDb = 12.0f;

  float response = magnitudeDb(nodes, 440.0f);
  EXPECT_NEAR(response, 12.0, 1.0) << "Response at 440Hz center should be ~12dB, got " << response;
}

TEST_F(GraphicEQTest, MagnitudeResponse_InactiveNodeIgnored)
{
  NodeArrays nodes;
  nodes.nodes[0].active = false;
  nodes.nodes[0].freqHz = 1000.0f;
  nodes.nodes[0].gainDb = 12.0f;

  float response = magnitudeDb(nodes, 1000.0f);
  EXPECT_NEAR(response, 0.0, 0.001) << "Inactive node must not contribute to the response";
}

TEST_F(GraphicEQTest, MagnitudeResponse_AwayFromNode_NearUnity)
{
  NodeArrays nodes;
  nodes.nodes[0].active = true;
  nodes.nodes[0].freqHz = 80.0f;
  nodes.nodes[0].gainDb = -12.0f;

  float response5k = magnitudeDb(nodes, 5000.0f);
  EXPECT_NEAR(response5k, 0.0, 1.0) << "5kHz should be unaffected by 80Hz cut, got " << response5k;
}

TEST_F(GraphicEQTest, MagnitudeResponseMatchesProcessing)
{
  constexpr size_t kNumSamples = 16384;
  constexpr size_t kSkip = 4096;

  eq.setNode(0, true, 1000.0f, 8.0f);

  auto input = generateSine(1000.0f, 44100.0f, kNumSamples);
  std::vector<float> output(kNumSamples);
  eq.process(input.data(), output.data(), kNumSamples);

  double measuredDb = 20.0 * std::log10(computeRMS(output, kSkip) / computeRMS(input, kSkip));

  NodeArrays nodes;
  nodes.nodes[0].active = true;
  nodes.nodes[0].freqHz = 1000.0f;
  nodes.nodes[0].gainDb = 8.0f;
  float predictedDb = magnitudeDb(nodes, 1000.0f);

  EXPECT_NEAR(measuredDb, static_cast<double>(predictedDb), 0.5)
      << "Magnitude response function should match actual processing";
}

TEST_F(GraphicEQTest, LowCutMinus3dBAtCutoff)
{
  NodeArrays nodes;
  nodes.lowCut.active = true;
  nodes.lowCut.freqHz = 1000.0f;

  float response = magnitudeDb(nodes, 1000.0f);
  EXPECT_NEAR(response, -3.0, 0.5)
      << "Butterworth low cut should be -3dB at cutoff, got " << response;
}

TEST_F(GraphicEQTest, LowCutSlope24dBPerOctave)
{
  NodeArrays nodes;
  nodes.lowCut.active = true;
  nodes.lowCut.freqHz = 1000.0f;

  float atHalf = magnitudeDb(nodes, 500.0f);
  float atQuarter = magnitudeDb(nodes, 250.0f);

  EXPECT_NEAR(atHalf - atQuarter, 24.0, 1.0)
      << "Low cut stopband should fall 24dB per octave, got " << (atHalf - atQuarter);
}

TEST_F(GraphicEQTest, HighCutSlope24dBPerOctave)
{
  NodeArrays nodes;
  nodes.highCut.active = true;
  nodes.highCut.freqHz = 1000.0f;

  float atDouble = magnitudeDb(nodes, 2000.0f);
  float atQuadruple = magnitudeDb(nodes, 4000.0f);

  EXPECT_NEAR(atDouble - atQuadruple, 24.0, 1.0)
      << "High cut stopband should fall 24dB per octave, got " << (atDouble - atQuadruple);
}

TEST_F(GraphicEQTest, LowCutProcessingAttenuatesBelowCutoff)
{
  constexpr size_t kNumSamples = 16384;
  constexpr size_t kSkip = 4096;

  eq.setLowCut(true, 1000.0f);

  auto input = generateSine(250.0f, 44100.0f, kNumSamples);
  std::vector<float> output(kNumSamples);
  eq.process(input.data(), output.data(), kNumSamples);

  double ratioDb = 20.0 * std::log10(computeRMS(output, kSkip) / computeRMS(input, kSkip));
  EXPECT_NEAR(ratioDb, -48.2, 2.0) << "250Hz through a 1kHz 24dB/oct low cut should be ~-48dB";

  eq.reset();

  auto passInput = generateSine(4000.0f, 44100.0f, kNumSamples);
  eq.process(passInput.data(), output.data(), kNumSamples);
  double passDb = 20.0 * std::log10(computeRMS(output, kSkip) / computeRMS(passInput, kSkip));
  EXPECT_NEAR(passDb, 0.0, 1.0) << "Frequencies well above the low cut should pass";
}

TEST_F(GraphicEQTest, HighCutProcessingAttenuatesAboveCutoff)
{
  constexpr size_t kNumSamples = 16384;
  constexpr size_t kSkip = 4096;

  eq.setHighCut(true, 1000.0f);

  auto input = generateSine(4000.0f, 44100.0f, kNumSamples);
  std::vector<float> output(kNumSamples);
  eq.process(input.data(), output.data(), kNumSamples);

  double ratioDb = 20.0 * std::log10(computeRMS(output, kSkip) / computeRMS(input, kSkip));
  EXPECT_NEAR(ratioDb, -48.2, 2.0) << "4kHz through a 1kHz 24dB/oct high cut should be ~-48dB";

  eq.reset();

  auto passInput = generateSine(250.0f, 44100.0f, kNumSamples);
  eq.process(passInput.data(), output.data(), kNumSamples);
  double passDb = 20.0 * std::log10(computeRMS(output, kSkip) / computeRMS(passInput, kSkip));
  EXPECT_NEAR(passDb, 0.0, 1.0) << "Frequencies well below the high cut should pass";
}

TEST_F(GraphicEQTest, CutFrequencyClamping)
{
  eq.setLowCut(true, 5.0f);
  EXPECT_FLOAT_EQ(eq.getLowCutFrequency(), MinGraphicEQFreqHz);

  eq.setHighCut(true, 30000.0f);
  EXPECT_FLOAT_EQ(eq.getHighCutFrequency(), MaxGraphicEQFreqHz);
}

TEST_F(GraphicEQTest, NodeAboveOldRangeLimit)
{
  constexpr size_t kNumSamples = 16384;
  constexpr size_t kSkip = 4096;

  // 16 kHz was outside the previous 12.6 kHz ceiling
  eq.setNode(0, true, 16000.0f, 6.0f);

  auto input = generateSine(16000.0f, 44100.0f, kNumSamples);
  std::vector<float> output(kNumSamples);
  eq.process(input.data(), output.data(), kNumSamples);

  double ratioDb = 20.0 * std::log10(computeRMS(output, kSkip) / computeRMS(input, kSkip));
  EXPECT_NEAR(ratioDb, 6.0, 1.0) << "Node at 16kHz +6dB should boost 16kHz by ~6dB";
}

TEST_F(GraphicEQTest, MaxFrequencyStableAtLowSampleRate)
{
  // At 32 kHz the 20 kHz ceiling exceeds Nyquist; coefficients must clamp
  // below Nyquist instead of going unstable
  constexpr size_t kNumSamples = 8192;

  eq.setSampleRate(32000.0);
  eq.setNode(0, true, MaxGraphicEQFreqHz, 12.0f);
  eq.setHighCut(true, MaxGraphicEQFreqHz);
  eq.setLowCut(true, MaxGraphicEQFreqHz);

  auto input = generateSine(1000.0f, 32000.0f, kNumSamples);
  std::vector<float> output(kNumSamples);
  eq.process(input.data(), output.data(), kNumSamples);

  for (size_t i = 0; i < kNumSamples; ++i)
    ASSERT_TRUE(std::isfinite(output[i])) << "Non-finite output at sample " << i;
}

TEST_F(GraphicEQTest, CutFrequencyChangeWhileRunning)
{
  constexpr size_t kNumSamples = 8192;

  eq.setLowCut(true, 100.0f);

  auto input = generateSine(440.0f, 44100.0f, kNumSamples);
  std::vector<float> output(kNumSamples);
  eq.process(input.data(), output.data(), kNumSamples);

  // Move the cut mid-stream without resetting, as a UI drag would
  eq.setLowCut(true, 2000.0f);
  eq.process(input.data(), output.data(), kNumSamples);

  for (size_t i = 0; i < kNumSamples; ++i)
    ASSERT_TRUE(std::isfinite(output[i])) << "Non-finite output at sample " << i;
}

TEST_F(GraphicEQTest, DeactivatingCutRestoresUnity)
{
  constexpr size_t kNumSamples = 8192;
  constexpr size_t kSkip = 1024;

  eq.setLowCut(true, 1000.0f);
  eq.setLowCut(false, 1000.0f);
  eq.reset();

  auto input = generateSine(250.0f, 44100.0f, kNumSamples);
  std::vector<float> output(kNumSamples);
  eq.process(input.data(), output.data(), kNumSamples);

  double ratioDb = 20.0 * std::log10(computeRMS(output, kSkip) / computeRMS(input, kSkip));
  EXPECT_NEAR(ratioDb, 0.0, 0.01) << "Deactivated low cut must stop affecting the signal";
}

TEST_F(GraphicEQTest, HighCutMinus3dBAtCutoff)
{
  NodeArrays nodes;
  nodes.highCut.active = true;
  nodes.highCut.freqHz = 1000.0f;

  float response = magnitudeDb(nodes, 1000.0f);
  EXPECT_NEAR(response, -3.0, 0.5)
      << "Butterworth high cut should be -3dB at cutoff, got " << response;
}

TEST_F(GraphicEQTest, ResetClearsStateWithCuts)
{
  constexpr size_t kNumSamples = 512;
  auto input = generateSine(1000.0f, 44100.0f, kNumSamples);
  std::vector<float> output(kNumSamples);

  eq.setLowCut(true, 500.0f);
  eq.setHighCut(true, 2000.0f);
  eq.process(input.data(), output.data(), kNumSamples);

  eq.reset();

  std::vector<float> silence(kNumSamples, 0.0f);
  eq.process(silence.data(), output.data(), kNumSamples);

  for (size_t i = 0; i < kNumSamples; ++i)
    EXPECT_FLOAT_EQ(output[i], 0.0f)
        << "Cut filter state not cleared at sample " << i << " after reset";
}

TEST_F(GraphicEQTest, DeactivatingNodeWhileRunningRestoresUnity)
{
  constexpr size_t kNumSamples = 8192;
  constexpr size_t kSkip = 1024;

  eq.setNode(0, true, 1000.0f, 12.0f);

  auto warmup = generateSine(1000.0f, 44100.0f, kNumSamples);
  std::vector<float> output(kNumSamples);
  eq.process(warmup.data(), output.data(), kNumSamples);

  // Deactivate mid-stream without resetting, as a UI toggle would
  eq.setNodeActive(0, false);

  auto input = generateSine(1000.0f, 44100.0f, kNumSamples);
  eq.process(input.data(), output.data(), kNumSamples);

  double ratioDb = 20.0 * std::log10(computeRMS(output, kSkip) / computeRMS(input, kSkip));
  EXPECT_NEAR(ratioDb, 0.0, 0.01)
      << "Node deactivated during processing must stop affecting the signal";
}

TEST_F(GraphicEQTest, MagnitudeResponseMatchesProcessing_NodeAndCut)
{
  constexpr size_t kNumSamples = 16384;
  constexpr size_t kSkip = 4096;

  eq.setNode(0, true, 1000.0f, 6.0f);
  eq.setLowCut(true, 200.0f);

  auto input = generateSine(1000.0f, 44100.0f, kNumSamples);
  std::vector<float> output(kNumSamples);
  eq.process(input.data(), output.data(), kNumSamples);

  double measuredDb = 20.0 * std::log10(computeRMS(output, kSkip) / computeRMS(input, kSkip));

  NodeArrays nodes;
  nodes.nodes[0].active = true;
  nodes.nodes[0].freqHz = 1000.0f;
  nodes.nodes[0].gainDb = 6.0f;
  nodes.lowCut.active = true;
  nodes.lowCut.freqHz = 200.0f;
  float predictedDb = magnitudeDb(nodes, 1000.0f);

  EXPECT_NEAR(measuredDb, static_cast<double>(predictedDb), 0.5)
      << "Combined node and cut response should match actual processing";
}

TEST_F(GraphicEQTest, MagnitudeResponseMatchesProcessing_WithCuts)
{
  constexpr size_t kNumSamples = 16384;
  constexpr size_t kSkip = 4096;

  eq.setLowCut(true, 200.0f);
  eq.setHighCut(true, 4000.0f);

  auto input = generateSine(300.0f, 44100.0f, kNumSamples);
  std::vector<float> output(kNumSamples);
  eq.process(input.data(), output.data(), kNumSamples);

  double measuredDb = 20.0 * std::log10(computeRMS(output, kSkip) / computeRMS(input, kSkip));

  NodeArrays nodes;
  nodes.lowCut.active = true;
  nodes.lowCut.freqHz = 200.0f;
  nodes.highCut.active = true;
  nodes.highCut.freqHz = 4000.0f;
  float predictedDb = magnitudeDb(nodes, 300.0f);

  EXPECT_NEAR(measuredDb, static_cast<double>(predictedDb), 0.5)
      << "Magnitude response with cuts should match actual processing";
}
