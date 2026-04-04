#include "octobir-core/IRProcessor.hpp"

#include <convoengine.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

namespace octob
{

IRProcessor::IRProcessor()
    : impulseBuffer1_(new WDL_ImpulseBuffer()),
      convolutionEngine1_(new WDL_ConvolutionEngine_Div()),
      irLoader1_(new IRLoader()),
      impulseBuffer2_(new WDL_ImpulseBuffer()),
      convolutionEngine2_(new WDL_ConvolutionEngine_Div()),
      irLoader2_(new IRLoader())
{
}

IRProcessor::~IRProcessor() = default;

bool IRProcessor::loadImpulseResponse1(const std::string& filepath, std::string& errorMessage)
{
  auto stagingBuffer = std::unique_ptr<WDL_ImpulseBuffer>(new WDL_ImpulseBuffer());
  auto stagingEngine = std::unique_ptr<WDL_ConvolutionEngine_Div>(new WDL_ConvolutionEngine_Div());
  auto stagingLoader = std::unique_ptr<IRLoader>(new IRLoader());

  const IRLoadResult result = stagingLoader->loadFromFile(filepath);
  if (!result.success)
  {
    errorMessage = result.errorMessage;
    return false;
  }

  if (!stagingLoader->resampleAndInitialize(*stagingBuffer, sampleRate_))
  {
    errorMessage = "Failed to resample IR to target sample rate";
    return false;
  }

  const int irLength = stagingBuffer->GetLength();
  const int irChannels = stagingBuffer->GetNumChannels();
  const double irSampleRate = stagingBuffer->samplerate;

  if (irLength <= 0)
  {
    errorMessage = "IR buffer length is invalid: " + std::to_string(irLength);
    return false;
  }

  if (irChannels <= 0)
  {
    errorMessage = "IR buffer channels is invalid: " + std::to_string(irChannels);
    return false;
  }

  if (irSampleRate <= 0)
  {
    errorMessage = "IR sample rate is invalid: " + std::to_string(irSampleRate);
    return false;
  }

  const int latency = stagingEngine->SetImpulse(stagingBuffer.get(), 64, 0, 0, 0);
  if (latency < 0)
  {
    errorMessage = "Failed to initialize convolution engine with IR (returned " +
                   std::to_string(latency) + "). IR: " + std::to_string(irLength) + " samples, " +
                   std::to_string(irChannels) + " channels, " + std::to_string(irSampleRate) +
                   " Hz";
    return false;
  }

  {
    std::lock_guard<std::mutex> lock(pendingMutex1_);
    stagingEngine1_ = std::move(stagingEngine);
    stagingLoaded1_ = true;
    stagingLatency1_ = latency;
    ir1Pending_.store(true, std::memory_order_release);
  }

  impulseBuffer1_ = std::move(stagingBuffer);
  irLoader1_ = std::move(stagingLoader);
  currentIR1Path_ = filepath;
  errorMessage.clear();
  return true;
}

bool IRProcessor::loadImpulseResponse2(const std::string& filepath, std::string& errorMessage)
{
  auto stagingBuffer = std::unique_ptr<WDL_ImpulseBuffer>(new WDL_ImpulseBuffer());
  auto stagingEngine = std::unique_ptr<WDL_ConvolutionEngine_Div>(new WDL_ConvolutionEngine_Div());
  auto stagingLoader = std::unique_ptr<IRLoader>(new IRLoader());

  const IRLoadResult result = stagingLoader->loadFromFile(filepath);
  if (!result.success)
  {
    errorMessage = result.errorMessage;
    return false;
  }

  if (!stagingLoader->resampleAndInitialize(*stagingBuffer, sampleRate_))
  {
    errorMessage = "Failed to resample IR2 to target sample rate";
    return false;
  }

  const int irLength = stagingBuffer->GetLength();
  const int irChannels = stagingBuffer->GetNumChannels();
  const double irSampleRate = stagingBuffer->samplerate;

  if (irLength <= 0)
  {
    errorMessage = "IR2 buffer length is invalid: " + std::to_string(irLength);
    return false;
  }

  if (irChannels <= 0)
  {
    errorMessage = "IR2 buffer channels is invalid: " + std::to_string(irChannels);
    return false;
  }

  if (irSampleRate <= 0)
  {
    errorMessage = "IR2 sample rate is invalid: " + std::to_string(irSampleRate);
    return false;
  }

  const int latency = stagingEngine->SetImpulse(stagingBuffer.get(), 64, 0, 0, 0);
  if (latency < 0)
  {
    errorMessage = "Failed to initialize convolution engine with IR2 (returned " +
                   std::to_string(latency) + "). IR2: " + std::to_string(irLength) + " samples, " +
                   std::to_string(irChannels) + " channels, " + std::to_string(irSampleRate) +
                   " Hz";
    return false;
  }

  {
    std::lock_guard<std::mutex> lock(pendingMutex2_);
    stagingEngine2_ = std::move(stagingEngine);
    stagingLoaded2_ = true;
    stagingLatency2_ = latency;
    ir2Pending_.store(true, std::memory_order_release);
  }

  impulseBuffer2_ = std::move(stagingBuffer);
  irLoader2_ = std::move(stagingLoader);
  currentIR2Path_ = filepath;
  errorMessage.clear();
  return true;
}

void IRProcessor::clearImpulseResponse1()
{
  {
    std::lock_guard<std::mutex> lock(pendingMutex1_);
    stagingEngine1_ = std::unique_ptr<WDL_ConvolutionEngine_Div>(new WDL_ConvolutionEngine_Div());
    stagingLoaded1_ = false;
    stagingLatency1_ = 0;
    ir1Pending_.store(true, std::memory_order_release);
  }
  currentIR1Path_.clear();
}

void IRProcessor::clearImpulseResponse2()
{
  {
    std::lock_guard<std::mutex> lock(pendingMutex2_);
    stagingEngine2_ = std::unique_ptr<WDL_ConvolutionEngine_Div>(new WDL_ConvolutionEngine_Div());
    stagingLoaded2_ = false;
    stagingLatency2_ = 0;
    ir2Pending_.store(true, std::memory_order_release);
  }
  currentIR2Path_.clear();
}

void IRProcessor::setSampleRate(SampleRate sampleRate)
{
  if (sampleRate_ != sampleRate)
  {
    sampleRate_ = sampleRate;
    updateSmoothingCoefficients();
    updateRMSBufferSize();

    if (ir1Loaded_.load(std::memory_order_relaxed) && impulseBuffer1_->GetLength() > 0)
    {
      irLoader1_->resampleAndInitialize(*impulseBuffer1_, sampleRate_);
      auto stagingEngine =
          std::unique_ptr<WDL_ConvolutionEngine_Div>(new WDL_ConvolutionEngine_Div());
      const int latency = stagingEngine->SetImpulse(impulseBuffer1_.get(), 64, 0, 0, 0);

      std::lock_guard<std::mutex> lock(pendingMutex1_);
      stagingEngine1_ = std::move(stagingEngine);
      stagingLoaded1_ = true;
      stagingLatency1_ = latency >= 0 ? latency : 0;
      ir1Pending_.store(true, std::memory_order_release);
    }

    if (ir2Loaded_.load(std::memory_order_relaxed) && impulseBuffer2_->GetLength() > 0)
    {
      irLoader2_->resampleAndInitialize(*impulseBuffer2_, sampleRate_);
      auto stagingEngine =
          std::unique_ptr<WDL_ConvolutionEngine_Div>(new WDL_ConvolutionEngine_Div());
      const int latency = stagingEngine->SetImpulse(impulseBuffer2_.get(), 64, 0, 0, 0);

      std::lock_guard<std::mutex> lock(pendingMutex2_);
      stagingEngine2_ = std::move(stagingEngine);
      stagingLoaded2_ = true;
      stagingLatency2_ = latency >= 0 ? latency : 0;
      ir2Pending_.store(true, std::memory_order_release);
    }
  }
}

void IRProcessor::setMaxBlockSize(FrameCount maxBlockSize)
{
  scratchL_.resize(maxBlockSize);
  scratchR_.resize(maxBlockSize);
}

void IRProcessor::setBlend(float blend)
{
  blend_ = std::max(-1.0f, std::min(1.0f, blend));
  if (!dynamicModeEnabled_)
  {
    currentBlend_ = blend_;
  }
}

void IRProcessor::setDynamicModeEnabled(bool enabled)
{
  dynamicModeEnabled_ = enabled;
  if (!enabled)
  {
    currentBlend_ = blend_;
  }
}

void IRProcessor::setSidechainEnabled(bool enabled)
{
  sidechainEnabled_ = enabled;
}

void IRProcessor::setThreshold(float thresholdDb)
{
  thresholdDb_ = std::max(-60.0f, std::min(0.0f, thresholdDb));
}

void IRProcessor::setRangeDb(float rangeDb)
{
  rangeDb_ = std::max(1.0f, std::min(60.0f, rangeDb));
}

void IRProcessor::setKneeWidthDb(float kneeDb)
{
  kneeWidthDb_ = std::max(0.0f, std::min(20.0f, kneeDb));
}

void IRProcessor::setDetectionMode(int mode)
{
  detectionMode_ = std::max(0, std::min(1, mode));
  updateRMSBufferSize();
}

void IRProcessor::updateRMSBufferSize()
{
  if (sampleRate_ <= 0.0)
  {
    return;
  }

  size_t newSize = static_cast<size_t>((kRMSWindowMs_ / 1000.0f) * sampleRate_);
  if (newSize != rmsBufferSize_)
  {
    rmsBufferSize_ = std::max(size_t(1), newSize);
    rmsBuffer_.resize(rmsBufferSize_, 0.0f);
    rmsBufferIndex_ = 0;
  }
}

void IRProcessor::setAttackTime(float attackTimeMs)
{
  const float clamped = std::max(1.0f, std::min(500.0f, attackTimeMs));
  if (clamped != attackTimeMs_)
  {
    attackTimeMs_ = clamped;
    updateSmoothingCoefficients();
  }
}

void IRProcessor::setReleaseTime(float releaseTimeMs)
{
  const float clamped = std::max(1.0f, std::min(1000.0f, releaseTimeMs));
  if (clamped != releaseTimeMs_)
  {
    releaseTimeMs_ = clamped;
    updateSmoothingCoefficients();
  }
}

void IRProcessor::setOutputGain(float gainDb)
{
  const float clamped = std::max(-24.0f, std::min(24.0f, gainDb));
  if (clamped != outputGainDb_)
  {
    outputGainDb_ = clamped;
    outputGainLinear_ = std::pow(10.0f, outputGainDb_ / 20.0f);
  }
}

void IRProcessor::setIRATrimGain(float gainDb)
{
  const float clamped = std::max(-12.0f, std::min(12.0f, gainDb));
  if (clamped != irATrimGainDb_)
  {
    irATrimGainDb_ = clamped;
    irATrimGainLinear_ = std::pow(10.0f, irATrimGainDb_ / 20.0f);
  }
}

void IRProcessor::setIRBTrimGain(float gainDb)
{
  const float clamped = std::max(-12.0f, std::min(12.0f, gainDb));
  if (clamped != irBTrimGainDb_)
  {
    irBTrimGainDb_ = clamped;
    irBTrimGainLinear_ = std::pow(10.0f, irBTrimGainDb_ / 20.0f);
  }
}

void IRProcessor::setIRAEnabled(bool enabled)
{
  irAEnabled_ = enabled;
}

void IRProcessor::setIRBEnabled(bool enabled)
{
  irBEnabled_ = enabled;
}

IRProcessor::BlendGains IRProcessor::resolveBlendGains(float inputLevelDb, FrameCount numFrames,
                                                       bool applySmoothing, bool hasIR1,
                                                       bool hasIR2)
{
  float blendToUse = blend_;
  if (dynamicModeEnabled_ && applySmoothing)
  {
    float targetBlend = calculateDynamicBlend(inputLevelDb);
    float coeff = (targetBlend > smoothedBlend_) ? attackCoeff_ : releaseCoeff_;
    float coeffAdjusted = std::pow(coeff, static_cast<float>(numFrames));
    smoothedBlend_ = smoothedBlend_ * coeffAdjusted + targetBlend * (1.0f - coeffAdjusted);
    blendToUse = smoothedBlend_;
    currentBlend_ = blendToUse;
  }
  else if (dynamicModeEnabled_)
  {
    blendToUse = smoothedBlend_;
    currentBlend_ = blendToUse;
  }
  else if (applySmoothing)
  {
    float coeffAdjusted = std::pow(blendSmoothCoeff_, static_cast<float>(numFrames));
    smoothedBlend_ = smoothedBlend_ * coeffAdjusted + blend_ * (1.0f - coeffAdjusted);
    blendToUse = smoothedBlend_;
    currentBlend_ = blendToUse;
  }
  else
  {
    blendToUse = smoothedBlend_;
    currentBlend_ = blendToUse;
  }

  const float normalizedBlend = (blendToUse + 1.0f) * 0.5f;
  BlendGains gains = {0.0f, 0.0f};
  if (hasIR1 && hasIR2)
  {
    gains.gain1 = std::sqrt(1.0f - normalizedBlend);
    gains.gain2 = std::sqrt(normalizedBlend);
  }
  else if (hasIR1)
  {
    if (irBEnabled_)
    {
      // Slot B is enabled but has no IR loaded — it acts as dry passthrough.
      gains.gain1 = 1.0f - normalizedBlend;
      gains.gain2 = normalizedBlend;
    }
    else
    {
      // Slot B is disabled entirely — blend is irrelevant, output 100% IR A wet.
      gains.gain1 = 1.0f;
      gains.gain2 = 0.0f;
    }
  }
  else
  {
    if (irAEnabled_)
    {
      // Slot A is enabled but has no IR loaded — it acts as dry passthrough.
      gains.gain1 = 1.0f - normalizedBlend;
      gains.gain2 = normalizedBlend;
    }
    else
    {
      // Slot A is disabled entirely — blend is irrelevant, output 100% IR B wet.
      gains.gain1 = 0.0f;
      gains.gain2 = 1.0f;
    }
  }
  return gains;
}

void IRProcessor::processMono(const Sample* input, Sample* output, FrameCount numFrames)
{
  applyPendingIRUpdates();

  bool hasIR1 = ir1Loaded_.load(std::memory_order_relaxed) && irAEnabled_;
  bool hasIR2 = ir2Loaded_.load(std::memory_order_relaxed) && irBEnabled_;

  if (!hasIR1 && !hasIR2)
  {
    std::copy(input, input + numFrames, output);
    applyOutputGain(output, numFrames);
    return;
  }

  currentInputLevelDb_ =
      (detectionMode_ == 0) ? detectPeakLevel(input, numFrames) : detectRMSLevel(input, numFrames);

  const BlendGains gains =
      resolveBlendGains(currentInputLevelDb_, numFrames, !sidechainEnabled_, hasIR1, hasIR2);
  const float gain1 = gains.gain1;
  const float gain2 = gains.gain2;

  // Feed mono input as stereo (duplicated to both channels) so that stereo IRs
  // produce output from both L and R IR channels for downmixing. For mono IRs,
  // WDL collapses identical channels internally — no extra cost.
  WDL_FFT_REAL* inputPtr = const_cast<WDL_FFT_REAL*>(input);
  std::array<WDL_FFT_REAL*, 2> stereoInput = {inputPtr, inputPtr};

  if (hasIR1 && hasIR2)
  {
    convolutionEngine1_->Add(stereoInput.data(), static_cast<int>(numFrames), 2);
    convolutionEngine2_->Add(stereoInput.data(), static_cast<int>(numFrames), 2);

    int available1 = convolutionEngine1_->Avail(static_cast<int>(numFrames));
    int available2 = convolutionEngine2_->Avail(static_cast<int>(numFrames));

    if (available1 >= static_cast<int>(numFrames) && available2 >= static_cast<int>(numFrames))
    {
      WDL_FFT_REAL** output1Ptr = convolutionEngine1_->Get();
      WDL_FFT_REAL** output2Ptr = convolutionEngine2_->Get();

      // Downmix stereo convolution output to mono in-place. When L and R point
      // to the same buffer (mono IR collapsed by WDL), the check skips the loop.
      if (output1Ptr[0] != output1Ptr[1])
        for (FrameCount i = 0; i < numFrames; ++i)
          output1Ptr[0][i] = (output1Ptr[0][i] + output1Ptr[1][i]) * 0.5f;
      if (output2Ptr[0] != output2Ptr[1])
        for (FrameCount i = 0; i < numFrames; ++i)
          output2Ptr[0][i] = (output2Ptr[0][i] + output2Ptr[1][i]) * 0.5f;

      const int latencyDiff = latencySamples1_ - latencySamples2_;

      if (latencyDiff > 0)
      {
        writeToDelayBuffer(ir1DelayBufferL_, ir1DelayWritePosL_, output1Ptr[0], numFrames);
        readFromDelayBuffer(ir1DelayBufferL_, ir1DelayWritePosL_, scratchL_.data(), numFrames,
                            latencyDiff);

        for (FrameCount i = 0; i < numFrames; ++i)
        {
          output[i] = gain1 * irATrimGainLinear_ * scratchL_[i] +
                      gain2 * irBTrimGainLinear_ * output2Ptr[0][i];
        }
      }
      else if (latencyDiff < 0)
      {
        writeToDelayBuffer(ir2DelayBufferL_, ir2DelayWritePosL_, output2Ptr[0], numFrames);
        readFromDelayBuffer(ir2DelayBufferL_, ir2DelayWritePosL_, scratchL_.data(), numFrames,
                            -latencyDiff);

        for (FrameCount i = 0; i < numFrames; ++i)
        {
          output[i] = gain1 * irATrimGainLinear_ * output1Ptr[0][i] +
                      gain2 * irBTrimGainLinear_ * scratchL_[i];
        }
      }
      else
      {
        for (FrameCount i = 0; i < numFrames; ++i)
        {
          output[i] = gain1 * irATrimGainLinear_ * output1Ptr[0][i] +
                      gain2 * irBTrimGainLinear_ * output2Ptr[0][i];
        }
      }

      convolutionEngine1_->Advance(static_cast<int>(numFrames));
      convolutionEngine2_->Advance(static_cast<int>(numFrames));
    }
    else
    {
      std::fill(output, output + numFrames, 0.0f);
    }
  }
  else if (hasIR1)
  {
    writeToDelayBuffer(dryDelayBufferL_, dryDelayWritePosL_, input, numFrames);

    convolutionEngine1_->Add(stereoInput.data(), static_cast<int>(numFrames), 2);

    int available = convolutionEngine1_->Avail(static_cast<int>(numFrames));
    if (available >= static_cast<int>(numFrames))
    {
      WDL_FFT_REAL** outputPtr = convolutionEngine1_->Get();

      if (outputPtr[0] != outputPtr[1])
        for (FrameCount i = 0; i < numFrames; ++i)
          outputPtr[0][i] = (outputPtr[0][i] + outputPtr[1][i]) * 0.5f;

      if (latencySamples1_ > 0)
        readFromDelayBuffer(dryDelayBufferL_, dryDelayWritePosL_, scratchL_.data(), numFrames,
                            latencySamples1_);
      else
        std::copy(input, input + numFrames, scratchL_.data());

      for (FrameCount i = 0; i < numFrames; ++i)
      {
        output[i] = gain1 * irATrimGainLinear_ * outputPtr[0][i] + gain2 * scratchL_[i];
      }
      convolutionEngine1_->Advance(static_cast<int>(numFrames));
    }
    else
    {
      std::fill(output, output + numFrames, 0.0f);
    }
  }
  else
  {
    writeToDelayBuffer(dryDelayBufferL_, dryDelayWritePosL_, input, numFrames);

    convolutionEngine2_->Add(stereoInput.data(), static_cast<int>(numFrames), 2);

    int available = convolutionEngine2_->Avail(static_cast<int>(numFrames));
    if (available >= static_cast<int>(numFrames))
    {
      WDL_FFT_REAL** outputPtr = convolutionEngine2_->Get();

      if (outputPtr[0] != outputPtr[1])
        for (FrameCount i = 0; i < numFrames; ++i)
          outputPtr[0][i] = (outputPtr[0][i] + outputPtr[1][i]) * 0.5f;

      if (latencySamples2_ > 0)
        readFromDelayBuffer(dryDelayBufferL_, dryDelayWritePosL_, scratchL_.data(), numFrames,
                            latencySamples2_);
      else
        std::copy(input, input + numFrames, scratchL_.data());

      for (FrameCount i = 0; i < numFrames; ++i)
      {
        output[i] = gain1 * scratchL_[i] + gain2 * irBTrimGainLinear_ * outputPtr[0][i];
      }
      convolutionEngine2_->Advance(static_cast<int>(numFrames));
    }
    else
    {
      std::fill(output, output + numFrames, 0.0f);
    }
  }

  applyOutputGain(output, numFrames);
}

void IRProcessor::applyPendingIRUpdates()
{
  bool delayBuffersNeedUpdate = false;

  if (ir1Pending_.load(std::memory_order_acquire))
  {
    std::unique_lock<std::mutex> lock(pendingMutex1_, std::try_to_lock);
    if (lock.owns_lock())
    {
      std::swap(convolutionEngine1_, stagingEngine1_);
      ir1Loaded_.store(stagingLoaded1_, std::memory_order_relaxed);
      latencySamples1_ = stagingLatency1_;
      ir1Pending_.store(false, std::memory_order_relaxed);
      delayBuffersNeedUpdate = true;
    }
  }

  if (ir2Pending_.load(std::memory_order_acquire))
  {
    std::unique_lock<std::mutex> lock(pendingMutex2_, std::try_to_lock);
    if (lock.owns_lock())
    {
      std::swap(convolutionEngine2_, stagingEngine2_);
      ir2Loaded_.store(stagingLoaded2_, std::memory_order_relaxed);
      latencySamples2_ = stagingLatency2_;
      ir2Pending_.store(false, std::memory_order_relaxed);
      delayBuffersNeedUpdate = true;
    }
  }

  if (delayBuffersNeedUpdate)
  {
    updateDelayBuffers();
  }
}

void IRProcessor::swapIRSlots()
{
  std::lock(pendingMutex1_, pendingMutex2_);
  std::lock_guard<std::mutex> lock1(pendingMutex1_, std::adopt_lock);
  std::lock_guard<std::mutex> lock2(pendingMutex2_, std::adopt_lock);

  std::swap(impulseBuffer1_, impulseBuffer2_);
  std::swap(convolutionEngine1_, convolutionEngine2_);
  std::swap(irLoader1_, irLoader2_);
  std::swap(currentIR1Path_, currentIR2Path_);
  std::swap(latencySamples1_, latencySamples2_);

  std::swap(stagingEngine1_, stagingEngine2_);
  std::swap(stagingLoaded1_, stagingLoaded2_);
  std::swap(stagingLatency1_, stagingLatency2_);

  bool loaded1 = ir1Loaded_.load(std::memory_order_relaxed);
  bool loaded2 = ir2Loaded_.load(std::memory_order_relaxed);
  ir1Loaded_.store(loaded2, std::memory_order_relaxed);
  ir2Loaded_.store(loaded1, std::memory_order_relaxed);

  bool pending1 = ir1Pending_.load(std::memory_order_relaxed);
  bool pending2 = ir2Pending_.load(std::memory_order_relaxed);
  ir1Pending_.store(pending2, std::memory_order_relaxed);
  ir2Pending_.store(pending1, std::memory_order_relaxed);

  std::swap(ir1DelayBufferL_, ir2DelayBufferL_);
  std::swap(ir1DelayBufferR_, ir2DelayBufferR_);
  std::swap(ir1DelayWritePosL_, ir2DelayWritePosL_);
  std::swap(ir1DelayWritePosR_, ir2DelayWritePosR_);
}

void IRProcessor::reset()
{
  if (convolutionEngine1_)
  {
    convolutionEngine1_->Reset();
  }
  if (convolutionEngine2_)
  {
    convolutionEngine2_->Reset();
  }
}

SampleRate IRProcessor::getIR1SampleRate() const
{
  return irLoader1_ ? irLoader1_->getIRSampleRate() : 0.0;
}

SampleRate IRProcessor::getIR2SampleRate() const
{
  return irLoader2_ ? irLoader2_->getIRSampleRate() : 0.0;
}

size_t IRProcessor::getIR1NumSamples() const
{
  return irLoader1_ ? irLoader1_->getNumSamples() : 0;
}

size_t IRProcessor::getIR2NumSamples() const
{
  return irLoader2_ ? irLoader2_->getNumSamples() : 0;
}

int IRProcessor::getNumIR1Channels() const
{
  return irLoader1_ ? irLoader1_->getNumChannels() : 0;
}

int IRProcessor::getNumIR2Channels() const
{
  return irLoader2_ ? irLoader2_->getNumChannels() : 0;
}

int IRProcessor::getLatencySamples() const
{
  return maxLatencySamples_.load();
}

void IRProcessor::processStereo(const Sample* inputL, const Sample* inputR, Sample* outputL,
                                Sample* outputR, FrameCount numFrames)
{
  applyPendingIRUpdates();

  bool hasIR1 = ir1Loaded_.load(std::memory_order_relaxed) && irAEnabled_;
  bool hasIR2 = ir2Loaded_.load(std::memory_order_relaxed) && irBEnabled_;

  if (!hasIR1 && !hasIR2)
  {
    std::copy(inputL, inputL + numFrames, outputL);
    std::copy(inputR, inputR + numFrames, outputR);
    applyOutputGain(outputL, numFrames);
    applyOutputGain(outputR, numFrames);
    return;
  }

  {
    float levelL = (detectionMode_ == 0) ? detectPeakLevel(inputL, numFrames)
                                         : detectRMSLevel(inputL, numFrames);
    float levelR = (detectionMode_ == 0) ? detectPeakLevel(inputR, numFrames)
                                         : detectRMSLevel(inputR, numFrames);
    currentInputLevelDb_ = std::max(levelL, levelR);
  }

  const BlendGains gains =
      resolveBlendGains(currentInputLevelDb_, numFrames, !sidechainEnabled_, hasIR1, hasIR2);
  const float gain1 = gains.gain1;
  const float gain2 = gains.gain2;

  std::array<WDL_FFT_REAL*, 2> inputPtrs = {const_cast<WDL_FFT_REAL*>(inputL),
                                            const_cast<WDL_FFT_REAL*>(inputR)};

  if (hasIR1 && hasIR2)
  {
    convolutionEngine1_->Add(inputPtrs.data(), static_cast<int>(numFrames), 2);
    convolutionEngine2_->Add(inputPtrs.data(), static_cast<int>(numFrames), 2);

    int available1 = convolutionEngine1_->Avail(static_cast<int>(numFrames));
    int available2 = convolutionEngine2_->Avail(static_cast<int>(numFrames));

    if (available1 >= static_cast<int>(numFrames) && available2 >= static_cast<int>(numFrames))
    {
      WDL_FFT_REAL** output1Ptr = convolutionEngine1_->Get();
      WDL_FFT_REAL** output2Ptr = convolutionEngine2_->Get();

      const int latencyDiff = latencySamples1_ - latencySamples2_;

      if (latencyDiff > 0)
      {
        writeToDelayBuffer(ir1DelayBufferL_, ir1DelayWritePosL_, output1Ptr[0], numFrames);
        writeToDelayBuffer(ir1DelayBufferR_, ir1DelayWritePosR_, output1Ptr[1], numFrames);
        readFromDelayBuffer(ir1DelayBufferL_, ir1DelayWritePosL_, scratchL_.data(), numFrames,
                            latencyDiff);
        readFromDelayBuffer(ir1DelayBufferR_, ir1DelayWritePosR_, scratchR_.data(), numFrames,
                            latencyDiff);

        for (FrameCount i = 0; i < numFrames; ++i)
        {
          outputL[i] = gain1 * irATrimGainLinear_ * scratchL_[i] +
                       gain2 * irBTrimGainLinear_ * output2Ptr[0][i];
          outputR[i] = gain1 * irATrimGainLinear_ * scratchR_[i] +
                       gain2 * irBTrimGainLinear_ * output2Ptr[1][i];
        }
      }
      else if (latencyDiff < 0)
      {
        writeToDelayBuffer(ir2DelayBufferL_, ir2DelayWritePosL_, output2Ptr[0], numFrames);
        writeToDelayBuffer(ir2DelayBufferR_, ir2DelayWritePosR_, output2Ptr[1], numFrames);
        readFromDelayBuffer(ir2DelayBufferL_, ir2DelayWritePosL_, scratchL_.data(), numFrames,
                            -latencyDiff);
        readFromDelayBuffer(ir2DelayBufferR_, ir2DelayWritePosR_, scratchR_.data(), numFrames,
                            -latencyDiff);

        for (FrameCount i = 0; i < numFrames; ++i)
        {
          outputL[i] = gain1 * irATrimGainLinear_ * output1Ptr[0][i] +
                       gain2 * irBTrimGainLinear_ * scratchL_[i];
          outputR[i] = gain1 * irATrimGainLinear_ * output1Ptr[1][i] +
                       gain2 * irBTrimGainLinear_ * scratchR_[i];
        }
      }
      else
      {
        for (FrameCount i = 0; i < numFrames; ++i)
        {
          outputL[i] = gain1 * irATrimGainLinear_ * output1Ptr[0][i] +
                       gain2 * irBTrimGainLinear_ * output2Ptr[0][i];
          outputR[i] = gain1 * irATrimGainLinear_ * output1Ptr[1][i] +
                       gain2 * irBTrimGainLinear_ * output2Ptr[1][i];
        }
      }

      convolutionEngine1_->Advance(static_cast<int>(numFrames));
      convolutionEngine2_->Advance(static_cast<int>(numFrames));
    }
    else
    {
      std::fill(outputL, outputL + numFrames, 0.0f);
      std::fill(outputR, outputR + numFrames, 0.0f);
    }
  }
  else if (hasIR1)
  {
    writeToDelayBuffer(dryDelayBufferL_, dryDelayWritePosL_, inputL, numFrames);
    writeToDelayBuffer(dryDelayBufferR_, dryDelayWritePosR_, inputR, numFrames);

    convolutionEngine1_->Add(inputPtrs.data(), static_cast<int>(numFrames), 2);

    int available = convolutionEngine1_->Avail(static_cast<int>(numFrames));
    if (available >= static_cast<int>(numFrames))
    {
      WDL_FFT_REAL** outputPtr = convolutionEngine1_->Get();

      if (latencySamples1_ > 0)
      {
        readFromDelayBuffer(dryDelayBufferL_, dryDelayWritePosL_, scratchL_.data(), numFrames,
                            latencySamples1_);
        readFromDelayBuffer(dryDelayBufferR_, dryDelayWritePosR_, scratchR_.data(), numFrames,
                            latencySamples1_);
      }
      else
      {
        std::copy(inputL, inputL + numFrames, scratchL_.data());
        std::copy(inputR, inputR + numFrames, scratchR_.data());
      }

      for (FrameCount i = 0; i < numFrames; ++i)
      {
        outputL[i] = gain1 * irATrimGainLinear_ * outputPtr[0][i] + gain2 * scratchL_[i];
        outputR[i] = gain1 * irATrimGainLinear_ * outputPtr[1][i] + gain2 * scratchR_[i];
      }
      convolutionEngine1_->Advance(static_cast<int>(numFrames));
    }
    else
    {
      std::fill(outputL, outputL + numFrames, 0.0f);
      std::fill(outputR, outputR + numFrames, 0.0f);
    }
  }
  else
  {
    writeToDelayBuffer(dryDelayBufferL_, dryDelayWritePosL_, inputL, numFrames);
    writeToDelayBuffer(dryDelayBufferR_, dryDelayWritePosR_, inputR, numFrames);

    convolutionEngine2_->Add(inputPtrs.data(), static_cast<int>(numFrames), 2);

    int available = convolutionEngine2_->Avail(static_cast<int>(numFrames));
    if (available >= static_cast<int>(numFrames))
    {
      WDL_FFT_REAL** outputPtr = convolutionEngine2_->Get();

      if (latencySamples2_ > 0)
      {
        readFromDelayBuffer(dryDelayBufferL_, dryDelayWritePosL_, scratchL_.data(), numFrames,
                            latencySamples2_);
        readFromDelayBuffer(dryDelayBufferR_, dryDelayWritePosR_, scratchR_.data(), numFrames,
                            latencySamples2_);
      }
      else
      {
        std::copy(inputL, inputL + numFrames, scratchL_.data());
        std::copy(inputR, inputR + numFrames, scratchR_.data());
      }

      for (FrameCount i = 0; i < numFrames; ++i)
      {
        outputL[i] = gain1 * scratchL_[i] + gain2 * irBTrimGainLinear_ * outputPtr[0][i];
        outputR[i] = gain1 * scratchR_[i] + gain2 * irBTrimGainLinear_ * outputPtr[1][i];
      }
      convolutionEngine2_->Advance(static_cast<int>(numFrames));
    }
    else
    {
      std::fill(outputL, outputL + numFrames, 0.0f);
      std::fill(outputR, outputR + numFrames, 0.0f);
    }
  }

  applyOutputGain(outputL, numFrames);
  applyOutputGain(outputR, numFrames);
}

void IRProcessor::processDualMono(const Sample* inputL, const Sample* inputR, Sample* outputL,
                                  Sample* outputR, FrameCount numFrames)
{
  processStereo(inputL, inputR, outputL, outputR, numFrames);
}

void IRProcessor::processMonoToStereo(const Sample* input, Sample* outputL, Sample* outputR,
                                      FrameCount numFrames)
{
  applyPendingIRUpdates();

  bool hasIR1 = ir1Loaded_.load(std::memory_order_relaxed) && irAEnabled_;
  bool hasIR2 = ir2Loaded_.load(std::memory_order_relaxed) && irBEnabled_;

  if (!hasIR1 && !hasIR2)
  {
    std::copy(input, input + numFrames, outputL);
    std::copy(input, input + numFrames, outputR);
    applyOutputGain(outputL, numFrames);
    applyOutputGain(outputR, numFrames);
    return;
  }

  currentInputLevelDb_ =
      (detectionMode_ == 0) ? detectPeakLevel(input, numFrames) : detectRMSLevel(input, numFrames);

  const BlendGains gains =
      resolveBlendGains(currentInputLevelDb_, numFrames, !sidechainEnabled_, hasIR1, hasIR2);
  const float gain1 = gains.gain1;
  const float gain2 = gains.gain2;

  WDL_FFT_REAL* inputPtr = const_cast<WDL_FFT_REAL*>(input);
  std::array<WDL_FFT_REAL*, 2> stereoInput = {inputPtr, inputPtr};

  if (hasIR1 && hasIR2)
  {
    convolutionEngine1_->Add(stereoInput.data(), static_cast<int>(numFrames), 2);
    convolutionEngine2_->Add(stereoInput.data(), static_cast<int>(numFrames), 2);

    int available1 = convolutionEngine1_->Avail(static_cast<int>(numFrames));
    int available2 = convolutionEngine2_->Avail(static_cast<int>(numFrames));

    if (available1 >= static_cast<int>(numFrames) && available2 >= static_cast<int>(numFrames))
    {
      WDL_FFT_REAL** output1Ptr = convolutionEngine1_->Get();
      WDL_FFT_REAL** output2Ptr = convolutionEngine2_->Get();

      const int latencyDiff = latencySamples1_ - latencySamples2_;

      if (latencyDiff > 0)
      {
        writeToDelayBuffer(ir1DelayBufferL_, ir1DelayWritePosL_, output1Ptr[0], numFrames);
        writeToDelayBuffer(ir1DelayBufferR_, ir1DelayWritePosR_, output1Ptr[1], numFrames);
        readFromDelayBuffer(ir1DelayBufferL_, ir1DelayWritePosL_, scratchL_.data(), numFrames,
                            latencyDiff);
        readFromDelayBuffer(ir1DelayBufferR_, ir1DelayWritePosR_, scratchR_.data(), numFrames,
                            latencyDiff);

        for (FrameCount i = 0; i < numFrames; ++i)
        {
          outputL[i] = gain1 * irATrimGainLinear_ * scratchL_[i] +
                       gain2 * irBTrimGainLinear_ * output2Ptr[0][i];
          outputR[i] = gain1 * irATrimGainLinear_ * scratchR_[i] +
                       gain2 * irBTrimGainLinear_ * output2Ptr[1][i];
        }
      }
      else if (latencyDiff < 0)
      {
        writeToDelayBuffer(ir2DelayBufferL_, ir2DelayWritePosL_, output2Ptr[0], numFrames);
        writeToDelayBuffer(ir2DelayBufferR_, ir2DelayWritePosR_, output2Ptr[1], numFrames);
        readFromDelayBuffer(ir2DelayBufferL_, ir2DelayWritePosL_, scratchL_.data(), numFrames,
                            -latencyDiff);
        readFromDelayBuffer(ir2DelayBufferR_, ir2DelayWritePosR_, scratchR_.data(), numFrames,
                            -latencyDiff);

        for (FrameCount i = 0; i < numFrames; ++i)
        {
          outputL[i] = gain1 * irATrimGainLinear_ * output1Ptr[0][i] +
                       gain2 * irBTrimGainLinear_ * scratchL_[i];
          outputR[i] = gain1 * irATrimGainLinear_ * output1Ptr[1][i] +
                       gain2 * irBTrimGainLinear_ * scratchR_[i];
        }
      }
      else
      {
        for (FrameCount i = 0; i < numFrames; ++i)
        {
          outputL[i] = gain1 * irATrimGainLinear_ * output1Ptr[0][i] +
                       gain2 * irBTrimGainLinear_ * output2Ptr[0][i];
          outputR[i] = gain1 * irATrimGainLinear_ * output1Ptr[1][i] +
                       gain2 * irBTrimGainLinear_ * output2Ptr[1][i];
        }
      }

      convolutionEngine1_->Advance(static_cast<int>(numFrames));
      convolutionEngine2_->Advance(static_cast<int>(numFrames));
    }
    else
    {
      std::fill(outputL, outputL + numFrames, 0.0f);
      std::fill(outputR, outputR + numFrames, 0.0f);
    }
  }
  else if (hasIR1)
  {
    writeToDelayBuffer(dryDelayBufferL_, dryDelayWritePosL_, input, numFrames);
    writeToDelayBuffer(dryDelayBufferR_, dryDelayWritePosR_, input, numFrames);

    convolutionEngine1_->Add(stereoInput.data(), static_cast<int>(numFrames), 2);

    int available = convolutionEngine1_->Avail(static_cast<int>(numFrames));
    if (available >= static_cast<int>(numFrames))
    {
      WDL_FFT_REAL** outputPtr = convolutionEngine1_->Get();

      if (latencySamples1_ > 0)
      {
        readFromDelayBuffer(dryDelayBufferL_, dryDelayWritePosL_, scratchL_.data(), numFrames,
                            latencySamples1_);
        readFromDelayBuffer(dryDelayBufferR_, dryDelayWritePosR_, scratchR_.data(), numFrames,
                            latencySamples1_);
      }
      else
      {
        std::copy(input, input + numFrames, scratchL_.data());
        std::copy(input, input + numFrames, scratchR_.data());
      }

      for (FrameCount i = 0; i < numFrames; ++i)
      {
        outputL[i] = gain1 * irATrimGainLinear_ * outputPtr[0][i] + gain2 * scratchL_[i];
        outputR[i] = gain1 * irATrimGainLinear_ * outputPtr[1][i] + gain2 * scratchR_[i];
      }
      convolutionEngine1_->Advance(static_cast<int>(numFrames));
    }
    else
    {
      std::fill(outputL, outputL + numFrames, 0.0f);
      std::fill(outputR, outputR + numFrames, 0.0f);
    }
  }
  else
  {
    writeToDelayBuffer(dryDelayBufferL_, dryDelayWritePosL_, input, numFrames);
    writeToDelayBuffer(dryDelayBufferR_, dryDelayWritePosR_, input, numFrames);

    convolutionEngine2_->Add(stereoInput.data(), static_cast<int>(numFrames), 2);

    int available = convolutionEngine2_->Avail(static_cast<int>(numFrames));
    if (available >= static_cast<int>(numFrames))
    {
      WDL_FFT_REAL** outputPtr = convolutionEngine2_->Get();

      if (latencySamples2_ > 0)
      {
        readFromDelayBuffer(dryDelayBufferL_, dryDelayWritePosL_, scratchL_.data(), numFrames,
                            latencySamples2_);
        readFromDelayBuffer(dryDelayBufferR_, dryDelayWritePosR_, scratchR_.data(), numFrames,
                            latencySamples2_);
      }
      else
      {
        std::copy(input, input + numFrames, scratchL_.data());
        std::copy(input, input + numFrames, scratchR_.data());
      }

      for (FrameCount i = 0; i < numFrames; ++i)
      {
        outputL[i] = gain1 * scratchL_[i] + gain2 * irBTrimGainLinear_ * outputPtr[0][i];
        outputR[i] = gain1 * scratchR_[i] + gain2 * irBTrimGainLinear_ * outputPtr[1][i];
      }
      convolutionEngine2_->Advance(static_cast<int>(numFrames));
    }
    else
    {
      std::fill(outputL, outputL + numFrames, 0.0f);
      std::fill(outputR, outputR + numFrames, 0.0f);
    }
  }

  applyOutputGain(outputL, numFrames);
  applyOutputGain(outputR, numFrames);
}

void IRProcessor::processMonoWithSidechain(const Sample* input, const Sample* sidechain,
                                           Sample* output, FrameCount numFrames)
{
  applyPendingIRUpdates();

  bool hasIR1 = ir1Loaded_.load(std::memory_order_relaxed) && irAEnabled_;
  bool hasIR2 = ir2Loaded_.load(std::memory_order_relaxed) && irBEnabled_;

  if (!hasIR1 && !hasIR2)
  {
    std::copy(input, input + numFrames, output);
    applyOutputGain(output, numFrames);
    return;
  }

  currentInputLevelDb_ = (detectionMode_ == 0) ? detectPeakLevel(sidechain, numFrames)
                                               : detectRMSLevel(sidechain, numFrames);

  const BlendGains gains =
      resolveBlendGains(currentInputLevelDb_, numFrames, sidechainEnabled_, hasIR1, hasIR2);
  const float gain1 = gains.gain1;
  const float gain2 = gains.gain2;

  WDL_FFT_REAL* inputPtr = const_cast<WDL_FFT_REAL*>(input);
  std::array<WDL_FFT_REAL*, 2> stereoInput = {inputPtr, inputPtr};

  if (hasIR1 && hasIR2)
  {
    convolutionEngine1_->Add(stereoInput.data(), static_cast<int>(numFrames), 2);
    convolutionEngine2_->Add(stereoInput.data(), static_cast<int>(numFrames), 2);

    int available1 = convolutionEngine1_->Avail(static_cast<int>(numFrames));
    int available2 = convolutionEngine2_->Avail(static_cast<int>(numFrames));

    if (available1 >= static_cast<int>(numFrames) && available2 >= static_cast<int>(numFrames))
    {
      WDL_FFT_REAL** output1Ptr = convolutionEngine1_->Get();
      WDL_FFT_REAL** output2Ptr = convolutionEngine2_->Get();

      if (output1Ptr[0] != output1Ptr[1])
        for (FrameCount i = 0; i < numFrames; ++i)
          output1Ptr[0][i] = (output1Ptr[0][i] + output1Ptr[1][i]) * 0.5f;
      if (output2Ptr[0] != output2Ptr[1])
        for (FrameCount i = 0; i < numFrames; ++i)
          output2Ptr[0][i] = (output2Ptr[0][i] + output2Ptr[1][i]) * 0.5f;

      const int latencyDiff = latencySamples1_ - latencySamples2_;

      if (latencyDiff > 0)
      {
        writeToDelayBuffer(ir1DelayBufferL_, ir1DelayWritePosL_, output1Ptr[0], numFrames);
        readFromDelayBuffer(ir1DelayBufferL_, ir1DelayWritePosL_, scratchL_.data(), numFrames,
                            latencyDiff);

        for (FrameCount i = 0; i < numFrames; ++i)
        {
          output[i] = gain1 * irATrimGainLinear_ * scratchL_[i] +
                      gain2 * irBTrimGainLinear_ * output2Ptr[0][i];
        }
      }
      else if (latencyDiff < 0)
      {
        writeToDelayBuffer(ir2DelayBufferL_, ir2DelayWritePosL_, output2Ptr[0], numFrames);
        readFromDelayBuffer(ir2DelayBufferL_, ir2DelayWritePosL_, scratchL_.data(), numFrames,
                            -latencyDiff);

        for (FrameCount i = 0; i < numFrames; ++i)
        {
          output[i] = gain1 * irATrimGainLinear_ * output1Ptr[0][i] +
                      gain2 * irBTrimGainLinear_ * scratchL_[i];
        }
      }
      else
      {
        for (FrameCount i = 0; i < numFrames; ++i)
        {
          output[i] = gain1 * irATrimGainLinear_ * output1Ptr[0][i] +
                      gain2 * irBTrimGainLinear_ * output2Ptr[0][i];
        }
      }

      convolutionEngine1_->Advance(static_cast<int>(numFrames));
      convolutionEngine2_->Advance(static_cast<int>(numFrames));
    }
    else
    {
      std::fill(output, output + numFrames, 0.0f);
    }
  }
  else if (hasIR1)
  {
    writeToDelayBuffer(dryDelayBufferL_, dryDelayWritePosL_, input, numFrames);

    convolutionEngine1_->Add(stereoInput.data(), static_cast<int>(numFrames), 2);

    int available = convolutionEngine1_->Avail(static_cast<int>(numFrames));
    if (available >= static_cast<int>(numFrames))
    {
      WDL_FFT_REAL** outputPtr = convolutionEngine1_->Get();

      if (outputPtr[0] != outputPtr[1])
        for (FrameCount i = 0; i < numFrames; ++i)
          outputPtr[0][i] = (outputPtr[0][i] + outputPtr[1][i]) * 0.5f;

      if (latencySamples1_ > 0)
        readFromDelayBuffer(dryDelayBufferL_, dryDelayWritePosL_, scratchL_.data(), numFrames,
                            latencySamples1_);
      else
        std::copy(input, input + numFrames, scratchL_.data());

      for (FrameCount i = 0; i < numFrames; ++i)
      {
        output[i] = gain1 * irATrimGainLinear_ * outputPtr[0][i] + gain2 * scratchL_[i];
      }
      convolutionEngine1_->Advance(static_cast<int>(numFrames));
    }
    else
    {
      std::fill(output, output + numFrames, 0.0f);
    }
  }
  else
  {
    writeToDelayBuffer(dryDelayBufferL_, dryDelayWritePosL_, input, numFrames);

    convolutionEngine2_->Add(stereoInput.data(), static_cast<int>(numFrames), 2);

    int available = convolutionEngine2_->Avail(static_cast<int>(numFrames));
    if (available >= static_cast<int>(numFrames))
    {
      WDL_FFT_REAL** outputPtr = convolutionEngine2_->Get();

      if (outputPtr[0] != outputPtr[1])
        for (FrameCount i = 0; i < numFrames; ++i)
          outputPtr[0][i] = (outputPtr[0][i] + outputPtr[1][i]) * 0.5f;

      if (latencySamples2_ > 0)
        readFromDelayBuffer(dryDelayBufferL_, dryDelayWritePosL_, scratchL_.data(), numFrames,
                            latencySamples2_);
      else
        std::copy(input, input + numFrames, scratchL_.data());

      for (FrameCount i = 0; i < numFrames; ++i)
      {
        output[i] = gain1 * scratchL_[i] + gain2 * irBTrimGainLinear_ * outputPtr[0][i];
      }
      convolutionEngine2_->Advance(static_cast<int>(numFrames));
    }
    else
    {
      std::fill(output, output + numFrames, 0.0f);
    }
  }

  applyOutputGain(output, numFrames);
}

void IRProcessor::processMonoToStereoWithSidechain(const Sample* input, const Sample* sidechain,
                                                   Sample* outputL, Sample* outputR,
                                                   FrameCount numFrames)
{
  applyPendingIRUpdates();

  bool hasIR1 = ir1Loaded_.load(std::memory_order_relaxed) && irAEnabled_;
  bool hasIR2 = ir2Loaded_.load(std::memory_order_relaxed) && irBEnabled_;

  if (!hasIR1 && !hasIR2)
  {
    std::copy(input, input + numFrames, outputL);
    std::copy(input, input + numFrames, outputR);
    applyOutputGain(outputL, numFrames);
    applyOutputGain(outputR, numFrames);
    return;
  }

  currentInputLevelDb_ = (detectionMode_ == 0) ? detectPeakLevel(sidechain, numFrames)
                                               : detectRMSLevel(sidechain, numFrames);

  const BlendGains gains =
      resolveBlendGains(currentInputLevelDb_, numFrames, sidechainEnabled_, hasIR1, hasIR2);
  const float gain1 = gains.gain1;
  const float gain2 = gains.gain2;

  WDL_FFT_REAL* inputPtr = const_cast<WDL_FFT_REAL*>(input);
  std::array<WDL_FFT_REAL*, 2> stereoInput = {inputPtr, inputPtr};

  if (hasIR1 && hasIR2)
  {
    convolutionEngine1_->Add(stereoInput.data(), static_cast<int>(numFrames), 2);
    convolutionEngine2_->Add(stereoInput.data(), static_cast<int>(numFrames), 2);

    int available1 = convolutionEngine1_->Avail(static_cast<int>(numFrames));
    int available2 = convolutionEngine2_->Avail(static_cast<int>(numFrames));

    if (available1 >= static_cast<int>(numFrames) && available2 >= static_cast<int>(numFrames))
    {
      WDL_FFT_REAL** output1Ptr = convolutionEngine1_->Get();
      WDL_FFT_REAL** output2Ptr = convolutionEngine2_->Get();

      const int latencyDiff = latencySamples1_ - latencySamples2_;

      if (latencyDiff > 0)
      {
        writeToDelayBuffer(ir1DelayBufferL_, ir1DelayWritePosL_, output1Ptr[0], numFrames);
        writeToDelayBuffer(ir1DelayBufferR_, ir1DelayWritePosR_, output1Ptr[1], numFrames);
        readFromDelayBuffer(ir1DelayBufferL_, ir1DelayWritePosL_, scratchL_.data(), numFrames,
                            latencyDiff);
        readFromDelayBuffer(ir1DelayBufferR_, ir1DelayWritePosR_, scratchR_.data(), numFrames,
                            latencyDiff);

        for (FrameCount i = 0; i < numFrames; ++i)
        {
          outputL[i] = gain1 * irATrimGainLinear_ * scratchL_[i] +
                       gain2 * irBTrimGainLinear_ * output2Ptr[0][i];
          outputR[i] = gain1 * irATrimGainLinear_ * scratchR_[i] +
                       gain2 * irBTrimGainLinear_ * output2Ptr[1][i];
        }
      }
      else if (latencyDiff < 0)
      {
        writeToDelayBuffer(ir2DelayBufferL_, ir2DelayWritePosL_, output2Ptr[0], numFrames);
        writeToDelayBuffer(ir2DelayBufferR_, ir2DelayWritePosR_, output2Ptr[1], numFrames);
        readFromDelayBuffer(ir2DelayBufferL_, ir2DelayWritePosL_, scratchL_.data(), numFrames,
                            -latencyDiff);
        readFromDelayBuffer(ir2DelayBufferR_, ir2DelayWritePosR_, scratchR_.data(), numFrames,
                            -latencyDiff);

        for (FrameCount i = 0; i < numFrames; ++i)
        {
          outputL[i] = gain1 * irATrimGainLinear_ * output1Ptr[0][i] +
                       gain2 * irBTrimGainLinear_ * scratchL_[i];
          outputR[i] = gain1 * irATrimGainLinear_ * output1Ptr[1][i] +
                       gain2 * irBTrimGainLinear_ * scratchR_[i];
        }
      }
      else
      {
        for (FrameCount i = 0; i < numFrames; ++i)
        {
          outputL[i] = gain1 * irATrimGainLinear_ * output1Ptr[0][i] +
                       gain2 * irBTrimGainLinear_ * output2Ptr[0][i];
          outputR[i] = gain1 * irATrimGainLinear_ * output1Ptr[1][i] +
                       gain2 * irBTrimGainLinear_ * output2Ptr[1][i];
        }
      }

      convolutionEngine1_->Advance(static_cast<int>(numFrames));
      convolutionEngine2_->Advance(static_cast<int>(numFrames));
    }
    else
    {
      std::fill(outputL, outputL + numFrames, 0.0f);
      std::fill(outputR, outputR + numFrames, 0.0f);
    }
  }
  else if (hasIR1)
  {
    writeToDelayBuffer(dryDelayBufferL_, dryDelayWritePosL_, input, numFrames);
    writeToDelayBuffer(dryDelayBufferR_, dryDelayWritePosR_, input, numFrames);

    convolutionEngine1_->Add(stereoInput.data(), static_cast<int>(numFrames), 2);

    int available = convolutionEngine1_->Avail(static_cast<int>(numFrames));
    if (available >= static_cast<int>(numFrames))
    {
      WDL_FFT_REAL** outputPtr = convolutionEngine1_->Get();

      if (latencySamples1_ > 0)
      {
        readFromDelayBuffer(dryDelayBufferL_, dryDelayWritePosL_, scratchL_.data(), numFrames,
                            latencySamples1_);
        readFromDelayBuffer(dryDelayBufferR_, dryDelayWritePosR_, scratchR_.data(), numFrames,
                            latencySamples1_);
      }
      else
      {
        std::copy(input, input + numFrames, scratchL_.data());
        std::copy(input, input + numFrames, scratchR_.data());
      }

      for (FrameCount i = 0; i < numFrames; ++i)
      {
        outputL[i] = gain1 * irATrimGainLinear_ * outputPtr[0][i] + gain2 * scratchL_[i];
        outputR[i] = gain1 * irATrimGainLinear_ * outputPtr[1][i] + gain2 * scratchR_[i];
      }
      convolutionEngine1_->Advance(static_cast<int>(numFrames));
    }
    else
    {
      std::fill(outputL, outputL + numFrames, 0.0f);
      std::fill(outputR, outputR + numFrames, 0.0f);
    }
  }
  else
  {
    writeToDelayBuffer(dryDelayBufferL_, dryDelayWritePosL_, input, numFrames);
    writeToDelayBuffer(dryDelayBufferR_, dryDelayWritePosR_, input, numFrames);

    convolutionEngine2_->Add(stereoInput.data(), static_cast<int>(numFrames), 2);

    int available = convolutionEngine2_->Avail(static_cast<int>(numFrames));
    if (available >= static_cast<int>(numFrames))
    {
      WDL_FFT_REAL** outputPtr = convolutionEngine2_->Get();

      if (latencySamples2_ > 0)
      {
        readFromDelayBuffer(dryDelayBufferL_, dryDelayWritePosL_, scratchL_.data(), numFrames,
                            latencySamples2_);
        readFromDelayBuffer(dryDelayBufferR_, dryDelayWritePosR_, scratchR_.data(), numFrames,
                            latencySamples2_);
      }
      else
      {
        std::copy(input, input + numFrames, scratchL_.data());
        std::copy(input, input + numFrames, scratchR_.data());
      }

      for (FrameCount i = 0; i < numFrames; ++i)
      {
        outputL[i] = gain1 * scratchL_[i] + gain2 * irBTrimGainLinear_ * outputPtr[0][i];
        outputR[i] = gain1 * scratchR_[i] + gain2 * irBTrimGainLinear_ * outputPtr[1][i];
      }
      convolutionEngine2_->Advance(static_cast<int>(numFrames));
    }
    else
    {
      std::fill(outputL, outputL + numFrames, 0.0f);
      std::fill(outputR, outputR + numFrames, 0.0f);
    }
  }

  applyOutputGain(outputL, numFrames);
  applyOutputGain(outputR, numFrames);
}

void IRProcessor::processStereoWithSidechain(const Sample* inputL, const Sample* inputR,
                                             const Sample* sidechainL, const Sample* sidechainR,
                                             Sample* outputL, Sample* outputR, FrameCount numFrames)
{
  applyPendingIRUpdates();

  bool hasIR1 = ir1Loaded_.load(std::memory_order_relaxed) && irAEnabled_;
  bool hasIR2 = ir2Loaded_.load(std::memory_order_relaxed) && irBEnabled_;

  if (!hasIR1 && !hasIR2)
  {
    std::copy(inputL, inputL + numFrames, outputL);
    std::copy(inputR, inputR + numFrames, outputR);
    applyOutputGain(outputL, numFrames);
    applyOutputGain(outputR, numFrames);
    return;
  }

  {
    float levelL = (detectionMode_ == 0) ? detectPeakLevel(sidechainL, numFrames)
                                         : detectRMSLevel(sidechainL, numFrames);
    float levelR = (detectionMode_ == 0) ? detectPeakLevel(sidechainR, numFrames)
                                         : detectRMSLevel(sidechainR, numFrames);
    currentInputLevelDb_ = std::max(levelL, levelR);
  }

  const BlendGains gains =
      resolveBlendGains(currentInputLevelDb_, numFrames, sidechainEnabled_, hasIR1, hasIR2);
  const float gain1 = gains.gain1;
  const float gain2 = gains.gain2;

  std::array<WDL_FFT_REAL*, 2> inputPtrs = {const_cast<WDL_FFT_REAL*>(inputL),
                                            const_cast<WDL_FFT_REAL*>(inputR)};

  if (hasIR1 && hasIR2)
  {
    convolutionEngine1_->Add(inputPtrs.data(), static_cast<int>(numFrames), 2);
    convolutionEngine2_->Add(inputPtrs.data(), static_cast<int>(numFrames), 2);

    int available1 = convolutionEngine1_->Avail(static_cast<int>(numFrames));
    int available2 = convolutionEngine2_->Avail(static_cast<int>(numFrames));

    if (available1 >= static_cast<int>(numFrames) && available2 >= static_cast<int>(numFrames))
    {
      WDL_FFT_REAL** output1Ptr = convolutionEngine1_->Get();
      WDL_FFT_REAL** output2Ptr = convolutionEngine2_->Get();

      const int latencyDiff = latencySamples1_ - latencySamples2_;

      if (latencyDiff > 0)
      {
        writeToDelayBuffer(ir1DelayBufferL_, ir1DelayWritePosL_, output1Ptr[0], numFrames);
        writeToDelayBuffer(ir1DelayBufferR_, ir1DelayWritePosR_, output1Ptr[1], numFrames);
        readFromDelayBuffer(ir1DelayBufferL_, ir1DelayWritePosL_, scratchL_.data(), numFrames,
                            latencyDiff);
        readFromDelayBuffer(ir1DelayBufferR_, ir1DelayWritePosR_, scratchR_.data(), numFrames,
                            latencyDiff);

        for (FrameCount i = 0; i < numFrames; ++i)
        {
          outputL[i] = gain1 * irATrimGainLinear_ * scratchL_[i] +
                       gain2 * irBTrimGainLinear_ * output2Ptr[0][i];
          outputR[i] = gain1 * irATrimGainLinear_ * scratchR_[i] +
                       gain2 * irBTrimGainLinear_ * output2Ptr[1][i];
        }
      }
      else if (latencyDiff < 0)
      {
        writeToDelayBuffer(ir2DelayBufferL_, ir2DelayWritePosL_, output2Ptr[0], numFrames);
        writeToDelayBuffer(ir2DelayBufferR_, ir2DelayWritePosR_, output2Ptr[1], numFrames);
        readFromDelayBuffer(ir2DelayBufferL_, ir2DelayWritePosL_, scratchL_.data(), numFrames,
                            -latencyDiff);
        readFromDelayBuffer(ir2DelayBufferR_, ir2DelayWritePosR_, scratchR_.data(), numFrames,
                            -latencyDiff);

        for (FrameCount i = 0; i < numFrames; ++i)
        {
          outputL[i] = gain1 * irATrimGainLinear_ * output1Ptr[0][i] +
                       gain2 * irBTrimGainLinear_ * scratchL_[i];
          outputR[i] = gain1 * irATrimGainLinear_ * output1Ptr[1][i] +
                       gain2 * irBTrimGainLinear_ * scratchR_[i];
        }
      }
      else
      {
        for (FrameCount i = 0; i < numFrames; ++i)
        {
          outputL[i] = gain1 * irATrimGainLinear_ * output1Ptr[0][i] +
                       gain2 * irBTrimGainLinear_ * output2Ptr[0][i];
          outputR[i] = gain1 * irATrimGainLinear_ * output1Ptr[1][i] +
                       gain2 * irBTrimGainLinear_ * output2Ptr[1][i];
        }
      }

      convolutionEngine1_->Advance(static_cast<int>(numFrames));
      convolutionEngine2_->Advance(static_cast<int>(numFrames));
    }
    else
    {
      std::fill(outputL, outputL + numFrames, 0.0f);
      std::fill(outputR, outputR + numFrames, 0.0f);
    }
  }
  else if (hasIR1)
  {
    writeToDelayBuffer(dryDelayBufferL_, dryDelayWritePosL_, inputL, numFrames);
    writeToDelayBuffer(dryDelayBufferR_, dryDelayWritePosR_, inputR, numFrames);

    convolutionEngine1_->Add(inputPtrs.data(), static_cast<int>(numFrames), 2);

    int available = convolutionEngine1_->Avail(static_cast<int>(numFrames));
    if (available >= static_cast<int>(numFrames))
    {
      WDL_FFT_REAL** outputPtr = convolutionEngine1_->Get();

      if (latencySamples1_ > 0)
      {
        readFromDelayBuffer(dryDelayBufferL_, dryDelayWritePosL_, scratchL_.data(), numFrames,
                            latencySamples1_);
        readFromDelayBuffer(dryDelayBufferR_, dryDelayWritePosR_, scratchR_.data(), numFrames,
                            latencySamples1_);
      }
      else
      {
        std::copy(inputL, inputL + numFrames, scratchL_.data());
        std::copy(inputR, inputR + numFrames, scratchR_.data());
      }

      for (FrameCount i = 0; i < numFrames; ++i)
      {
        outputL[i] = gain1 * irATrimGainLinear_ * outputPtr[0][i] + gain2 * scratchL_[i];
        outputR[i] = gain1 * irATrimGainLinear_ * outputPtr[1][i] + gain2 * scratchR_[i];
      }
      convolutionEngine1_->Advance(static_cast<int>(numFrames));
    }
    else
    {
      std::fill(outputL, outputL + numFrames, 0.0f);
      std::fill(outputR, outputR + numFrames, 0.0f);
    }
  }
  else
  {
    writeToDelayBuffer(dryDelayBufferL_, dryDelayWritePosL_, inputL, numFrames);
    writeToDelayBuffer(dryDelayBufferR_, dryDelayWritePosR_, inputR, numFrames);

    convolutionEngine2_->Add(inputPtrs.data(), static_cast<int>(numFrames), 2);

    int available = convolutionEngine2_->Avail(static_cast<int>(numFrames));
    if (available >= static_cast<int>(numFrames))
    {
      WDL_FFT_REAL** outputPtr = convolutionEngine2_->Get();

      if (latencySamples2_ > 0)
      {
        readFromDelayBuffer(dryDelayBufferL_, dryDelayWritePosL_, scratchL_.data(), numFrames,
                            latencySamples2_);
        readFromDelayBuffer(dryDelayBufferR_, dryDelayWritePosR_, scratchR_.data(), numFrames,
                            latencySamples2_);
      }
      else
      {
        std::copy(inputL, inputL + numFrames, scratchL_.data());
        std::copy(inputR, inputR + numFrames, scratchR_.data());
      }

      for (FrameCount i = 0; i < numFrames; ++i)
      {
        outputL[i] = gain1 * scratchL_[i] + gain2 * irBTrimGainLinear_ * outputPtr[0][i];
        outputR[i] = gain1 * scratchR_[i] + gain2 * irBTrimGainLinear_ * outputPtr[1][i];
      }
      convolutionEngine2_->Advance(static_cast<int>(numFrames));
    }
    else
    {
      std::fill(outputL, outputL + numFrames, 0.0f);
      std::fill(outputR, outputR + numFrames, 0.0f);
    }
  }

  applyOutputGain(outputL, numFrames);
  applyOutputGain(outputR, numFrames);
}

void IRProcessor::processDualMonoWithSidechain(const Sample* inputL, const Sample* inputR,
                                               const Sample* sidechainL, const Sample* sidechainR,
                                               Sample* outputL, Sample* outputR,
                                               FrameCount numFrames)
{
  processStereoWithSidechain(inputL, inputR, sidechainL, sidechainR, outputL, outputR, numFrames);
}

float IRProcessor::calculateDynamicBlend(float inputLevelDb) const
{
  float kneeStart = thresholdDb_ - (kneeWidthDb_ / 2.0f);
  float kneeEnd = thresholdDb_ + (kneeWidthDb_ / 2.0f);

  float blendPosition = 0.0f;

  if (inputLevelDb <= kneeStart)
  {
    blendPosition = 0.0f;
  }
  else if (inputLevelDb >= thresholdDb_ + rangeDb_)
  {
    blendPosition = 1.0f;
  }
  else if (inputLevelDb < kneeEnd)
  {
    float kneeInput = inputLevelDb - kneeStart;
    float kneeOvershoot = kneeInput / kneeWidthDb_;
    blendPosition = (kneeOvershoot * kneeOvershoot) / 2.0f;
  }
  else
  {
    float aboveKnee = inputLevelDb - kneeEnd;
    float effectiveRange = rangeDb_ - (kneeWidthDb_ / 2.0f);
    float kneeContribution = (kneeWidthDb_ > 0.0f) ? 0.5f : 0.0f;
    blendPosition = kneeContribution + ((aboveKnee / effectiveRange) * (1.0f - kneeContribution));
    blendPosition = std::min(1.0f, blendPosition);
  }

  return -1.0f + 2.0f * blendPosition;
}

float IRProcessor::detectPeakLevel(const Sample* buffer, FrameCount numFrames)
{
  if (numFrames == 0)
  {
    return -96.0f;
  }

  float peak = 0.0f;
  for (FrameCount i = 0; i < numFrames; ++i)
  {
    float absSample = std::abs(buffer[i]);
    peak = std::max(peak, absSample);
  }

  if (peak < 1e-6f)
  {
    return -96.0f;
  }

  return 20.0f * std::log10(peak);
}

float IRProcessor::detectRMSLevel(const Sample* buffer, FrameCount numFrames)
{
  if (numFrames == 0 || rmsBufferSize_ == 0)
  {
    return -96.0f;
  }

  for (FrameCount i = 0; i < numFrames; ++i)
  {
    float sample = buffer[i];
    rmsBuffer_[rmsBufferIndex_] = sample * sample;
    rmsBufferIndex_ = (rmsBufferIndex_ + 1) % rmsBufferSize_;
  }

  float sumSquares = 0.0f;
  for (size_t i = 0; i < rmsBufferSize_; ++i)
  {
    sumSquares += rmsBuffer_[i];
  }
  float rms = std::sqrt(sumSquares / static_cast<float>(rmsBufferSize_));

  if (rms < 1e-6f)
  {
    return -96.0f;
  }

  return 20.0f * std::log10(rms);
}

void IRProcessor::updateSmoothingCoefficients()
{
  if (sampleRate_ <= 0.0)
  {
    attackCoeff_ = 0.0f;
    releaseCoeff_ = 0.0f;
    return;
  }

  // Coefficients are calculated for per-sample application.
  // In processing functions, they are adjusted via pow(coeff, numFrames)
  // to compensate for per-buffer (not per-sample) envelope updates.

  if (attackTimeMs_ > 0.0f)
  {
    float attackTimeSeconds = attackTimeMs_ / 1000.0f;
    attackCoeff_ = std::exp(-1.0f / (attackTimeSeconds * static_cast<float>(sampleRate_)));
  }
  else
  {
    attackCoeff_ = 0.0f;
  }

  if (releaseTimeMs_ > 0.0f)
  {
    float releaseTimeSeconds = releaseTimeMs_ / 1000.0f;
    releaseCoeff_ = std::exp(-1.0f / (releaseTimeSeconds * static_cast<float>(sampleRate_)));
  }
  else
  {
    releaseCoeff_ = 0.0f;
  }

  static constexpr float kBlendSmoothTimeMs = 5.0f;
  blendSmoothCoeff_ =
      std::exp(-1.0f / ((kBlendSmoothTimeMs / 1000.0f) * static_cast<float>(sampleRate_)));
}

void IRProcessor::applyOutputGain(Sample* buffer, FrameCount numFrames) const
{
  if (outputGainLinear_ == 1.0f)
  {
    return;
  }

  for (FrameCount i = 0; i < numFrames; ++i)
  {
    buffer[i] *= outputGainLinear_;
  }
}

void IRProcessor::updateDelayBuffers()
{
  const int maxLatency = std::max(latencySamples1_, latencySamples2_);
  maxLatencySamples_.store(maxLatency <= 0 ? 0 : maxLatency);

  if (maxLatency <= 0)
    return;

  const size_t bufferSize = static_cast<size_t>(maxLatency);

  if (dryDelayBufferL_.size() != bufferSize)
  {
    dryDelayBufferL_.resize(bufferSize, 0.0f);
    dryDelayBufferR_.resize(bufferSize, 0.0f);
    ir1DelayBufferL_.resize(bufferSize, 0.0f);
    ir1DelayBufferR_.resize(bufferSize, 0.0f);
    ir2DelayBufferL_.resize(bufferSize, 0.0f);
    ir2DelayBufferR_.resize(bufferSize, 0.0f);
    dryDelayWritePosL_ = 0;
    dryDelayWritePosR_ = 0;
    ir1DelayWritePosL_ = 0;
    ir1DelayWritePosR_ = 0;
    ir2DelayWritePosL_ = 0;
    ir2DelayWritePosR_ = 0;
  }
}

void IRProcessor::writeToDelayBuffer(std::vector<Sample>& buffer, size_t& writePos,
                                     const Sample* input, FrameCount numFrames)
{
  if (buffer.empty())
  {
    return;
  }

  const size_t bufferSize = buffer.size();

  for (FrameCount i = 0; i < numFrames; ++i)
  {
    buffer[writePos] = input[i];
    writePos = (writePos + 1) % bufferSize;
  }
}

void IRProcessor::readFromDelayBuffer(const std::vector<Sample>& buffer, size_t writePos,
                                      Sample* output, FrameCount numFrames, int delaySamples)
{
  if (buffer.empty() || delaySamples <= 0)
  {
    return;
  }

  const size_t bufferSize = buffer.size();
  const int clampedDelay = std::min(delaySamples, static_cast<int>(bufferSize));

  for (FrameCount i = 0; i < numFrames; ++i)
  {
    const size_t readPos = (writePos + bufferSize - clampedDelay + i) % bufferSize;
    output[i] = buffer[readPos];
  }
}

}  // namespace octob
