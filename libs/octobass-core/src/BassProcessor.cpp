#include "octobass-core/BassProcessor.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace octob
{

BassProcessor::BassProcessor()
    : lowBandDelayWritePos_(0),
      currentIRLatency_(0),
      lowBandLevelDb_(DefaultBandLevelDb),
      highInputGainDb_(DefaultHighInputGainDb),
      highOutputGainDb_(DefaultHighOutputGainDb),
      outputGainDb_(DefaultOutputGainDb),
      dryWetMix_(DefaultDryWetMix),
      highBandMix_(DefaultHighBandMix),
      lowBandSolo_(false),
      highBandSolo_(false),
      currentMakeupLinear_(1.0f),
      makeupSmoothCoeff_(0.0f),
      lowBandLevelLinear_(1.0f),
      highInputGainLinear_(1.0f),
      highOutputGainLinear_(1.0f),
      outputGainLinear_(1.0f)
{
  // Configure IRProcessor for single-slot operation
  irProcessor_.setIRAEnabled(true);
  irProcessor_.setIRBEnabled(false);
  irProcessor_.setBlend(-1.0f);  // 100% IR A
  irProcessor_.setDynamicModeEnabled(false);
  irProcessor_.setOutputGain(0.0f);
}

BassProcessor::~BassProcessor() = default;

void BassProcessor::setSampleRate(SampleRate sampleRate)
{
  graphicEQ_.setSampleRate(sampleRate);
  crossover_.setSampleRate(sampleRate);
  compressor_.setSampleRate(sampleRate);
  namProcessor_.setSampleRate(sampleRate);
  irProcessor_.setSampleRate(sampleRate);
  noiseGate_.setSampleRate(sampleRate);

  // 5ms smoothing on static makeup gain to prevent clicks during parameter changes
  constexpr float kMakeupSmoothMs = 5.0f;
  float srFloat = static_cast<float>(sampleRate);
  makeupSmoothCoeff_ = 1.0f - std::exp(-1.0f / (srFloat * kMakeupSmoothMs * 0.001f));
}

void BassProcessor::setMaxBlockSize(FrameCount maxBlockSize)
{
  eqBuffer_.resize(maxBlockSize, 0.0f);
  lowBandBuffer_.resize(maxBlockSize, 0.0f);
  highBandBuffer_.resize(maxBlockSize, 0.0f);
  dryBuffer_.resize(maxBlockSize, 0.0f);
  dryHighBandBuffer_.resize(maxBlockSize, 0.0f);
  delayedLowBuffer_.resize(maxBlockSize, 0.0f);
  namProcessor_.setMaxBlockSize(maxBlockSize);
  irProcessor_.setMaxBlockSize(maxBlockSize);

  // Prepare-time call with no concurrent audio: size the delay line directly
  // and drop any staged buffer, which was sized for the old block size
  const std::lock_guard<std::mutex> lock(delayBufferSwapMutex_);
  hasPendingDelayBuffer_.store(false, std::memory_order_release);
  pendingDelayBuffer_ = std::vector<Sample>();
  retiredDelayBuffer_ = std::vector<Sample>();
  const int irLatency = irProcessor_.getLatencySamples();
  if (irLatency > 0)
  {
    lowBandDelayBuffer_.assign(static_cast<size_t>(irLatency) + maxBlockSize, 0.0f);
    lowBandDelayWritePos_ = 0;
  }
}

bool BassProcessor::loadImpulseResponse(const std::string& filepath, std::string& errorMessage)
{
  if (irProcessor_.loadImpulseResponse1(filepath, errorMessage))
  {
    currentIRPath_ = filepath;
    stageDelayBuffer(irProcessor_.getLatencySamples());
    return true;
  }
  return false;
}

void BassProcessor::clearImpulseResponse()
{
  irProcessor_.clearImpulseResponse1();
  currentIRPath_.clear();

  // The audio thread sees the IR latency drop to zero and skips the delay
  // path on its own; the delay buffer must not be freed here while that
  // thread may still be reading it. It is replaced on the next IR load.
  const std::lock_guard<std::mutex> lock(delayBufferSwapMutex_);
  hasPendingDelayBuffer_.store(false, std::memory_order_release);
  pendingDelayBuffer_ = std::vector<Sample>();
  retiredDelayBuffer_ = std::vector<Sample>();
}

bool BassProcessor::isIRLoaded() const
{
  return irProcessor_.isIR1Loaded();
}

std::string BassProcessor::getCurrentIRPath() const
{
  return currentIRPath_;
}

bool BassProcessor::loadNamModel(const std::string& filepath, std::string& errorMessage)
{
  if (namProcessor_.loadModel(filepath, errorMessage))
  {
    currentNamModelPath_ = filepath;
    return true;
  }
  return false;
}

void BassProcessor::clearNamModel()
{
  namProcessor_.clearModel();
  currentNamModelPath_.clear();
}

void BassProcessor::setNamQuality(float quality)
{
  namProcessor_.setQuality(static_cast<double>(quality));
}

int BassProcessor::getNamQualityLevels() const
{
  return namProcessor_.getNumQualityLevels();
}

void BassProcessor::setNamCalibrateInput(bool enabled)
{
  namProcessor_.setCalibrateInput(enabled);
}

void BassProcessor::setNamInputCalibrationLevel(float levelDbu)
{
  namProcessor_.setInputCalibrationLevel(levelDbu);
}

void BassProcessor::setNamOutputMode(int mode)
{
  mode = std::clamp(mode, static_cast<int>(NamOutputMode::Raw),
                    static_cast<int>(NamOutputMode::Calibrated));
  namProcessor_.setOutputMode(static_cast<NamOutputMode>(mode));
}

NamModelMetadata BassProcessor::getNamModelMetadata() const
{
  return namProcessor_.getModelMetadata();
}

bool BassProcessor::isNamModelLoaded() const
{
  return namProcessor_.isModelLoaded();
}

std::string BassProcessor::getCurrentNamModelPath() const
{
  return currentNamModelPath_;
}

void BassProcessor::setGraphicEQNode(int slot, bool active, float freqHz, float gainDb)
{
  graphicEQ_.setNode(slot, active, freqHz, gainDb);
}

void BassProcessor::setGraphicEQLowCut(bool active, float freqHz)
{
  graphicEQ_.setLowCut(active, freqHz);
}

void BassProcessor::setGraphicEQHighCut(bool active, float freqHz)
{
  graphicEQ_.setHighCut(active, freqHz);
}

void BassProcessor::setCrossoverFrequency(float frequencyHz)
{
  crossover_.setFrequency(frequencyHz);
}

void BassProcessor::setSquash(float amount)
{
  compressor_.setSquash(amount);
}

void BassProcessor::setCompressionMode(int mode)
{
  compressor_.setMode(mode);
}

void BassProcessor::setLowBandLevel(float levelDb)
{
  levelDb = std::clamp(levelDb, MinBandLevelDb, MaxBandLevelDb);
  if (lowBandLevelDb_ == levelDb)
    return;
  lowBandLevelDb_ = levelDb;
  lowBandLevelLinear_ = dbToLinear(levelDb);
}

void BassProcessor::setHighInputGain(float gainDb)
{
  gainDb = std::clamp(gainDb, MinHighInputGainDb, MaxHighInputGainDb);
  if (highInputGainDb_ == gainDb)
    return;
  highInputGainDb_ = gainDb;
  highInputGainLinear_ = dbToLinear(gainDb);
}

void BassProcessor::setHighOutputGain(float gainDb)
{
  gainDb = std::clamp(gainDb, MinHighOutputGainDb, MaxHighOutputGainDb);
  if (highOutputGainDb_ == gainDb)
    return;
  highOutputGainDb_ = gainDb;
  highOutputGainLinear_ = dbToLinear(gainDb);
}

void BassProcessor::setOutputGain(float gainDb)
{
  gainDb = std::clamp(gainDb, MinOutputGainDb, MaxOutputGainDb);
  if (outputGainDb_ == gainDb)
    return;
  outputGainDb_ = gainDb;
  outputGainLinear_ = dbToLinear(gainDb);
}

void BassProcessor::setDryWetMix(float mix)
{
  dryWetMix_ = std::clamp(mix, 0.0f, 1.0f);
}

void BassProcessor::setGateThreshold(float thresholdDb)
{
  noiseGate_.setThresholdDb(thresholdDb);
}

void BassProcessor::setHighBandMix(float mix)
{
  highBandMix_ = std::clamp(mix, 0.0f, 1.0f);
}

void BassProcessor::setLowBandSolo(bool solo)
{
  lowBandSolo_ = solo;
}

void BassProcessor::setHighBandSolo(bool solo)
{
  highBandSolo_ = solo;
}

void BassProcessor::processMono(const Sample* input, Sample* output, FrameCount numFrames)
{
  if (numFrames == 0)
    return;

  // Save dry input for dry/wet blend
  std::copy(input, input + numFrames, dryBuffer_.data());

  // Apply graphic EQ before crossover
  graphicEQ_.process(input, eqBuffer_.data(), numFrames);

  // Split into low and high bands
  crossover_.process(eqBuffer_.data(), lowBandBuffer_.data(), highBandBuffer_.data(), numFrames);

  // Save dry high band for wet/dry blend before any processing
  if (highBandMix_ < 1.0f)
    std::copy(highBandBuffer_.data(), highBandBuffer_.data() + numFrames,
              dryHighBandBuffer_.data());

  // High band chain: InputGain -> NAM -> IR -> OutputGain

  // 1. Apply input gain to high band
  for (FrameCount i = 0; i < numFrames; ++i)
    highBandBuffer_[i] *= highInputGainLinear_;

  // 2. NAM processing (passes through when no model loaded)
  namProcessor_.process(highBandBuffer_.data(), highBandBuffer_.data(), numFrames);

  // 3. Convolve high band through IR
  irProcessor_.processMono(highBandBuffer_.data(), highBandBuffer_.data(), numFrames);

  // 4. Apply output gain to high band
  for (FrameCount i = 0; i < numFrames; ++i)
    highBandBuffer_[i] *= highOutputGainLinear_;

  // 5. High band wet/dry blend
  if (highBandMix_ < 1.0f)
  {
    float wet = highBandMix_;
    float dry = 1.0f - wet;
    for (FrameCount i = 0; i < numFrames; ++i)
      highBandBuffer_[i] = dryHighBandBuffer_[i] * dry + highBandBuffer_[i] * wet;
  }

  // Pick up a delay buffer staged by an IR load on the message thread.
  // try_lock keeps this non-blocking: a missed swap is retried next block.
  if (hasPendingDelayBuffer_.load(std::memory_order_acquire))
  {
    std::unique_lock<std::mutex> lock(delayBufferSwapMutex_, std::try_to_lock);
    if (lock.owns_lock())
    {
      retiredDelayBuffer_ = std::move(lowBandDelayBuffer_);
      lowBandDelayBuffer_ = std::move(pendingDelayBuffer_);
      lowBandDelayWritePos_ = 0;
      hasPendingDelayBuffer_.store(false, std::memory_order_release);
    }
  }

  currentIRLatency_ = irProcessor_.getLatencySamples();

  // Apply delay compensation to low band. The size check skips the path until
  // a staged buffer large enough for the current latency has been swapped in.
  if (currentIRLatency_ > 0 &&
      lowBandDelayBuffer_.size() >= static_cast<size_t>(currentIRLatency_) + numFrames)
  {
    Sample* delayedLow = delayedLowBuffer_.data();
    writeToDelayBuffer(lowBandDelayBuffer_, lowBandDelayWritePos_, lowBandBuffer_.data(),
                       numFrames);
    readFromDelayBuffer(lowBandDelayBuffer_, lowBandDelayWritePos_, delayedLow, numFrames,
                        currentIRLatency_);
    std::copy(delayedLow, delayedLow + numFrames, lowBandBuffer_.data());
  }

  // Apply compression to low band (skip entirely when squash is off)
  if (compressor_.getSquash() > 0.0f)
  {
    compressor_.process(lowBandBuffer_.data(), lowBandBuffer_.data(), numFrames);

    // Static makeup gain: compensate level based on compressor parameters, not signal.
    // Smoothed over 5ms to prevent clicks during parameter changes.
    float targetMakeupLinear = dbToLinear(compressor_.getStaticMakeupDb());
    for (FrameCount i = 0; i < numFrames; ++i)
    {
      currentMakeupLinear_ += (targetMakeupLinear - currentMakeupLinear_) * makeupSmoothCoeff_;
      lowBandBuffer_[i] *= currentMakeupLinear_;
    }
  }

  // Apply band levels, sum, and output gain into highBandBuffer_ (reused as scratch)
  // Solo: include a band if it's soloed, or if the other band is NOT soloed (normal mode)
  bool includeLow = lowBandSolo_ || !highBandSolo_;
  bool includeHigh = highBandSolo_ || !lowBandSolo_;
  for (FrameCount i = 0; i < numFrames; ++i)
  {
    float low = includeLow ? lowBandBuffer_[i] * lowBandLevelLinear_ : 0.0f;
    float high = includeHigh ? highBandBuffer_[i] : 0.0f;
    highBandBuffer_[i] = (low + high) * outputGainLinear_;
  }

  // Noise gate: key signal is the clean pre-split input, applied to the wet sum
  noiseGate_.process(dryBuffer_.data(), highBandBuffer_.data(), highBandBuffer_.data(), numFrames);

  // Dry/wet blend
  for (FrameCount i = 0; i < numFrames; ++i)
    output[i] = dryBuffer_[i] * (1.0f - dryWetMix_) + highBandBuffer_[i] * dryWetMix_;
}

void BassProcessor::reset()
{
  graphicEQ_.reset();
  crossover_.reset();
  compressor_.reset();
  namProcessor_.reset();
  irProcessor_.reset();
  noiseGate_.reset();
  currentMakeupLinear_ = 1.0f;

  std::fill(eqBuffer_.begin(), eqBuffer_.end(), 0.0f);
  std::fill(lowBandBuffer_.begin(), lowBandBuffer_.end(), 0.0f);
  std::fill(highBandBuffer_.begin(), highBandBuffer_.end(), 0.0f);
  std::fill(dryBuffer_.begin(), dryBuffer_.end(), 0.0f);
  std::fill(dryHighBandBuffer_.begin(), dryHighBandBuffer_.end(), 0.0f);
  std::fill(delayedLowBuffer_.begin(), delayedLowBuffer_.end(), 0.0f);
  std::fill(lowBandDelayBuffer_.begin(), lowBandDelayBuffer_.end(), 0.0f);
  lowBandDelayWritePos_ = 0;
  currentIRLatency_ = 0;
}

int BassProcessor::getLatencySamples() const
{
  return irProcessor_.getLatencySamples();
}

void BassProcessor::stageDelayBuffer(int latencySamples)
{
  const std::lock_guard<std::mutex> lock(delayBufferSwapMutex_);
  retiredDelayBuffer_ = std::vector<Sample>();

  if (latencySamples <= 0)
  {
    hasPendingDelayBuffer_.store(false, std::memory_order_release);
    pendingDelayBuffer_ = std::vector<Sample>();
    return;
  }

  // lowBandBuffer_ is only resized at prepare time, so its size is stable
  // while audio is running
  pendingDelayBuffer_.assign(static_cast<size_t>(latencySamples) + lowBandBuffer_.size(), 0.0f);
  hasPendingDelayBuffer_.store(true, std::memory_order_release);
}

void BassProcessor::writeToDelayBuffer(std::vector<Sample>& buffer, size_t& writePos,
                                       const Sample* input, FrameCount numFrames)
{
  const size_t bufferSize = buffer.size();
  for (FrameCount i = 0; i < numFrames; ++i)
  {
    buffer[writePos] = input[i];
    writePos = (writePos + 1) % bufferSize;
  }
}

void BassProcessor::readFromDelayBuffer(const std::vector<Sample>& buffer, size_t writePos,
                                        Sample* output, FrameCount numFrames, int delaySamples)
{
  const size_t bufferSize = buffer.size();
  // Guard the unsigned cast: a negative latency would otherwise wrap to a huge offset
  const size_t delay = static_cast<size_t>(std::max(0, delaySamples));
  size_t readPos = (writePos + bufferSize - numFrames - delay) % bufferSize;
  for (FrameCount i = 0; i < numFrames; ++i)
  {
    output[i] = buffer[readPos];
    readPos = (readPos + 1) % bufferSize;
  }
}

}  // namespace octob
