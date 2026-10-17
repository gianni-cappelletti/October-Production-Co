#include <gtest/gtest.h>

#include <cmath>
#include <octobir-core/IRProcessor.hpp>
#include <string>
#include <vector>

#include "AllocationTracker.hpp"

using namespace octob;
using octob::test::resetTrackedAllocationCount;
using octob::test::ScopedAllocationTracking;
using octob::test::trackedAllocationCount;

namespace
{

constexpr double SampleRate = 48000.0;
constexpr size_t BlockSize = 256;
constexpr int BlocksAfterLoad = 400;

const std::string IrA = std::string(TEST_DATA_DIR) + "/INPUT_ir_a.wav";
const std::string IrB = std::string(TEST_DATA_DIR) + "/INPUT_ir_b.wav";
const std::string StereoIr = std::string(TEST_DATA_DIR) + "/INPUT_ir_stereo.wav";

std::vector<float> sineBlock(size_t blockIndex)
{
  std::vector<float> block(BlockSize);
  for (size_t i = 0; i < BlockSize; ++i)
  {
    const double t = static_cast<double>(blockIndex * BlockSize + i) / SampleRate;
    block[i] = static_cast<float>(0.3 * std::sin(2.0 * 3.14159265358979323846 * 110.0 * t));
  }
  return block;
}

class IRProcessorRealtimeAllocation : public ::testing::Test
{
 protected:
  void SetUp() override
  {
    processor_.setSampleRate(SampleRate);
    processor_.setMaxBlockSize(BlockSize);
  }

  void load(bool (IRProcessor::*loader)(const std::string&, std::string&), const std::string& path)
  {
    std::string error;
    ASSERT_TRUE((processor_.*loader)(path, error)) << error;
  }

  size_t allocationsWhileProcessingMono()
  {
    std::vector<float> output(BlockSize);
    resetTrackedAllocationCount();
    for (int block = 0; block < BlocksAfterLoad; ++block)
    {
      const auto input = sineBlock(static_cast<size_t>(block));
      const ScopedAllocationTracking tracking;
      processor_.processMono(input.data(), output.data(), BlockSize);
    }
    return trackedAllocationCount();
  }

  size_t allocationsWhileProcessingStereo()
  {
    std::vector<float> left(BlockSize);
    std::vector<float> right(BlockSize);
    resetTrackedAllocationCount();
    for (int block = 0; block < BlocksAfterLoad; ++block)
    {
      const auto input = sineBlock(static_cast<size_t>(block));
      const ScopedAllocationTracking tracking;
      processor_.processMonoToStereo(input.data(), left.data(), right.data(), BlockSize);
    }
    return trackedAllocationCount();
  }

  IRProcessor processor_;
};

}  // namespace

TEST_F(IRProcessorRealtimeAllocation, MonoProcessingAfterFirstIrLoadDoesNotAllocate)
{
  load(&IRProcessor::loadImpulseResponse1, IrA);
  EXPECT_EQ(allocationsWhileProcessingMono(), 0u);
}

TEST_F(IRProcessorRealtimeAllocation, MonoProcessingAfterBothIrsLoadDoesNotAllocate)
{
  load(&IRProcessor::loadImpulseResponse1, IrA);
  load(&IRProcessor::loadImpulseResponse2, IrB);
  EXPECT_EQ(allocationsWhileProcessingMono(), 0u);
}

TEST_F(IRProcessorRealtimeAllocation, ProcessingAfterIrReplacementDoesNotAllocate)
{
  load(&IRProcessor::loadImpulseResponse1, IrA);
  allocationsWhileProcessingMono();
  load(&IRProcessor::loadImpulseResponse1, StereoIr);
  EXPECT_EQ(allocationsWhileProcessingMono(), 0u);
}

TEST_F(IRProcessorRealtimeAllocation, StereoProcessingAfterIrLoadDoesNotAllocate)
{
  load(&IRProcessor::loadImpulseResponse1, StereoIr);
  load(&IRProcessor::loadImpulseResponse2, IrB);
  EXPECT_EQ(allocationsWhileProcessingStereo(), 0u);
}

TEST_F(IRProcessorRealtimeAllocation, ProcessingAfterSampleRateChangeDoesNotAllocate)
{
  load(&IRProcessor::loadImpulseResponse1, IrA);
  allocationsWhileProcessingMono();
  processor_.setSampleRate(44100.0);
  EXPECT_EQ(allocationsWhileProcessingMono(), 0u);
}

TEST(AllocationTracker, CountsHeapAllocationsInsideTrackedScope)
{
  volatile size_t elementCount = 64;
  resetTrackedAllocationCount();
  {
    const ScopedAllocationTracking tracking;
    std::vector<float> buffer(elementCount);
    buffer[0] = 1.0f;
    volatile float sink = buffer[0];
    (void)sink;
  }
  EXPECT_GT(trackedAllocationCount(), 0u);
}
