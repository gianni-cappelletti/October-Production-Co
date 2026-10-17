#pragma once

#include <array>
#include <octobass-core/BassProcessor.hpp>
#include <string>
#include <vector>

#include "Ports.hpp"

namespace octob::lv2::test
{

using ControlValues = std::array<float, port::NumPorts>;

class ReferenceChain
{
 public:
  ReferenceChain(double sampleRate, size_t maxBlockSize);

  void loadNamModel(NamSlot slot, const std::string& path);
  void clearNamModel(NamSlot slot);
  void loadImpulseResponse(const std::string& path);
  void clearImpulseResponse();
  void setNamQualityPercent(NamSlot slot, float percent);

  std::vector<float> process(const std::vector<float>& input, size_t blockSize,
                             const ControlValues& controls);

  int latencySamples() const { return processor_.getLatencySamples(); }

 private:
  void applyControls(const ControlValues& controls);

  BassProcessor processor_;
  size_t maxBlockSize_;
};

ControlValues defaultControls();

std::vector<float> generateBassSignal(double sampleRate, double seconds);

std::string testDataPath(const std::string& fileName);

}  // namespace octob::lv2::test
