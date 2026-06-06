#pragma once

#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>

#include <array>
#include <cmath>
#include <vector>

// Two-path multi-resolution analyzer: a full-rate FFT covers 200 Hz - 20 kHz,
// and a decimated path with a second FFT covers 20 - 200 Hz. Frequency
// resolution is set by window duration, so the low bands need a longer window
// than the full-rate FFT provides at high sample rates; decimating to ~6 kHz
// buys that resolution without a giant FFT. The cost is a ~340 ms low-band
// window, so the bottom bars respond more slowly by design.
class SpectrumAnalyzer
{
 public:
  static constexpr int kFFTOrder = 12;
  static constexpr int kFFTSize = 1 << kFFTOrder;  // 4096
  static constexpr int kLFFFTOrder = 11;
  static constexpr int kLFFFTSize = 1 << kLFFFTOrder;  // 2048
  static constexpr int kNumBands = 24;
  static constexpr int kNumLFBands = 8;  // bands below kLFCrossoverHz use the decimated path
  static constexpr float kLFCrossoverHz = 200.0f;
  static constexpr float kMinDb = -100.0f;
  static constexpr float kMaxDb = 0.0f;

  // Display bands: 24 log-spaced bands covering 20 Hz - 20 kHz (3 decades, so
  // 8 bands per decade; edge i = 20 * 10^(i/8)). Bar boundaries line up exactly
  // with the EQ display's log frequency axis so the spectrum reads true under
  // the EQ curve.
  struct BandRange
  {
    float lowHz;
    float highHz;
  };

  static constexpr std::array<BandRange, kNumBands> kBandRanges = {{
      {20.00f, 26.67f},        // 23 Hz center
      {26.67f, 35.57f},        // 31 Hz
      {35.57f, 47.43f},        // 41 Hz
      {47.43f, 63.25f},        // 55 Hz
      {63.25f, 84.34f},        // 73 Hz
      {84.34f, 112.47f},       // 97 Hz
      {112.47f, 149.98f},      // 130 Hz
      {149.98f, 200.00f},      // 173 Hz
      {200.00f, 266.70f},      // 231 Hz
      {266.70f, 355.66f},      // 308 Hz
      {355.66f, 474.27f},      // 411 Hz
      {474.27f, 632.46f},      // 548 Hz
      {632.46f, 843.39f},      // 730 Hz
      {843.39f, 1124.68f},     // 974 Hz
      {1124.68f, 1499.79f},    // 1.3 kHz
      {1499.79f, 2000.00f},    // 1.73 kHz
      {2000.00f, 2667.04f},    // 2.31 kHz
      {2667.04f, 3556.56f},    // 3.08 kHz
      {3556.56f, 4742.75f},    // 4.11 kHz
      {4742.75f, 6324.56f},    // 5.48 kHz
      {6324.56f, 8433.93f},    // 7.3 kHz
      {8433.93f, 11246.83f},   // 9.74 kHz
      {11246.83f, 14997.88f},  // 12.99 kHz
      {14997.88f, 20000.00f},  // 17.32 kHz
  }};

  static_assert(kBandRanges[kNumLFBands - 1].highHz > kLFCrossoverHz - 0.01f &&
                    kBandRanges[kNumLFBands - 1].highHz < kLFCrossoverHz + 0.01f,
                "The path split must fall exactly on a band edge");

  SpectrumAnalyzer()
  {
    sampleBuffer_.fill(0.0f);
    fftData_.fill(0.0f);
    lfSampleBuffer_.fill(0.0f);
    lfFFTData_.fill(0.0f);
    smoothedLevels_.fill(kMinDb);
    prepareLFPath();
  }

  void setSampleRate(double sampleRate)
  {
    if (sampleRate <= 0.0)
    {
      DBG("Ignoring invalid analyzer sample rate " + juce::String(sampleRate));
      return;
    }
    if (std::abs(sampleRate - sampleRate_) < 1.0)
      return;

    sampleRate_ = sampleRate;
    prepareLFPath();
  }

  double getLFSampleRate() const { return lfSampleRate_; }

  void processFromFifo(juce::AbstractFifo& fifo, const float* fifoBuffer)
  {
    const int numReady = fifo.getNumReady();
    if (numReady == 0)
      return;

    const auto scope = fifo.read(numReady);

    for (int i = 0; i < scope.blockSize1; ++i)
      pushSample(fifoBuffer[scope.startIndex1 + i]);

    for (int i = 0; i < scope.blockSize2; ++i)
      pushSample(fifoBuffer[scope.startIndex2 + i]);

    performAnalysis();
  }

