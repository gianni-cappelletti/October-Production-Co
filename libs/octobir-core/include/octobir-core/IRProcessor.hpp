#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "AudioBuffer.hpp"
#include "IRLoader.hpp"
#include "Types.hpp"

class WDL_ImpulseBuffer;          // NOLINT(readability-identifier-naming)
class WDL_ConvolutionEngine_Div;  // NOLINT(readability-identifier-naming)

namespace octob
{

class IRProcessor
{
 public:
  IRProcessor();
  ~IRProcessor();

  bool loadImpulseResponse1(const std::string& filepath, std::string& errorMessage);
  bool loadImpulseResponse2(const std::string& filepath, std::string& errorMessage);
  void clearImpulseResponse1();
  void clearImpulseResponse2();

  void setSampleRate(SampleRate sampleRate);
  void setMaxBlockSize(FrameCount maxBlockSize);
  void setBlend(float blend);

  void setIRAEnabled(bool enabled);
  void setIRBEnabled(bool enabled);
  void setDynamicModeEnabled(bool enabled);
  void setSidechainEnabled(bool enabled);
  void setThreshold(float thresholdDb);
  void setRangeDb(float rangeDb);
  void setKneeWidthDb(float kneeDb);
  void setDetectionMode(int mode);
  void setAttackTime(float attackTimeMs);
  void setReleaseTime(float releaseTimeMs);
  void setOutputGain(float gainDb);
  void setIRATrimGain(float gainDb);
  void setIRBTrimGain(float gainDb);

  void processMono(const Sample* input, Sample* output, FrameCount numFrames);
  void processStereo(const Sample* inputL, const Sample* inputR, Sample* outputL, Sample* outputR,
                     FrameCount numFrames);
  void processDualMono(const Sample* inputL, const Sample* inputR, Sample* outputL, Sample* outputR,
                       FrameCount numFrames);
  void processMonoToStereo(const Sample* input, Sample* outputL, Sample* outputR,
                           FrameCount numFrames);

  void processMonoWithSidechain(const Sample* input, const Sample* sidechain, Sample* output,
                                FrameCount numFrames);
  void processMonoToStereoWithSidechain(const Sample* input, const Sample* sidechain,
                                        Sample* outputL, Sample* outputR, FrameCount numFrames);
  void processStereoWithSidechain(const Sample* inputL, const Sample* inputR,
                                  const Sample* sidechainL, const Sample* sidechainR,
                                  Sample* outputL, Sample* outputR, FrameCount numFrames);
  void processDualMonoWithSidechain(const Sample* inputL, const Sample* inputR,
                                    const Sample* sidechainL, const Sample* sidechainR,
                                    Sample* outputL, Sample* outputR, FrameCount numFrames);

  bool isIR1Loaded() const { return ir1Loaded_.load(); }
  bool isIR2Loaded() const { return ir2Loaded_.load(); }
  std::string getCurrentIR1Path() const { return currentIR1Path_; }
  std::string getCurrentIR2Path() const { return currentIR2Path_; }
  SampleRate getIR1SampleRate() const;
  SampleRate getIR2SampleRate() const;
  size_t getIR1NumSamples() const;
  size_t getIR2NumSamples() const;
  int getNumIR1Channels() const;
  int getNumIR2Channels() const;
  int getLatencySamples() const;
  float getBlend() const { return blend_; }

  float calculateDynamicBlend(float inputLevelDb) const;

  bool getIRAEnabled() const { return irAEnabled_; }
  bool getIRBEnabled() const { return irBEnabled_; }
  bool getDynamicModeEnabled() const { return dynamicModeEnabled_; }
  bool getSidechainEnabled() const { return sidechainEnabled_; }
  float getThreshold() const { return thresholdDb_; }
  float getRangeDb() const { return rangeDb_; }
  float getKneeWidthDb() const { return kneeWidthDb_; }
  int getDetectionMode() const { return detectionMode_; }
  float getAttackTime() const { return attackTimeMs_; }
  float getReleaseTime() const { return releaseTimeMs_; }
  float getOutputGain() const { return outputGainDb_; }
  float getIRATrimGain() const { return irATrimGainDb_; }
  float getIRBTrimGain() const { return irBTrimGainDb_; }
  float getCurrentInputLevel() const { return currentInputLevelDb_; }
  float getCurrentBlend() const { return currentBlend_; }

  void swapIRSlots();

