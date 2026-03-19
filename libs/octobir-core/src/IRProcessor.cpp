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
  const IRLoadResult result = irLoader1_->loadFromFile(filepath);

  if (!result.success)
  {
    errorMessage = result.errorMessage;
    ir1Loaded_ = false;
    return false;
  }

  if (!irLoader1_->resampleAndInitialize(*impulseBuffer1_, sampleRate_))
  {
    errorMessage = "Failed to resample IR to target sample rate";
    ir1Loaded_ = false;
    return false;
  }

  const int irLength = impulseBuffer1_->GetLength();
  const int irChannels = impulseBuffer1_->GetNumChannels();
  const double irSampleRate = impulseBuffer1_->samplerate;

  if (irLength <= 0)
  {
    errorMessage = "IR buffer length is invalid: " + std::to_string(irLength);
    ir1Loaded_ = false;
    return false;
  }

  if (irChannels <= 0)
  {
    errorMessage = "IR buffer channels is invalid: " + std::to_string(irChannels);
    ir1Loaded_ = false;
    return false;
  }

  if (irSampleRate <= 0)
  {
    errorMessage = "IR sample rate is invalid: " + std::to_string(irSampleRate);
    ir1Loaded_ = false;
    return false;
  }

  ir1PeakOffset_ = IRLoader::findPeakSampleIndex(*impulseBuffer1_);

  latencySamples1_ =
      convolutionEngine1_->SetImpulse(impulseBuffer1_.get(), 64, 0, 0, ir1PeakOffset_);
  if (latencySamples1_ < 0)
  {
    errorMessage = "Failed to initialize convolution engine with IR (returned " +
                   std::to_string(latencySamples1_) + "). IR: " + std::to_string(irLength) +
                   " samples, " + std::to_string(irChannels) + " channels, " +
                   std::to_string(irSampleRate) + " Hz";
    ir1Loaded_ = false;
    latencySamples1_ = 0;
    ir1PeakOffset_ = 0;
    return false;
  }

  currentIR1Path_ = filepath;
  ir1Loaded_ = true;
  errorMessage.clear();

  updateDelayBuffers();

  return true;
}

bool IRProcessor::loadImpulseResponse2(const std::string& filepath, std::string& errorMessage)
{
  const IRLoadResult result = irLoader2_->loadFromFile(filepath);

  if (!result.success)
  {
    errorMessage = result.errorMessage;
    ir2Loaded_ = false;
    return false;
  }

  if (!irLoader2_->resampleAndInitialize(*impulseBuffer2_, sampleRate_))
  {
    errorMessage = "Failed to resample IR2 to target sample rate";
    ir2Loaded_ = false;
    return false;
  }

  const int irLength = impulseBuffer2_->GetLength();
  const int irChannels = impulseBuffer2_->GetNumChannels();
  const double irSampleRate = impulseBuffer2_->samplerate;

  if (irLength <= 0)
  {
    errorMessage = "IR2 buffer length is invalid: " + std::to_string(irLength);
    ir2Loaded_ = false;
    return false;
  }

  if (irChannels <= 0)
  {
    errorMessage = "IR2 buffer channels is invalid: " + std::to_string(irChannels);
    ir2Loaded_ = false;
    return false;
  }

  if (irSampleRate <= 0)
  {
    errorMessage = "IR2 sample rate is invalid: " + std::to_string(irSampleRate);
    ir2Loaded_ = false;
    return false;
  }

  ir2PeakOffset_ = IRLoader::findPeakSampleIndex(*impulseBuffer2_);

  latencySamples2_ =
      convolutionEngine2_->SetImpulse(impulseBuffer2_.get(), 64, 0, 0, ir2PeakOffset_);
  if (latencySamples2_ < 0)
  {
    errorMessage = "Failed to initialize convolution engine with IR2 (returned " +
                   std::to_string(latencySamples2_) + "). IR2: " + std::to_string(irLength) +
                   " samples, " + std::to_string(irChannels) + " channels, " +
                   std::to_string(irSampleRate) + " Hz";
    ir2Loaded_ = false;
    latencySamples2_ = 0;
    ir2PeakOffset_ = 0;
    return false;
  }

  currentIR2Path_ = filepath;
  ir2Loaded_ = true;
  errorMessage.clear();

  updateDelayBuffers();

  return true;
}