  const std::array<float, kNumBands>& getBandLevels() const { return smoothedLevels_; }

 private:
  // One decimate-by-2 stage: halfband FIR on the kept samples only. All stages
  // share the same normalized coefficient set.
  class HalfbandDecimatorStage
  {
   public:
    void prepare(const std::vector<float>& taps)
    {
      taps_ = &taps;
      delay_.assign(taps.size(), 0.0f);
      reset();
    }

    void reset()
    {
      std::fill(delay_.begin(), delay_.end(), 0.0f);
      writePos_ = 0;
      keepNext_ = true;
    }

    // Returns true and fills `out` when this input sample produces an output
    bool push(float in, float& out)
    {
      delay_[static_cast<size_t>(writePos_)] = in;

      bool produce = keepNext_;
      keepNext_ = !keepNext_;
      if (produce)
      {
        float acc = 0.0f;
        int idx = writePos_;
        int size = static_cast<int>(delay_.size());
        for (float tap : *taps_)
        {
          acc += tap * delay_[static_cast<size_t>(idx)];
          idx = (idx == 0) ? size - 1 : idx - 1;
        }
        out = acc;
      }

      writePos_ = (writePos_ + 1) % static_cast<int>(delay_.size());
      return produce;
    }

   private:
    const std::vector<float>* taps_ = nullptr;
    std::vector<float> delay_;
    int writePos_ = 0;
    bool keepNext_ = true;
  };

  void prepareLFPath()
  {
    int numStages =
        std::max(1, static_cast<int>(std::round(std::log2(sampleRate_ / kTargetLFRateHz))));
    lfSampleRate_ = sampleRate_ / std::exp2(numStages);

    if (decimatorTaps_.empty())
    {
      auto coeffs = juce::dsp::FilterDesign<float>::designFIRLowpassHalfBandEquirippleMethod(
          kHalfbandTransitionWidth, -kAliasAttenuationDb);
      decimatorTaps_.assign(coeffs->getRawCoefficients(),
                            coeffs->getRawCoefficients() + coeffs->getFilterOrder() + 1);
      DBG("Spectrum analyzer halfband decimator: " + juce::String(decimatorTaps_.size()) + " taps");
    }

    decimatorStages_.resize(static_cast<size_t>(numStages));
    for (auto& stage : decimatorStages_)
      stage.prepare(decimatorTaps_);

    sampleBuffer_.fill(0.0f);
    writePos_ = 0;
    lfSampleBuffer_.fill(0.0f);
    lfWritePos_ = 0;
    smoothedLevels_.fill(kMinDb);

    DBG("Spectrum analyzer LF path: " + juce::String(numStages) + " stages, decimated rate " +
        juce::String(lfSampleRate_, 1) + " Hz");
  }

  void pushSample(float sample)
  {
    sampleBuffer_[static_cast<size_t>(writePos_)] = sample;
    writePos_ = (writePos_ + 1) % kFFTSize;

    for (auto& stage : decimatorStages_)
    {
      if (!stage.push(sample, sample))
        return;
    }

    lfSampleBuffer_[static_cast<size_t>(lfWritePos_)] = sample;
    lfWritePos_ = (lfWritePos_ + 1) % kLFFFTSize;
  }

  // Linearize a circular buffer (oldest sample first), window, and transform;
  // dest[0..size/2] receives the magnitudes
  template <size_t Size>
  static void windowAndTransform(const std::array<float, Size>& ring, int ringWritePos,
                                 juce::dsp::WindowingFunction<float>& window, juce::dsp::FFT& fft,
                                 std::array<float, Size * 2>& dest)
  {
    for (size_t i = 0; i < Size; ++i)
      dest[i] = ring[(static_cast<size_t>(ringWritePos) + i) % Size];

    std::fill(dest.begin() + Size, dest.end(), 0.0f);

    window.multiplyWithWindowingTable(dest.data(), Size);
    fft.performFrequencyOnlyForwardTransform(dest.data(), true);
  }

  void performAnalysis()
  {
    windowAndTransform(sampleBuffer_, writePos_, window_, fft_, fftData_);
    windowAndTransform(lfSampleBuffer_, lfWritePos_, lfWindow_, lfFFT_, lfFFTData_);

    const auto binWidth = static_cast<float>(sampleRate_) / static_cast<float>(kFFTSize);
    const auto lfBinWidth = static_cast<float>(lfSampleRate_) / static_cast<float>(kLFFFTSize);

    computeBands(lfFFTData_.data(), kLFFFTSize / 2, lfBinWidth, kLFNormFactor, 0, kNumLFBands - 1);
    computeBands(fftData_.data(), kFFTSize / 2, binWidth, kNormFactor, kNumLFBands, kNumBands - 1);
  }

