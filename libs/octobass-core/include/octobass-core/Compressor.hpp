#pragma once

#include "BusCompressor.hpp"
#include "CompressorMode.hpp"
#include "FETCompressor.hpp"
#include "OptoCompressor.hpp"
#include "VCACompressor.hpp"

namespace octob
{

// Wrapper owning all four compressor modes.
// Delegates processing to the active mode selected by setMode().
class Compressor
{
 public:
  Compressor();

  void setSampleRate(SampleRate sampleRate);
  void setSquash(float amount);
  void setMode(int mode);

  void process(const Sample* input, Sample* output, FrameCount numFrames);
  void reset();

  float getSquash() const { return squash_; }
  int getMode() const { return mode_; }
  float getGainReductionDb() const;
  float getStaticMakeupDb() const;

 private:
  VCACompressor vca_;
  OptoCompressor opto_;
  FETCompressor fet_;
  BusCompressor bus_;
  CompressorMode* activeMode_;

  SampleRate sampleRate_;
  float squash_;
  int mode_;
};

}  // namespace octob