void IRProcessor::clearImpulseResponse1()
{
  ir1Loaded_ = false;
  currentIR1Path_.clear();
  latencySamples1_ = 0;
  ir1PeakOffset_ = 0;

  if (convolutionEngine1_)
  {
    convolutionEngine1_->Reset();
  }

  updateDelayBuffers();
}

void IRProcessor::clearImpulseResponse2()
{
  ir2Loaded_ = false;
  currentIR2Path_.clear();
  latencySamples2_ = 0;
  ir2PeakOffset_ = 0;

  if (convolutionEngine2_)
  {
    convolutionEngine2_->Reset();
  }

  updateDelayBuffers();
}

void IRProcessor::setSampleRate(SampleRate sampleRate)
{
  if (sampleRate_ != sampleRate)
  {
    sampleRate_ = sampleRate;
    updateSmoothingCoefficients();
    updateRMSBufferSize();

    if (ir1Loaded_)
    {
      irLoader1_->resampleAndInitialize(*impulseBuffer1_, sampleRate_);
      ir1PeakOffset_ = IRLoader::findPeakSampleIndex(*impulseBuffer1_);
      convolutionEngine1_->Reset();
      latencySamples1_ =
          convolutionEngine1_->SetImpulse(impulseBuffer1_.get(), 64, 0, 0, ir1PeakOffset_);
    }

    if (ir2Loaded_)
    {
      irLoader2_->resampleAndInitialize(*impulseBuffer2_, sampleRate_);
      ir2PeakOffset_ = IRLoader::findPeakSampleIndex(*impulseBuffer2_);
      convolutionEngine2_->Reset();
      latencySamples2_ =
          convolutionEngine2_->SetImpulse(impulseBuffer2_.get(), 64, 0, 0, ir2PeakOffset_);
    }
  }
}

void IRProcessor::setBlend(float blend)
{
  blend_ = std::max(-1.0f, std::min(1.0f, blend));
  if (!dynamicModeEnabled_)
  {
    currentBlend_ = blend_;
    smoothedBlend_ = blend_;
  }
}

void IRProcessor::setDynamicModeEnabled(bool enabled)
{
  dynamicModeEnabled_ = enabled;
  if (!enabled)
  {
    currentBlend_ = blend_;
    smoothedBlend_ = blend_;
  }
}

void IRProcessor::setSidechainEnabled(bool enabled)
{
  sidechainEnabled_ = enabled;
}

void IRProcessor::setLowBlend(float lowBlend)
{
  lowBlend_ = std::max(-1.0f, std::min(1.0f, lowBlend));
}

void IRProcessor::setHighBlend(float highBlend)
{
  highBlend_ = std::max(-1.0f, std::min(1.0f, highBlend));
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
  attackTimeMs_ = std::max(1.0f, std::min(500.0f, attackTimeMs));
  updateSmoothingCoefficients();
}

void IRProcessor::setReleaseTime(float releaseTimeMs)
{
  releaseTimeMs_ = std::max(1.0f, std::min(1000.0f, releaseTimeMs));
  updateSmoothingCoefficients();
}

void IRProcessor::setOutputGain(float gainDb)
{
  outputGainDb_ = std::max(-24.0f, std::min(24.0f, gainDb));
  outputGainLinear_ = std::pow(10.0f, outputGainDb_ / 20.0f);
}

void IRProcessor::setIRAEnabled(bool enabled)
{
  irAEnabled_ = enabled;
}

void IRProcessor::setIRBEnabled(bool enabled)
{
  irBEnabled_ = enabled;
}

