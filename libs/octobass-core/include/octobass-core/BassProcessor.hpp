#pragma once

#include <array>
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

  bool loadNamModel(NamSlot slot, const std::string& filepath, std::string& errorMessage);
  void clearNamModel(NamSlot slot);
  bool isNamModelLoaded(NamSlot slot) const;
  std::string getCurrentNamModelPath(NamSlot slot) const;

  // Quality/CPU trade-off for slimmable NAM models (0.0 slimmest, 1.0 full).
  // Message thread only -- not real-time safe.
  void setNamQuality(NamSlot slot, float quality);

  int getNamQualityLevels(NamSlot slot) const;

  void setNamCalibrateInput(NamSlot slot, bool enabled);
  void setNamInputCalibrationLevel(NamSlot slot, float levelDbu);
  void setNamOutputMode(NamSlot slot, int mode);
  NamModelMetadata getNamModelMetadata(NamSlot slot) const;
  void setNamEnabled(NamSlot slot, bool enabled);
  bool isNamEnabled(NamSlot slot) const;

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
  std::array<NamProcessor, kNumNamSlots> namSlots_;
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

  mutable std::mutex irPathMutex_;
  std::string currentIRPath_;

  NamProcessor& namSlot(NamSlot slot) { return namSlots_[slotIndex(slot)]; }
  const NamProcessor& namSlot(NamSlot slot) const { return namSlots_[slotIndex(slot)]; }
  static size_t slotIndex(NamSlot slot) { return static_cast<size_t>(slot); }
  void setCurrentIRPath(std::string path);

  void stageDelayBuffer(int latencySamples);

  static void writeToDelayBuffer(std::vector<Sample>& buffer, size_t& writePos, const Sample* input,
                                 FrameCount numFrames);
  static void readFromDelayBuffer(const std::vector<Sample>& buffer, size_t writePos,
                                  Sample* output, FrameCount numFrames, int delaySamples);
};

}  // namespace octob
