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
  crossover_.setSampleRate(sampleRate);
  compressor_.setSampleRate(sampleRate);
  namProcessor_.setSampleRate(sampleRate);
  irProcessor_.setSampleRate(sampleRate);
}

void BassProcessor::setMaxBlockSize(FrameCount maxBlockSize)
{
  lowBandBuffer_.resize(maxBlockSize, 0.0f);
  highBandBuffer_.resize(maxBlockSize, 0.0f);
  dryBuffer_.resize(maxBlockSize, 0.0f);
  delayedLowBuffer_.resize(maxBlockSize, 0.0f);
  namProcessor_.setMaxBlockSize(maxBlockSize);
  irProcessor_.setMaxBlockSize(maxBlockSize);
  updateDelayBuffer();
}

bool BassProcessor::loadImpulseResponse(const std::string& filepath, std::string& errorMessage)
{
  if (irProcessor_.loadImpulseResponse1(filepath, errorMessage))
  {
    currentIRPath_ = filepath;
    return true;
  }
  return false;
}

void BassProcessor::clearImpulseResponse()
{
  irProcessor_.clearImpulseResponse1();
  currentIRPath_.clear();
  currentIRLatency_ = 0;
  lowBandDelayBuffer_.clear();
  lowBandDelayWritePos_ = 0;
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

bool BassProcessor::isNamModelLoaded() const
{
  return namProcessor_.isModelLoaded();
}

std::string BassProcessor::getCurrentNamModelPath() const
{
  return currentNamModelPath_;
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
  lowBandLevelDb_ = clamp(levelDb, MinBandLevelDb, MaxBandLevelDb);
  lowBandLevelLinear_ = dbToLinear(lowBandLevelDb_);
}

void BassProcessor::setHighInputGain(float gainDb)
{
  highInputGainDb_ = clamp(gainDb, MinHighInputGainDb, MaxHighInputGainDb);
  highInputGainLinear_ = dbToLinear(highInputGainDb_);
}

void BassProcessor::setHighOutputGain(float gainDb)
{
  highOutputGainDb_ = clamp(gainDb, MinHighOutputGainDb, MaxHighOutputGainDb);
  highOutputGainLinear_ = dbToLinear(highOutputGainDb_);
}

void BassProcessor::setOutputGain(float gainDb)
{
  outputGainDb_ = clamp(gainDb, MinOutputGainDb, MaxOutputGainDb);
  outputGainLinear_ = dbToLinear(outputGainDb_);
}

void BassProcessor::setDryWetMix(float mix)
{
  dryWetMix_ = clamp(mix, 0.0f, 1.0f);
}

void BassProcessor::processMono(const Sample* input, Sample* output, FrameCount numFrames)
{
  if (numFrames == 0)
    return;

  // Save dry input for dry/wet blend
  std::copy(input, input + numFrames, dryBuffer_.data());

  // Split into low and high bands
  crossover_.process(input, lowBandBuffer_.data(), highBandBuffer_.data(), numFrames);

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

  // Update latency compensation if IR latency changed
  int irLatency = irProcessor_.getLatencySamples();
  if (irLatency != currentIRLatency_)
  {
    currentIRLatency_ = irLatency;
    updateDelayBuffer();
  }

  // Apply delay compensation to low band
  if (currentIRLatency_ > 0 && !lowBandDelayBuffer_.empty())
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
  }

  // Apply band levels and sum
  for (FrameCount i = 0; i < numFrames; ++i)
  {
    float wet = lowBandBuffer_[i] * lowBandLevelLinear_ + highBandBuffer_[i];

    // Apply output gain
    wet *= outputGainLinear_;

    // Dry/wet blend
    output[i] = dryBuffer_[i] * (1.0f - dryWetMix_) + wet * dryWetMix_;
  }
}

void BassProcessor::reset()
{
  crossover_.reset();
  compressor_.reset();
  namProcessor_.reset();
  irProcessor_.reset();

  std::fill(lowBandBuffer_.begin(), lowBandBuffer_.end(), 0.0f);
  std::fill(highBandBuffer_.begin(), highBandBuffer_.end(), 0.0f);
  std::fill(dryBuffer_.begin(), dryBuffer_.end(), 0.0f);
  std::fill(delayedLowBuffer_.begin(), delayedLowBuffer_.end(), 0.0f);
  std::fill(lowBandDelayBuffer_.begin(), lowBandDelayBuffer_.end(), 0.0f);
  lowBandDelayWritePos_ = 0;
  currentIRLatency_ = 0;
}

int BassProcessor::getLatencySamples() const
{
  return irProcessor_.getLatencySamples();
}

void BassProcessor::updateDelayBuffer()
{
  if (currentIRLatency_ > 0)
  {
    size_t bufferSize = static_cast<size_t>(currentIRLatency_) + lowBandBuffer_.size();
    if (lowBandDelayBuffer_.size() < bufferSize)
    {
      lowBandDelayBuffer_.resize(bufferSize, 0.0f);
    }
  }
  else
  {
    lowBandDelayBuffer_.clear();
    lowBandDelayWritePos_ = 0;
  }
}

float BassProcessor::clamp(float value, float minVal, float maxVal)
{
  return std::max(minVal, std::min(maxVal, value));
}

float BassProcessor::dbToLinear(float db)
{
  return std::pow(10.0f, db / 20.0f);
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
  size_t readPos =
      (writePos + bufferSize - numFrames - static_cast<size_t>(delaySamples)) % bufferSize;
  for (FrameCount i = 0; i < numFrames; ++i)
  {
    output[i] = buffer[readPos];
    readPos = (readPos + 1) % bufferSize;
  }
}

}  // namespace octob
