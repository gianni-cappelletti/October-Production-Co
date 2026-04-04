#pragma once

#include <string>
#include <vector>

#include "Types.hpp"

class WDL_ImpulseBuffer;  // NOLINT(readability-identifier-naming)

namespace octob
{

struct IRLoadResult
{
  bool success = false;
  std::string errorMessage;
  size_t numSamples = 0;
  int numChannels = 0;
  SampleRate sampleRate = 0.0;
};

class IRLoader
{
 public:
  IRLoader();
  ~IRLoader();

  IRLoadResult loadFromFile(const std::string& filepath);

  bool resampleAndInitialize(WDL_ImpulseBuffer& impulseBuffer, SampleRate targetSampleRate);

  static int findPeakSampleIndex(WDL_ImpulseBuffer& impulseBuffer);

  SampleRate getIRSampleRate() const { return irSampleRate_; }
  size_t getNumSamples() const { return numSamples_; }
  int getNumChannels() const { return numChannels_; }

 private:
  // Converts a single-channel IR to minimum phase in-place using the cepstrum method.
  // fftSize must be a power of 2, >= 2 * samples.size(), and a multiple of 32.
  static void convertToMinimumPhase(std::vector<Sample>& samples, int fftSize);

  std::vector<Sample> irBuffer_;
  SampleRate irSampleRate_ = 0.0;
  size_t numSamples_ = 0;
  int numChannels_ = 0;
};

}  // namespace octob