  // Band level = power spectral density over the band's bins, scaled by a fixed
  // reference bandwidth. PSD is bin-width invariant, so readings stay consistent
  // across sample rates and analysis paths; the reference bandwidth (the 44.1 kHz
  // bin width) keeps absolute levels where the pre-PSD display had them.
  void computeBands(const float* magnitudes, int maxBin, float binWidthHz, float normFactor,
                    int firstBand, int lastBand)
  {
    for (int b = firstBand; b <= lastBand; ++b)
    {
      const auto& range = kBandRanges[static_cast<size_t>(b)];

      int lowBin = std::max(1, static_cast<int>(std::ceil(range.lowHz / binWidthHz)));
      int highBin = std::min(maxBin, static_cast<int>(std::floor(range.highHz / binWidthHz)));

      // Safety net for bands narrower than one bin -- use the nearest bin
      if (lowBin > highBin)
      {
        float centerHz = (range.lowHz + range.highHz) * 0.5f;
        int nearest = juce::jlimit(1, maxBin, static_cast<int>(std::round(centerHz / binWidthHz)));
        lowBin = nearest;
        highBin = nearest;
      }

      float power = 0.0f;
      int count = 0;
      for (int bin = lowBin; bin <= highBin; ++bin)
      {
        float mag = magnitudes[bin];
        power += mag * mag;
        ++count;
      }

      float avgPower = (count > 0) ? power / static_cast<float>(count) : 0.0f;
      float rms = std::sqrt(avgPower * (kRefBandwidthHz / binWidthHz)) * normFactor;

      float db = (rms > 1e-10f) ? 20.0f * std::log10(rms) : kMinDb;
      db = std::max(kMinDb, std::min(kMaxDb, db));

      // Asymmetric exponential smoothing: fast attack, slow decay
      float alpha = (db > smoothedLevels_[static_cast<size_t>(b)]) ? kAttackAlpha : kDecayAlpha;
      smoothedLevels_[static_cast<size_t>(b)] +=
          alpha * (db - smoothedLevels_[static_cast<size_t>(b)]);
      smoothedLevels_[static_cast<size_t>(b)] =
          std::max(kMinDb, smoothedLevels_[static_cast<size_t>(b)]);
    }
  }

  juce::dsp::FFT fft_{kFFTOrder};
  juce::dsp::WindowingFunction<float> window_{static_cast<size_t>(kFFTSize),
                                              juce::dsp::WindowingFunction<float>::hann, true};
  juce::dsp::FFT lfFFT_{kLFFFTOrder};
  juce::dsp::WindowingFunction<float> lfWindow_{static_cast<size_t>(kLFFFTSize),
                                                juce::dsp::WindowingFunction<float>::hann, true};

  std::array<float, kFFTSize> sampleBuffer_{};
  int writePos_ = 0;
  std::array<float, kLFFFTSize> lfSampleBuffer_{};
  int lfWritePos_ = 0;

  std::array<float, kFFTSize * 2> fftData_{};
  std::array<float, kLFFFTSize * 2> lfFFTData_{};
  std::array<float, kNumBands> smoothedLevels_{};

  std::vector<float> decimatorTaps_;
  std::vector<HalfbandDecimatorStage> decimatorStages_;

  double sampleRate_ = 44100.0;
  double lfSampleRate_ = 0.0;

  static constexpr float kNormFactor = 2.0f / static_cast<float>(kFFTSize);
  static constexpr float kLFNormFactor = 2.0f / static_cast<float>(kLFFFTSize);
  static constexpr float kRefBandwidthHz = 44100.0f / static_cast<float>(kFFTSize);
  static constexpr float kAttackAlpha = 0.7f;
  static constexpr float kDecayAlpha = 0.05f;

  // Decimated rate target: D = 2^round(log2(fs / 6000)), giving ~5.5-6 kHz at
  // all common host rates and >= 2 LF bins for the narrowest band (6.7 Hz)
  static constexpr double kTargetLFRateHz = 6000.0;
  static constexpr float kHalfbandTransitionWidth = 0.08f;
  static constexpr float kAliasAttenuationDb = 85.0f;

  SpectrumAnalyzer(const SpectrumAnalyzer&) = delete;
  SpectrumAnalyzer& operator=(const SpectrumAnalyzer&) = delete;
};
