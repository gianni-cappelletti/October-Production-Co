#include <gtest/gtest.h>

#include <cmath>
#include <octobir-core/IRProcessor.hpp>
#include <string>
#include <vector>

#include "AllocationTracker.hpp"

using namespace octob;
using octob::test::allocationTrackingAvailable;
using octob::test::resetTrackedAllocationCount;
using octob::test::ScopedAllocationTracking;
using octob::test::trackedAllocationCount;

namespace
{

constexpr double SampleRate = 48000.0;
constexpr size_t FramesAfterLoad = 2 * static_cast<size_t>(SampleRate);

const std::string IrA = std::string(TEST_DATA_DIR) + "/INPUT_ir_a.wav";
const std::string IrB = std::string(TEST_DATA_DIR) + "/INPUT_ir_b.wav";
const std::string StereoIr = std::string(TEST_DATA_DIR) + "/INPUT_ir_stereo.wav";

class IRProcessorRealtimeAllocation : public ::testing::TestWithParam<size_t>
{
 protected:
  void SetUp() override
  {
    if (!allocationTrackingAvailable())
      GTEST_SKIP() << "Allocation tracking requires an AddressSanitizer build";
    processor_.setSampleRate(SampleRate);
    processor_.setMaxBlockSize(blockSize());
  }

  static size_t blockSize() { return GetParam(); }

  static std::vector<float> sineBlock(size_t blockIndex)
  {
    std::vector<float> block(blockSize());
    for (size_t i = 0; i < block.size(); ++i)
    {
      const double t = static_cast<double>(blockIndex * block.size() + i) / SampleRate;
      block[i] = static_cast<float>(0.3 * std::sin(2.0 * Pi * 110.0 * t));
    }
    return block;
  }

  void load(bool (IRProcessor::*loader)(const std::string&, std::string&), const std::string& path)
  {
    std::string error;
    ASSERT_TRUE((processor_.*loader)(path, error)) << error;
  }

  size_t allocationsWhileProcessingMono()
  {
    std::vector<float> output(blockSize());
    resetTrackedAllocationCount();
    for (size_t block = 0; block < FramesAfterLoad / blockSize(); ++block)
    {
      const auto input = sineBlock(block);
      const ScopedAllocationTracking tracking;
      processor_.processMono(input.data(), output.data(), blockSize());
    }
    return trackedAllocationCount();
  }

  size_t allocationsWhileProcessingStereo()
  {
    std::vector<float> left(blockSize());
    std::vector<float> right(blockSize());
    resetTrackedAllocationCount();
    for (size_t block = 0; block < FramesAfterLoad / blockSize(); ++block)
    {
      const auto input = sineBlock(block);
      const ScopedAllocationTracking tracking;
      processor_.processMonoToStereo(input.data(), left.data(), right.data(), blockSize());
    }
    return trackedAllocationCount();
  }

  IRProcessor processor_;
};

}  // namespace

TEST_P(IRProcessorRealtimeAllocation, MonoProcessingAfterFirstIrLoadDoesNotAllocate)
{
  load(&IRProcessor::loadImpulseResponse1, IrA);
  EXPECT_EQ(allocationsWhileProcessingMono(), 0u);
}

TEST_P(IRProcessorRealtimeAllocation, MonoProcessingAfterBothIrsLoadDoesNotAllocate)
{
  load(&IRProcessor::loadImpulseResponse1, IrA);
  load(&IRProcessor::loadImpulseResponse2, IrB);
  EXPECT_EQ(allocationsWhileProcessingMono(), 0u);
}

TEST_P(IRProcessorRealtimeAllocation, ProcessingAfterIrReplacementDoesNotAllocate)
{
  load(&IRProcessor::loadImpulseResponse1, IrA);
  allocationsWhileProcessingMono();
  load(&IRProcessor::loadImpulseResponse1, StereoIr);
  EXPECT_EQ(allocationsWhileProcessingMono(), 0u);
}

TEST_P(IRProcessorRealtimeAllocation, StereoProcessingAfterIrLoadDoesNotAllocate)
{
  load(&IRProcessor::loadImpulseResponse1, StereoIr);
  load(&IRProcessor::loadImpulseResponse2, IrB);
  EXPECT_EQ(allocationsWhileProcessingStereo(), 0u);
}

TEST_P(IRProcessorRealtimeAllocation, ProcessingAfterSampleRateChangeDoesNotAllocate)
{
  load(&IRProcessor::loadImpulseResponse1, IrA);
  allocationsWhileProcessingMono();
  processor_.setSampleRate(44100.0);
  EXPECT_EQ(allocationsWhileProcessingMono(), 0u);
}

INSTANTIATE_TEST_SUITE_P(HostBlockSizes, IRProcessorRealtimeAllocation,
                         ::testing::Values<size_t>(32, 64, 256, 1024, 4096),
                         [](const ::testing::TestParamInfo<size_t>& info)
                         { return "Block" + std::to_string(info.param); });

TEST(AllocationTracker, CountsHeapAllocationsInsideTrackedScope)
{
  if (!allocationTrackingAvailable())
    GTEST_SKIP() << "Allocation tracking requires an AddressSanitizer build";

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
