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
#include "pffft.h"

namespace octob
{

namespace
{

int nextPow2(int n)
{
  int p = 1;
  while (p < n)
    p <<= 1;
  return p;
}

}  // namespace

IRLoader::IRLoader() = default;
IRLoader::~IRLoader() = default;

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
void IRLoader::convertToMinimumPhase(std::vector<Sample>& samples, int fftSize)
{
  // Cepstrum method: the minimum-phase system with the same magnitude spectrum
  // is computed by zeroing the anticausal half of the real cepstrum.
  // Reference: https://ccrma.stanford.edu/~jos/fp/Conversion_Minimum_Phase.html
  //
  // pffft normalization:
  //   PFFFT_FORWARD  → out = DFT(in)           (unscaled)
  //   PFFFT_BACKWARD → out = N * IDFT(in)       (unnormalized inverse)
  //
  // pffft real transform packed output (pffft_transform_ordered, PFFFT_REAL):
  //   out[0]     = Re(DC)
  //   out[1]     = Re(Nyquist)
  //   out[2k]    = Re(bin k)  for k = 1..N/2-1
  //   out[2k+1]  = Im(bin k)  for k = 1..N/2-1

  const int irLen = static_cast<int>(samples.size());

  PFFFT_Setup* fft = pffft_new_setup(fftSize, PFFFT_REAL);
  if (fft == nullptr)
    return;

  std::vector<float> in(fftSize, 0.0f);
  std::vector<float> out(fftSize, 0.0f);
  std::vector<float> work(fftSize, 0.0f);

  std::copy(samples.begin(), samples.end(), in.begin());

  // Step 1: Forward FFT → H[k]
  pffft_transform_ordered(fft, in.data(), out.data(), work.data(), PFFFT_FORWARD);

  // Step 2: log-magnitude spectrum (real-valued, symmetric for real input)
  std::vector<float> logMag(fftSize, 0.0f);
  logMag[0] = std::log(std::max(std::abs(out[0]), 1e-10f));  // DC
  logMag[1] = std::log(std::max(std::abs(out[1]), 1e-10f));  // Nyquist
  for (int k = 1; k < fftSize / 2; ++k)
  {
    const size_t idx = static_cast<size_t>(k) * 2;
    const float re = out[idx];
    const float im = out[idx + 1];
    logMag[idx] = std::log(std::max(std::hypot(re, im), 1e-10f));
    logMag[idx + 1] = 0.0f;
  }

  // Step 3: IFFT of log-magnitude → real cepstrum
  // pffft BACKWARD returns N * IDFT, so cepstrum[n] = fftSize * true_cepstrum[n]
  pffft_transform_ordered(fft, logMag.data(), in.data(), work.data(), PFFFT_BACKWARD);

  // Step 4: minimum-phase window — double the causal half, zero the anticausal half
  std::vector<float> winCeps(fftSize, 0.0f);
  winCeps[0] = in[0];
  for (int n = 1; n < fftSize / 2; ++n)
    winCeps[n] = 2.0f * in[n];
  winCeps[fftSize / 2] = in[fftSize / 2];

  // Step 5: Forward FFT of windowed cepstrum → log of min-phase spectrum
  // out[k] = N * DFT(true_cepstrum)[k] = N * log(Hmin[k])  (N factor from step 3)
  pffft_transform_ordered(fft, winCeps.data(), out.data(), work.data(), PFFFT_FORWARD);

  // Step 6: exponentiate to get min-phase spectrum
  // Divide by fftSize to cancel the N factor from the unnormalized BACKWARD in step 3
  const float invN = 1.0f / static_cast<float>(fftSize);

  float re = out[0] * invN;
  out[0] = std::exp(re);  // DC (purely real)
  re = out[1] * invN;
  out[1] = std::exp(re);  // Nyquist (purely real)
  for (int k = 1; k < fftSize / 2; ++k)
  {
    const size_t idx = static_cast<size_t>(k) * 2;
    re = out[idx] * invN;
    float im = out[idx + 1] * invN;
    float expRe = std::exp(re);
    out[idx] = expRe * std::cos(im);
    out[idx + 1] = expRe * std::sin(im);
  }

  // Step 7: IFFT → minimum-phase IR
  // pffft BACKWARD returns N * IDFT; divide by fftSize to normalize
  pffft_transform_ordered(fft, out.data(), in.data(), work.data(), PFFFT_BACKWARD);

  for (int n = 0; n < irLen; ++n)
    samples[n] = in[n] * invN;

  pffft_destroy_setup(fft);
}

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

  const size_t maxAllowedSamples = static_cast<size_t>(MaxIrLengthSeconds) * sampleRate;
  if (irLength > maxAllowedSamples)
  {
    drwav_free(sampleData, nullptr);
    result.success = false;
    result.errorMessage =
        "IR file exceeds maximum length of " + std::to_string(MaxIrLengthSeconds) + " seconds";
    return result;
  }

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

  const float irCompensationGain = std::exp(IrCompensationGainDb * DbToLinearScalar);
  for (auto& sample : irBuffer_)
  {
    sample *= irCompensationGain;
  }

  // Convert to minimum phase so that IR energy starts at t=0.
  // This eliminates comb filtering when blending two IRs with different pre-delays.
  const int mptFftSize = nextPow2(static_cast<int>(irLength) * 2);
  if (channels == 1)
  {
    convertToMinimumPhase(irBuffer_, mptFftSize);
  }
  else
  {
    std::vector<Sample> channel(irLength);
    for (uint32_t ch = 0; ch < channels; ++ch)
    {
      for (size_t i = 0; i < irLength; ++i)
        channel[i] = irBuffer_[i * channels + ch];
      convertToMinimumPhase(channel, mptFftSize);
      for (size_t i = 0; i < irLength; ++i)
        irBuffer_[i * channels + ch] = channel[i];
    }
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

      std::vector<WDL_ResampleSample> resampledIr(outFrames + 64, 0.0);

      // ResamplePrepare's first argument is the desired OUTPUT sample count.
      // It returns how many input samples must be written to rsinbuf.
      WDL_ResampleSample* rsinbuf = nullptr;
      const int needed = resampler.ResamplePrepare(static_cast<int>(outFrames), 1, &rsinbuf);

      for (int i = 0; i < needed; i++)
      {
        if (i < static_cast<int>(numSamples_))
        {
          const int srcCh = (numChannels_ == 1) ? 0 : ch;
          const size_t srcIdx = static_cast<size_t>(i) * static_cast<size_t>(numChannels_) +
                                static_cast<size_t>(srcCh);
          rsinbuf[i] = irBuffer_[srcIdx];
        }
        else
        {
          rsinbuf[i] = 0.0;
        }
      }

      const int actualOutSamples =
          resampler.ResampleOut(resampledIr.data(), needed, static_cast<int>(outFrames), 1);

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
