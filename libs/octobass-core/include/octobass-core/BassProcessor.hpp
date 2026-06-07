#pragma once

#include <atomic>
#include <mutex>
#include <octobir-core/IRProcessor.hpp>
#include <string>
#include <vector>

#include "Compressor.hpp"
#include "Crossover.hpp"
#include "GraphicEQ.hpp"
#include "NamProcessor.hpp"
#include "NoiseGate.hpp"
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

  // Quality/CPU trade-off for slimmable NAM models (0.0 slimmest, 1.0 full).
  // Message thread only -- not real-time safe.
  void setNamQuality(float quality);

  // Discrete quality levels of the loaded NAM model: 0 = no model,
  // 1 = no quality options, >1 = selectable levels. Message thread only.
  int getNamQualityLevels() const;

  // Crossover
  void setCrossoverFrequency(float frequencyHz);

  // Compressor
  void setSquash(float amount);
  void setCompressionMode(int mode);

  // Noise gate
  void setGateThreshold(float thresholdDb);

  // High band blend
  void setHighBandMix(float mix);

  // Solo (mutually exclusive at caller level)
  void setLowBandSolo(bool solo);
  void setHighBandSolo(bool solo);

  // Graphic EQ
  void setGraphicEQNode(int slot, bool active, float freqHz, float gainDb);
  void setGraphicEQLowCut(bool active, float freqHz);
  void setGraphicEQHighCut(bool active, float freqHz);

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
  float getGateThreshold() const { return noiseGate_.getThresholdDb(); }
  float getHighBandMix() const { return highBandMix_; }
  bool getLowBandSolo() const { return lowBandSolo_; }
  bool getHighBandSolo() const { return highBandSolo_; }

 private:
  GraphicEQ graphicEQ_;
  Crossover crossover_;
  Compressor compressor_;
  NamProcessor namProcessor_;
  IRProcessor irProcessor_;
  NoiseGate noiseGate_;

  std::vector<Sample> eqBuffer_;
  std::vector<Sample> lowBandBuffer_;
  std::vector<Sample> highBandBuffer_;
  std::vector<Sample> dryBuffer_;
  std::vector<Sample> dryHighBandBuffer_;
  std::vector<Sample> delayedLowBuffer_;

  // Delay compensation for the low band path. The audio thread owns
  // lowBandDelayBuffer_; the message thread stages a replacement in
  // pendingDelayBuffer_ on IR load and the audio thread swaps it in via
  // try_lock, so neither thread ever allocates or frees on the audio path.
  // The displaced buffer parks in retiredDelayBuffer_ until the next
  // message-thread call releases it.
  std::vector<Sample> lowBandDelayBuffer_;
  std::vector<Sample> pendingDelayBuffer_;
  std::vector<Sample> retiredDelayBuffer_;
  std::mutex delayBufferSwapMutex_;
  std::atomic<bool> hasPendingDelayBuffer_{false};
  size_t lowBandDelayWritePos_;
  int currentIRLatency_;

  float lowBandLevelDb_;
  float highInputGainDb_;
  float highOutputGainDb_;
  float outputGainDb_;
  float dryWetMix_;
  float highBandMix_;
  bool lowBandSolo_;
  bool highBandSolo_;

  // Static makeup gain smoother (5ms to prevent clicks on parameter change)
  float currentMakeupLinear_;
  float makeupSmoothCoeff_;

  float lowBandLevelLinear_;
  float highInputGainLinear_;
  float highOutputGainLinear_;
  float outputGainLinear_;

  std::string currentIRPath_;
  std::string currentNamModelPath_;

  void stageDelayBuffer(int latencySamples);

  static void writeToDelayBuffer(std::vector<Sample>& buffer, size_t& writePos, const Sample* input,
                                 FrameCount numFrames);
  static void readFromDelayBuffer(const std::vector<Sample>& buffer, size_t writePos,
                                  Sample* output, FrameCount numFrames, int delaySamples);
};

}  // namespace octob