  void reset();

 private:
  std::unique_ptr<WDL_ImpulseBuffer> impulseBuffer1_;
  std::unique_ptr<WDL_ConvolutionEngine_Div> convolutionEngine1_;
  std::unique_ptr<IRLoader> irLoader1_;

  std::unique_ptr<WDL_ImpulseBuffer> impulseBuffer2_;
  std::unique_ptr<WDL_ConvolutionEngine_Div> convolutionEngine2_;
  std::unique_ptr<IRLoader> irLoader2_;

  SampleRate sampleRate_ = 44100.0;
  std::string currentIR1Path_;
  std::string currentIR2Path_;
  std::atomic<bool> ir1Loaded_{false};
  std::atomic<bool> ir2Loaded_{false};
  int latencySamples1_ = 0;
  int latencySamples2_ = 0;

  std::unique_ptr<WDL_ConvolutionEngine_Div> stagingEngine1_;
  bool stagingLoaded1_ = false;
  int stagingLatency1_ = 0;

  std::unique_ptr<WDL_ConvolutionEngine_Div> stagingEngine2_;
  bool stagingLoaded2_ = false;
  int stagingLatency2_ = 0;

  std::mutex pendingMutex1_;
  std::mutex pendingMutex2_;
  std::atomic<bool> ir1Pending_{false};
  std::atomic<bool> ir2Pending_{false};
  float blend_ = 0.0f;

  bool irAEnabled_ = true;
  bool irBEnabled_ = true;
  bool dynamicModeEnabled_ = false;
  bool sidechainEnabled_ = false;
  float thresholdDb_ = -30.0f;
  float rangeDb_ = 20.0f;
  float kneeWidthDb_ = 5.0f;
  int detectionMode_ = 0;
  std::vector<float> rmsBuffer_;
  size_t rmsBufferIndex_ = 0;
  size_t rmsBufferSize_ = 0;
  static constexpr float RmsWindowMs = 10.0f;
  float attackTimeMs_ = 50.0f;
  float releaseTimeMs_ = 200.0f;
  float outputGainDb_ = 0.0f;
  float irATrimGainDb_ = 0.0f;
  float irBTrimGainDb_ = 0.0f;
  float irATrimGainLinear_ = 1.0f;
  float irBTrimGainLinear_ = 1.0f;

  float currentInputLevelDb_ = -96.0f;
  float currentBlend_ = 0.0f;
  float smoothedBlend_ = 0.0f;
  float outputGainLinear_ = 1.0f;
  float attackCoeff_ = 0.0f;
  float releaseCoeff_ = 0.0f;
  float blendSmoothCoeff_ = 0.0f;

  std::vector<Sample> scratchL_;
  std::vector<Sample> scratchR_;

  std::vector<Sample> dryDelayBufferL_;
  std::vector<Sample> dryDelayBufferR_;
  std::vector<Sample> ir1DelayBufferL_;
  std::vector<Sample> ir1DelayBufferR_;
  std::vector<Sample> ir2DelayBufferL_;
  std::vector<Sample> ir2DelayBufferR_;
  size_t dryDelayWritePosL_ = 0;
  size_t dryDelayWritePosR_ = 0;
  size_t ir1DelayWritePosL_ = 0;
  size_t ir1DelayWritePosR_ = 0;
  size_t ir2DelayWritePosL_ = 0;
  size_t ir2DelayWritePosR_ = 0;
  std::atomic<int> maxLatencySamples_{0};

  struct BlendGains
  {
    float gain1;
    float gain2;
  };

  BlendGains resolveBlendGains(float inputLevelDb, FrameCount numFrames, bool applySmoothing,
                               bool hasIR1, bool hasIR2);

  static float detectPeakLevel(const Sample* buffer, FrameCount numFrames);
  float detectRMSLevel(const Sample* buffer, FrameCount numFrames);
  void updateSmoothingCoefficients();
  void updateRMSBufferSize();
  void applyOutputGain(Sample* buffer, FrameCount numFrames) const;
  void updateDelayBuffers();
  static void writeToDelayBuffer(std::vector<Sample>& buffer, size_t& writePos, const Sample* input,
                                 FrameCount numFrames);
  static void readFromDelayBuffer(const std::vector<Sample>& buffer, size_t writePos,
                                  Sample* output, FrameCount numFrames, int delaySamples);
  void applyPendingIRUpdates();
};

}  // namespace octob
