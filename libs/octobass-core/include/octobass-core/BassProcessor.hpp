#pragma once

#include <octobir-core/IRProcessor.hpp>
#include <string>
#include <vector>

#include "Compressor.hpp"
#include "Crossover.hpp"
#include "NamProcessor.hpp"
#include "Types.hpp"

namespace octob
{

class BassProcessor
{
 public:
  BassProcessor();
  ~BassProcessor();

  void setSampleRate(SampleRate sampleRate);
  void setMaxBlockSize(FrameCount maxBlockSize);

  // IR loading (delegates to internal IRProcessor, single slot)
  bool loadImpulseResponse(const std::string& filepath, std::string& errorMessage);
  void clearImpulseResponse();
  bool isIRLoaded() const;
  std::string getCurrentIRPath() const;

  // NAM model loading
  bool loadNamModel(const std::string& filepath, std::string& errorMessage);
  void clearNamModel();
  bool isNamModelLoaded() const;
  std::string getCurrentNamModelPath() const;

  // Crossover
  void setCrossoverFrequency(float frequencyHz);

  // Compressor
  void setSquash(float amount);
  void setCompressionMode(int mode);

  // Levels
  void setLowBandLevel(float levelDb);
  void setHighInputGain(float gainDb);
  void setHighOutputGain(float gainDb);
  void setOutputGain(float gainDb);
  void setDryWetMix(float mix);

  // Processing
  void processMono(const Sample* input, Sample* output, FrameCount numFrames);

  void reset();

  // Queries
  int getLatencySamples() const;
  float getCrossoverFrequency() const { return crossover_.getFrequency(); }
  float getSquash() const { return compressor_.getSquash(); }
  int getCompressionMode() const { return compressor_.getMode(); }
  float getLowBandLevel() const { return lowBandLevelDb_; }
  float getHighInputGain() const { return highInputGainDb_; }
  float getHighOutputGain() const { return highOutputGainDb_; }
  float getOutputGain() const { return outputGainDb_; }
  float getDryWetMix() const { return dryWetMix_; }

 private:
  Crossover crossover_;
  Compressor compressor_;
  NamProcessor namProcessor_;
  IRProcessor irProcessor_;

  std::vector<Sample> lowBandBuffer_;
  std::vector<Sample> highBandBuffer_;
  std::vector<Sample> dryBuffer_;
  std::vector<Sample> delayedLowBuffer_;

  // Delay compensation for low band path
  std::vector<Sample> lowBandDelayBuffer_;
  size_t lowBandDelayWritePos_;
  int currentIRLatency_;

  float lowBandLevelDb_;
  float highInputGainDb_;
  float highOutputGainDb_;
  float outputGainDb_;
  float dryWetMix_;

  float lowBandLevelLinear_;
  float highInputGainLinear_;
  float highOutputGainLinear_;
  float outputGainLinear_;

  std::string currentIRPath_;
  std::string currentNamModelPath_;

  void updateDelayBuffer();

  static float clamp(float value, float minVal, float maxVal);
  static float dbToLinear(float db);

  static void writeToDelayBuffer(std::vector<Sample>& buffer, size_t& writePos, const Sample* input,
                                 FrameCount numFrames);
  static void readFromDelayBuffer(const std::vector<Sample>& buffer, size_t writePos,
                                  Sample* output, FrameCount numFrames, int delaySamples);
};

}  // namespace octob