void IRProcessor::processMono(const Sample* input, Sample* output, FrameCount numFrames)
{
  bool hasIR1 = ir1Loaded_ && irAEnabled_;
  bool hasIR2 = ir2Loaded_ && irBEnabled_;

  if (!hasIR1 && !hasIR2)
  {
    std::copy(input, input + numFrames, output);
    applyOutputGain(output, numFrames);
    return;
  }

  currentInputLevelDb_ =
      (detectionMode_ == 0) ? detectPeakLevel(input, numFrames) : detectRMSLevel(input, numFrames);

  float blendToUse = blend_;
  if (dynamicModeEnabled_ && !sidechainEnabled_)
  {
    float targetBlend = calculateDynamicBlend(currentInputLevelDb_);
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
  else
  {
    currentBlend_ = blend_;
  }

  float normalizedBlend = (blendToUse + 1.0f) * 0.5f;
  float gain1 = 0.0f;
  float gain2 = 0.0f;

  if (hasIR1 && hasIR2)
  {
    gain1 = std::sqrt(1.0f - normalizedBlend);
    gain2 = std::sqrt(normalizedBlend);
  }
  else
  {
    gain1 = 1.0f - normalizedBlend;
    gain2 = normalizedBlend;
  }

  if (hasIR1 && hasIR2)
  {
    WDL_FFT_REAL* inputPtr = const_cast<WDL_FFT_REAL*>(input);
    convolutionEngine1_->Add(&inputPtr, static_cast<int>(numFrames), 1);
    convolutionEngine2_->Add(&inputPtr, static_cast<int>(numFrames), 1);

    int available1 = convolutionEngine1_->Avail(static_cast<int>(numFrames));
    int available2 = convolutionEngine2_->Avail(static_cast<int>(numFrames));

    if (available1 >= static_cast<int>(numFrames) && available2 >= static_cast<int>(numFrames))
    {
      WDL_FFT_REAL** output1Ptr = convolutionEngine1_->Get();
      WDL_FFT_REAL** output2Ptr = convolutionEngine2_->Get();

      const int latencyDiff = latencySamples1_ - latencySamples2_;

      if (latencyDiff > 0)
      {
        writeToDelayBuffer(ir1DelayBufferL_, output1Ptr[0], numFrames);
        std::vector<Sample> delayedIR1(numFrames);
        readFromDelayBuffer(ir1DelayBufferL_, delayedIR1.data(), numFrames, latencyDiff);

        for (FrameCount i = 0; i < numFrames; ++i)
        {
          output[i] = gain1 * delayedIR1[i] + gain2 * output2Ptr[0][i];
        }
      }
      else if (latencyDiff < 0)
      {
        writeToDelayBuffer(ir2DelayBufferL_, output2Ptr[0], numFrames);
        std::vector<Sample> delayedIR2(numFrames);
        readFromDelayBuffer(ir2DelayBufferL_, delayedIR2.data(), numFrames, -latencyDiff);

        for (FrameCount i = 0; i < numFrames; ++i)
        {
          output[i] = gain1 * output1Ptr[0][i] + gain2 * delayedIR2[i];
        }
      }
      else
      {
        for (FrameCount i = 0; i < numFrames; ++i)
        {
          output[i] = gain1 * output1Ptr[0][i] + gain2 * output2Ptr[0][i];
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
    writeToDelayBuffer(dryDelayBufferL_, input, numFrames);

    WDL_FFT_REAL* inputPtr = const_cast<WDL_FFT_REAL*>(input);
    convolutionEngine1_->Add(&inputPtr, static_cast<int>(numFrames), 1);

    int available = convolutionEngine1_->Avail(static_cast<int>(numFrames));
    if (available >= static_cast<int>(numFrames))
    {
      WDL_FFT_REAL** outputPtr = convolutionEngine1_->Get();

      std::vector<Sample> delayedDry(numFrames);
      readFromDelayBuffer(dryDelayBufferL_, delayedDry.data(), numFrames, latencySamples1_);

      for (FrameCount i = 0; i < numFrames; ++i)
      {
        output[i] = gain1 * outputPtr[0][i] + gain2 * delayedDry[i];
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
    writeToDelayBuffer(dryDelayBufferL_, input, numFrames);

    WDL_FFT_REAL* inputPtr = const_cast<WDL_FFT_REAL*>(input);
    convolutionEngine2_->Add(&inputPtr, static_cast<int>(numFrames), 1);

    int available = convolutionEngine2_->Avail(static_cast<int>(numFrames));
    if (available >= static_cast<int>(numFrames))
    {
      WDL_FFT_REAL** outputPtr = convolutionEngine2_->Get();

      std::vector<Sample> delayedDry(numFrames);
      readFromDelayBuffer(dryDelayBufferL_, delayedDry.data(), numFrames, latencySamples2_);

      for (FrameCount i = 0; i < numFrames; ++i)
      {
        output[i] = gain1 * delayedDry[i] + gain2 * outputPtr[0][i];
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
  return maxLatencySamples_;
}

void IRProcessor::processStereo(const Sample* inputL, const Sample* inputR, Sample* outputL,
                                Sample* outputR, FrameCount numFrames)
{
  bool hasIR1 = ir1Loaded_ && irAEnabled_;
  bool hasIR2 = ir2Loaded_ && irBEnabled_;

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

  float blendToUse = blend_;
  if (dynamicModeEnabled_ && !sidechainEnabled_)
  {
    float targetBlend = calculateDynamicBlend(currentInputLevelDb_);
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
  else
  {
    currentBlend_ = blend_;
  }

  float normalizedBlend = (blendToUse + 1.0f) * 0.5f;
  float gain1 = 0.0f;
  float gain2 = 0.0f;

  if (hasIR1 && hasIR2)
  {
    gain1 = std::sqrt(1.0f - normalizedBlend);
    gain2 = std::sqrt(normalizedBlend);
  }
  else
  {
    gain1 = 1.0f - normalizedBlend;
    gain2 = normalizedBlend;
  }

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
        writeToDelayBuffer(ir1DelayBufferL_, output1Ptr[0], numFrames);
        writeToDelayBuffer(ir1DelayBufferR_, output1Ptr[1], numFrames);
        std::vector<Sample> delayedIR1L(numFrames);
        std::vector<Sample> delayedIR1R(numFrames);
        readFromDelayBuffer(ir1DelayBufferL_, delayedIR1L.data(), numFrames, latencyDiff);
        readFromDelayBuffer(ir1DelayBufferR_, delayedIR1R.data(), numFrames, latencyDiff);

        for (FrameCount i = 0; i < numFrames; ++i)
        {
          outputL[i] = gain1 * delayedIR1L[i] + gain2 * output2Ptr[0][i];
          outputR[i] = gain1 * delayedIR1R[i] + gain2 * output2Ptr[1][i];
        }
      }
      else if (latencyDiff < 0)
      {
        writeToDelayBuffer(ir2DelayBufferL_, output2Ptr[0], numFrames);
        writeToDelayBuffer(ir2DelayBufferR_, output2Ptr[1], numFrames);
        std::vector<Sample> delayedIR2L(numFrames);
        std::vector<Sample> delayedIR2R(numFrames);
        readFromDelayBuffer(ir2DelayBufferL_, delayedIR2L.data(), numFrames, -latencyDiff);
        readFromDelayBuffer(ir2DelayBufferR_, delayedIR2R.data(), numFrames, -latencyDiff);

        for (FrameCount i = 0; i < numFrames; ++i)
        {
          outputL[i] = gain1 * output1Ptr[0][i] + gain2 * delayedIR2L[i];
          outputR[i] = gain1 * output1Ptr[1][i] + gain2 * delayedIR2R[i];
        }
      }
      else
      {
        for (FrameCount i = 0; i < numFrames; ++i)
        {
          outputL[i] = gain1 * output1Ptr[0][i] + gain2 * output2Ptr[0][i];
          outputR[i] = gain1 * output1Ptr[1][i] + gain2 * output2Ptr[1][i];
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
    writeToDelayBuffer(dryDelayBufferL_, inputL, numFrames);
    writeToDelayBuffer(dryDelayBufferR_, inputR, numFrames);

    convolutionEngine1_->Add(inputPtrs.data(), static_cast<int>(numFrames), 2);

    int available = convolutionEngine1_->Avail(static_cast<int>(numFrames));
    if (available >= static_cast<int>(numFrames))
    {
      WDL_FFT_REAL** outputPtr = convolutionEngine1_->Get();

      std::vector<Sample> delayedDryL(numFrames);
      std::vector<Sample> delayedDryR(numFrames);
      readFromDelayBuffer(dryDelayBufferL_, delayedDryL.data(), numFrames, latencySamples1_);
      readFromDelayBuffer(dryDelayBufferR_, delayedDryR.data(), numFrames, latencySamples1_);

      for (FrameCount i = 0; i < numFrames; ++i)
      {
        outputL[i] = gain1 * outputPtr[0][i] + gain2 * delayedDryL[i];
        outputR[i] = gain1 * outputPtr[1][i] + gain2 * delayedDryR[i];
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
    writeToDelayBuffer(dryDelayBufferL_, inputL, numFrames);
    writeToDelayBuffer(dryDelayBufferR_, inputR, numFrames);

    convolutionEngine2_->Add(inputPtrs.data(), static_cast<int>(numFrames), 2);

    int available = convolutionEngine2_->Avail(static_cast<int>(numFrames));
    if (available >= static_cast<int>(numFrames))
    {
      WDL_FFT_REAL** outputPtr = convolutionEngine2_->Get();

      std::vector<Sample> delayedDryL(numFrames);
      std::vector<Sample> delayedDryR(numFrames);
      readFromDelayBuffer(dryDelayBufferL_, delayedDryL.data(), numFrames, latencySamples2_);
      readFromDelayBuffer(dryDelayBufferR_, delayedDryR.data(), numFrames, latencySamples2_);

      for (FrameCount i = 0; i < numFrames; ++i)
      {
        outputL[i] = gain1 * delayedDryL[i] + gain2 * outputPtr[0][i];
        outputR[i] = gain1 * delayedDryR[i] + gain2 * outputPtr[1][i];
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

void IRProcessor::processMonoWithSidechain(const Sample* input, const Sample* sidechain,
                                           Sample* output, FrameCount numFrames)
{
  bool hasIR1 = ir1Loaded_ && irAEnabled_;
  bool hasIR2 = ir2Loaded_ && irBEnabled_;

  if (!hasIR1 && !hasIR2)
  {
    std::copy(input, input + numFrames, output);
    applyOutputGain(output, numFrames);
    return;
  }

  currentInputLevelDb_ = (detectionMode_ == 0) ? detectPeakLevel(sidechain, numFrames)
                                               : detectRMSLevel(sidechain, numFrames);

  float blendToUse = blend_;
  if (dynamicModeEnabled_ && sidechainEnabled_)
  {
    float targetBlend = calculateDynamicBlend(currentInputLevelDb_);
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
  else
  {
    currentBlend_ = blend_;
  }

  float normalizedBlend = (blendToUse + 1.0f) * 0.5f;
  float gain1 = 0.0f;
  float gain2 = 0.0f;

  if (hasIR1 && hasIR2)
  {
    gain1 = std::sqrt(1.0f - normalizedBlend);
    gain2 = std::sqrt(normalizedBlend);
  }
  else
  {
    gain1 = 1.0f - normalizedBlend;
    gain2 = normalizedBlend;
  }

  if (hasIR1 && hasIR2)
  {
    WDL_FFT_REAL* inputPtr = const_cast<WDL_FFT_REAL*>(input);
    convolutionEngine1_->Add(&inputPtr, static_cast<int>(numFrames), 1);
    convolutionEngine2_->Add(&inputPtr, static_cast<int>(numFrames), 1);

    int available1 = convolutionEngine1_->Avail(static_cast<int>(numFrames));
    int available2 = convolutionEngine2_->Avail(static_cast<int>(numFrames));

    if (available1 >= static_cast<int>(numFrames) && available2 >= static_cast<int>(numFrames))
    {
      WDL_FFT_REAL** output1Ptr = convolutionEngine1_->Get();
      WDL_FFT_REAL** output2Ptr = convolutionEngine2_->Get();

      const int latencyDiff = latencySamples1_ - latencySamples2_;

      if (latencyDiff > 0)
      {
        writeToDelayBuffer(ir1DelayBufferL_, output1Ptr[0], numFrames);
        std::vector<Sample> delayedIR1(numFrames);
        readFromDelayBuffer(ir1DelayBufferL_, delayedIR1.data(), numFrames, latencyDiff);

        for (FrameCount i = 0; i < numFrames; ++i)
        {
          output[i] = gain1 * delayedIR1[i] + gain2 * output2Ptr[0][i];
        }
      }
      else if (latencyDiff < 0)
      {
        writeToDelayBuffer(ir2DelayBufferL_, output2Ptr[0], numFrames);
        std::vector<Sample> delayedIR2(numFrames);
        readFromDelayBuffer(ir2DelayBufferL_, delayedIR2.data(), numFrames, -latencyDiff);

        for (FrameCount i = 0; i < numFrames; ++i)
        {
          output[i] = gain1 * output1Ptr[0][i] + gain2 * delayedIR2[i];
        }
      }
      else
      {
        for (FrameCount i = 0; i < numFrames; ++i)
        {
          output[i] = gain1 * output1Ptr[0][i] + gain2 * output2Ptr[0][i];
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
    writeToDelayBuffer(dryDelayBufferL_, input, numFrames);

    WDL_FFT_REAL* inputPtr = const_cast<WDL_FFT_REAL*>(input);
    convolutionEngine1_->Add(&inputPtr, static_cast<int>(numFrames), 1);

    int available = convolutionEngine1_->Avail(static_cast<int>(numFrames));
    if (available >= static_cast<int>(numFrames))
    {
      WDL_FFT_REAL** outputPtr = convolutionEngine1_->Get();

      std::vector<Sample> delayedDry(numFrames);
      readFromDelayBuffer(dryDelayBufferL_, delayedDry.data(), numFrames, latencySamples1_);

      for (FrameCount i = 0; i < numFrames; ++i)
      {
        output[i] = gain1 * outputPtr[0][i] + gain2 * delayedDry[i];
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
    writeToDelayBuffer(dryDelayBufferL_, input, numFrames);

    WDL_FFT_REAL* inputPtr = const_cast<WDL_FFT_REAL*>(input);
    convolutionEngine2_->Add(&inputPtr, static_cast<int>(numFrames), 1);

    int available = convolutionEngine2_->Avail(static_cast<int>(numFrames));
    if (available >= static_cast<int>(numFrames))
    {
      WDL_FFT_REAL** outputPtr = convolutionEngine2_->Get();

      std::vector<Sample> delayedDry(numFrames);
      readFromDelayBuffer(dryDelayBufferL_, delayedDry.data(), numFrames, latencySamples2_);

      for (FrameCount i = 0; i < numFrames; ++i)
      {
        output[i] = gain1 * delayedDry[i] + gain2 * outputPtr[0][i];
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

void IRProcessor::processStereoWithSidechain(const Sample* inputL, const Sample* inputR,
                                             const Sample* sidechainL, const Sample* sidechainR,
                                             Sample* outputL, Sample* outputR, FrameCount numFrames)
{
  bool hasIR1 = ir1Loaded_ && irAEnabled_;
  bool hasIR2 = ir2Loaded_ && irBEnabled_;

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

  float blendToUse = blend_;
  if (dynamicModeEnabled_ && sidechainEnabled_)
  {
    float targetBlend = calculateDynamicBlend(currentInputLevelDb_);
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
  else
  {
    currentBlend_ = blend_;
  }

  float normalizedBlend = (blendToUse + 1.0f) * 0.5f;
  float gain1 = 0.0f;
  float gain2 = 0.0f;

  if (hasIR1 && hasIR2)
  {
    gain1 = std::sqrt(1.0f - normalizedBlend);
    gain2 = std::sqrt(normalizedBlend);
  }
  else
  {
    gain1 = 1.0f - normalizedBlend;
    gain2 = normalizedBlend;
  }

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
        writeToDelayBuffer(ir1DelayBufferL_, output1Ptr[0], numFrames);
        writeToDelayBuffer(ir1DelayBufferR_, output1Ptr[1], numFrames);
        std::vector<Sample> delayedIR1L(numFrames);
        std::vector<Sample> delayedIR1R(numFrames);
        readFromDelayBuffer(ir1DelayBufferL_, delayedIR1L.data(), numFrames, latencyDiff);
        readFromDelayBuffer(ir1DelayBufferR_, delayedIR1R.data(), numFrames, latencyDiff);

        for (FrameCount i = 0; i < numFrames; ++i)
        {
          outputL[i] = gain1 * delayedIR1L[i] + gain2 * output2Ptr[0][i];
          outputR[i] = gain1 * delayedIR1R[i] + gain2 * output2Ptr[1][i];
        }
      }
      else if (latencyDiff < 0)
      {
        writeToDelayBuffer(ir2DelayBufferL_, output2Ptr[0], numFrames);
        writeToDelayBuffer(ir2DelayBufferR_, output2Ptr[1], numFrames);
        std::vector<Sample> delayedIR2L(numFrames);
        std::vector<Sample> delayedIR2R(numFrames);
        readFromDelayBuffer(ir2DelayBufferL_, delayedIR2L.data(), numFrames, -latencyDiff);
        readFromDelayBuffer(ir2DelayBufferR_, delayedIR2R.data(), numFrames, -latencyDiff);

        for (FrameCount i = 0; i < numFrames; ++i)
        {
          outputL[i] = gain1 * output1Ptr[0][i] + gain2 * delayedIR2L[i];
          outputR[i] = gain1 * output1Ptr[1][i] + gain2 * delayedIR2R[i];
        }
      }
      else
      {
        for (FrameCount i = 0; i < numFrames; ++i)
        {
          outputL[i] = gain1 * output1Ptr[0][i] + gain2 * output2Ptr[0][i];
          outputR[i] = gain1 * output1Ptr[1][i] + gain2 * output2Ptr[1][i];
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
    writeToDelayBuffer(dryDelayBufferL_, inputL, numFrames);
    writeToDelayBuffer(dryDelayBufferR_, inputR, numFrames);

    convolutionEngine1_->Add(inputPtrs.data(), static_cast<int>(numFrames), 2);

    int available = convolutionEngine1_->Avail(static_cast<int>(numFrames));
    if (available >= static_cast<int>(numFrames))
    {
      WDL_FFT_REAL** outputPtr = convolutionEngine1_->Get();

      std::vector<Sample> delayedDryL(numFrames);
      std::vector<Sample> delayedDryR(numFrames);
      readFromDelayBuffer(dryDelayBufferL_, delayedDryL.data(), numFrames, latencySamples1_);
      readFromDelayBuffer(dryDelayBufferR_, delayedDryR.data(), numFrames, latencySamples1_);

      for (FrameCount i = 0; i < numFrames; ++i)
      {
        outputL[i] = gain1 * outputPtr[0][i] + gain2 * delayedDryL[i];
        outputR[i] = gain1 * outputPtr[1][i] + gain2 * delayedDryR[i];
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
    writeToDelayBuffer(dryDelayBufferL_, inputL, numFrames);
    writeToDelayBuffer(dryDelayBufferR_, inputR, numFrames);

    convolutionEngine2_->Add(inputPtrs.data(), static_cast<int>(numFrames), 2);

    int available = convolutionEngine2_->Avail(static_cast<int>(numFrames));
    if (available >= static_cast<int>(numFrames))
    {
      WDL_FFT_REAL** outputPtr = convolutionEngine2_->Get();

      std::vector<Sample> delayedDryL(numFrames);
      std::vector<Sample> delayedDryR(numFrames);
      readFromDelayBuffer(dryDelayBufferL_, delayedDryL.data(), numFrames, latencySamples2_);
      readFromDelayBuffer(dryDelayBufferR_, delayedDryR.data(), numFrames, latencySamples2_);

      for (FrameCount i = 0; i < numFrames; ++i)
      {
        outputL[i] = gain1 * delayedDryL[i] + gain2 * outputPtr[0][i];
        outputR[i] = gain1 * delayedDryR[i] + gain2 * outputPtr[1][i];
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
  else if (inputLevelDb >= kneeEnd && inputLevelDb >= thresholdDb_ + rangeDb_)
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

  return blend_ + (highBlend_ - blend_) * blendPosition;
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
  maxLatencySamples_ = std::max(latencySamples1_, latencySamples2_);

  if (maxLatencySamples_ <= 0)
  {
    maxLatencySamples_ = 0;
    return;
  }

  const size_t bufferSize = static_cast<size_t>(maxLatencySamples_);

  if (dryDelayBufferL_.size() != bufferSize)
  {
    dryDelayBufferL_.resize(bufferSize, 0.0f);
    dryDelayBufferR_.resize(bufferSize, 0.0f);
    ir1DelayBufferL_.resize(bufferSize, 0.0f);
    ir1DelayBufferR_.resize(bufferSize, 0.0f);
    ir2DelayBufferL_.resize(bufferSize, 0.0f);
    ir2DelayBufferR_.resize(bufferSize, 0.0f);
    delayBufferWritePos_ = 0;
  }
}

void IRProcessor::writeToDelayBuffer(std::vector<Sample>& buffer, const Sample* input,
                                     FrameCount numFrames)
{
  if (buffer.empty())
  {
    return;
  }

  const size_t bufferSize = buffer.size();

  for (FrameCount i = 0; i < numFrames; ++i)
  {
    buffer[delayBufferWritePos_] = input[i];
    delayBufferWritePos_ = (delayBufferWritePos_ + 1) % bufferSize;
  }
}

void IRProcessor::readFromDelayBuffer(const std::vector<Sample>& buffer, Sample* output,
                                      FrameCount numFrames, int delaySamples) const
{
  if (buffer.empty() || delaySamples <= 0)
  {
    return;
  }

  const size_t bufferSize = buffer.size();
  const int clampedDelay = std::min(delaySamples, static_cast<int>(bufferSize));

  for (FrameCount i = 0; i < numFrames; ++i)
  {
    const size_t readPos = (delayBufferWritePos_ + bufferSize - clampedDelay + i) % bufferSize;
    output[i] = buffer[readPos];
  }
}

}  // namespace octob
