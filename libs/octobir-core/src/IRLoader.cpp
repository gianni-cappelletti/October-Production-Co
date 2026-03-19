#include "octobir-core/IRLoader.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#define DR_WAV_IMPLEMENTATION
#include <convoengine.h>
#include <resample.h>

#include "dr_wav.h"

namespace octob
{

IRLoader::IRLoader() = default;
IRLoader::~IRLoader() = default;

IRLoadResult IRLoader::loadFromFile(const std::string& filepath)
{
  IRLoadResult result;

  uint32_t channels = 0;
  uint32_t sampleRate = 0;
  drwav_uint64 totalPCMFrameCount = 0;

  float* sampleData = drwav_open_file_and_read_pcm_frames_f32(
      filepath.c_str(), &channels, &sampleRate, &totalPCMFrameCount, nullptr);

  if (sampleData == nullptr)
  {
    result.success = false;
    result.errorMessage = "Failed to open or read WAV file";
    return result;
  }

  const auto irLength = static_cast<size_t>(totalPCMFrameCount);
  irBuffer_.clear();

  if (channels == 1)
  {
    irBuffer_.resize(irLength);
    for (size_t i = 0; i < irLength; i++)
    {
      irBuffer_[i] = sampleData[i];
    }
  }
  else if (channels == 2)
  {
    irBuffer_.resize(irLength * 2);
    for (size_t i = 0; i < irLength; i++)
    {
      irBuffer_[(i * 2)] = sampleData[(i * 2)];
      irBuffer_[(i * 2) + 1] = sampleData[(i * 2) + 1];
    }
  }
  else
  {
    irBuffer_.resize(irLength);
    for (size_t i = 0; i < irLength; i++)
    {
      irBuffer_[i] = sampleData[i * channels];
    }
  }

  drwav_free(sampleData, nullptr);

  const float irCompensationGain = std::exp(kIrCompensationGainDb * kDbToLinearScalar);
  for (auto& sample : irBuffer_)
  {
    sample *= irCompensationGain;
  }

  irSampleRate_ = sampleRate;
  numSamples_ = irLength;
  numChannels_ = static_cast<int>(channels);

  result.success = true;
  result.numSamples = numSamples_;
  result.numChannels = numChannels_;
  result.sampleRate = irSampleRate_;

  return result;
}

bool IRLoader::resampleAndInitialize(WDL_ImpulseBuffer& impulseBuffer, SampleRate targetSampleRate)
{
  if (irBuffer_.empty())
  {
    return false;
  }

  const int outputChannels = 2;

  constexpr float KReferenceSampleRate = 48000.0f;
  const float sampleRateScaling = KReferenceSampleRate / static_cast<float>(targetSampleRate);

  const bool needsResampling = (irSampleRate_ != targetSampleRate);

  if (needsResampling)
  {
    const size_t outFrames = (numSamples_ * static_cast<size_t>(targetSampleRate) +
                              static_cast<size_t>(irSampleRate_) / 2) /
                             static_cast<size_t>(irSampleRate_);

    const int actualLength = impulseBuffer.SetLength(static_cast<int>(outFrames));
    if (actualLength <= 0)
    {
      return false;
    }

    impulseBuffer.SetNumChannels(outputChannels);
    if (impulseBuffer.GetNumChannels() != outputChannels)
    {
      return false;
    }

    impulseBuffer.samplerate = targetSampleRate;

    for (int ch = 0; ch < outputChannels; ch++)
    {
      WDL_Resampler resampler;
      resampler.SetMode(false, 1, false);
      resampler.SetRates(irSampleRate_, targetSampleRate);
      resampler.SetFilterParms(1.0, 0.693);

      std::vector<WDL_ResampleSample> resampledIr(outFrames + 64);

      WDL_ResampleSample* rsinbuf = nullptr;
      int inSamples = static_cast<int>(numSamples_);
      int outSamples = static_cast<int>(outFrames);

      int actualOutSamples = 0;
      while (inSamples > 0 || resampler.ResampleOut(resampledIr.data() + actualOutSamples,
                                                    inSamples, outSamples, 1) > 0)
      {
        const int needed = resampler.ResamplePrepare(inSamples, 1, &rsinbuf);
        for (int i = 0; i < needed && inSamples > 0; i++)
        {
          const int srcCh = (numChannels_ == 1) ? 0 : ch;
          const size_t srcIdx = ((numSamples_ - inSamples) * numChannels_) + srcCh;
          rsinbuf[i] = irBuffer_[srcIdx];
          inSamples--;
        }
        const int processed =
            resampler.ResampleOut(resampledIr.data() + actualOutSamples, inSamples, outSamples, 1);
        if (processed <= 0)
          break;
        actualOutSamples += processed;
        outSamples -= processed;
      }

      WDL_FFT_REAL* irBufferPtr = impulseBuffer.impulses[ch].Get();
      if (irBufferPtr == nullptr)
      {
        return false;
      }

      for (int i = 0; i < actualLength; i++)
      {
        irBufferPtr[i] = (i < actualOutSamples)
                             ? static_cast<WDL_FFT_REAL>(resampledIr[i] * sampleRateScaling)
                             : 0.0f;
      }
    }

    return true;
  }

  const int actualLength = impulseBuffer.SetLength(static_cast<int>(numSamples_));
  if (actualLength <= 0)
  {
    return false;
  }

  impulseBuffer.SetNumChannels(outputChannels);
  if (impulseBuffer.GetNumChannels() != outputChannels)
  {
    return false;
  }

  impulseBuffer.samplerate = targetSampleRate;

  for (int ch = 0; ch < outputChannels; ch++)
  {
    WDL_FFT_REAL* irBufferPtr = impulseBuffer.impulses[ch].Get();
    if (irBufferPtr == nullptr)
    {
      return false;
    }

    for (int i = 0; i < actualLength; i++)
    {
      if (i < static_cast<int>(numSamples_))
      {
        const int srcCh = (numChannels_ == 1) ? 0 : ch;
        const size_t srcIdx = (i * numChannels_) + srcCh;
        irBufferPtr[i] = static_cast<WDL_FFT_REAL>(irBuffer_[srcIdx] * sampleRateScaling);
      }
      else
      {
        irBufferPtr[i] = 0.0f;
      }
    }
  }

  return true;
}

int IRLoader::findPeakSampleIndex(WDL_ImpulseBuffer& impulseBuffer)
{
  const int length = impulseBuffer.GetLength();
  if (length <= 0 || impulseBuffer.GetNumChannels() <= 0)
  {
    return 0;
  }

  const WDL_FFT_REAL* data = impulseBuffer.impulses[0].Get();
  if (data == nullptr)
  {
    return 0;
  }

  int peakIndex = 0;
  WDL_FFT_REAL peakValue = 0.0;
  for (int i = 0; i < length; ++i)
  {
    const WDL_FFT_REAL absVal = std::abs(data[i]);
    if (absVal > peakValue)
    {
      peakValue = absVal;
      peakIndex = i;
    }
  }

  return peakIndex;
}

}  // namespace octob
