#pragma once

#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>

#include <array>
#include <cmath>

class SpectrumAnalyzer
{
 public:
  static constexpr int kFFTOrder = 12;
  static constexpr int kFFTSize = 1 << kFFTOrder;  // 4096
  static constexpr int kNumBands = 24;
  static constexpr float kMinDb = -80.0f;
  static constexpr float kMaxDb = 0.0f;

  // Display bands: <50Hz combined, 22 individual 1/3-octave 50Hz-6.3kHz, >6.3kHz combined.
  // Edge frequencies computed via geometric mean between adjacent ISO centers for contiguity.
  struct BandRange
  {
    float lowHz;
    float highHz;
  };

  static constexpr std::array<BandRange, kNumBands> kBandRanges = {{
      {17.82f, 44.72f},      // <50 Hz (combined: 20, 25, 31.5, 40 Hz)
      {44.72f, 56.12f},      // 50 Hz
      {56.12f, 71.00f},      // 63 Hz
      {71.00f, 89.44f},      // 80 Hz
      {89.44f, 111.80f},     // 100 Hz
      {111.80f, 141.42f},    // 125 Hz
      {141.42f, 178.89f},    // 160 Hz
      {178.89f, 223.61f},    // 200 Hz
      {223.61f, 280.62f},    // 250 Hz
      {280.62f, 354.96f},    // 315 Hz
      {354.96f, 447.21f},    // 400 Hz
      {447.21f, 561.25f},    // 500 Hz
      {561.25f, 710.00f},    // 630 Hz
      {710.00f, 894.43f},    // 800 Hz
      {894.43f, 1118.03f},   // 1 kHz
      {1118.03f, 1414.21f},  // 1.25 kHz
      {1414.21f, 1788.85f},  // 1.6 kHz
      {1788.85f, 2236.07f},  // 2 kHz
      {2236.07f, 2806.24f},  // 2.5 kHz
      {2806.24f, 3549.65f},  // 3.15 kHz
      {3549.65f, 4472.14f},  // 4 kHz
      {4472.14f, 5612.49f},  // 5 kHz
      {5612.49f, 7099.30f},  // 6.3 kHz
      {7099.30f, 22449.0f},  // >6.3 kHz (combined: 8k, 10k, 12.5k, 16k, 20k)
  }};

  SpectrumAnalyzer()
  {
    sampleBuffer_.fill(0.0f);
    fftData_.fill(0.0f);
    smoothedLevels_.fill(kMinDb);
  }

  void setSampleRate(double sampleRate) { sampleRate_ = sampleRate; }

  void processFromFifo(juce::AbstractFifo& fifo, const float* fifoBuffer)
  {
    const int numReady = fifo.getNumReady();
    if (numReady == 0)
      return;

    const auto scope = fifo.read(numReady);

    if (scope.blockSize1 > 0)
    {
      for (int i = 0; i < scope.blockSize1; ++i)
      {
        sampleBuffer_[static_cast<size_t>(writePos_)] = fifoBuffer[scope.startIndex1 + i];
        writePos_ = (writePos_ + 1) % kFFTSize;
      }
    }

    if (scope.blockSize2 > 0)
    {
      for (int i = 0; i < scope.blockSize2; ++i)
      {
        sampleBuffer_[static_cast<size_t>(writePos_)] = fifoBuffer[scope.startIndex2 + i];
        writePos_ = (writePos_ + 1) % kFFTSize;
      }
    }

    performAnalysis();
  }

  const std::array<float, kNumBands>& getBandLevels() const { return smoothedLevels_; }

 private:
  void performAnalysis()
  {
    // Linearize circular buffer into FFT input (oldest sample first)
    for (int i = 0; i < kFFTSize; ++i)
      fftData_[static_cast<size_t>(i)] =
          sampleBuffer_[static_cast<size_t>((writePos_ + i) % kFFTSize)];

    std::fill(fftData_.begin() + kFFTSize, fftData_.end(), 0.0f);

    window_.multiplyWithWindowingTable(fftData_.data(), static_cast<size_t>(kFFTSize));
    fft_.performFrequencyOnlyForwardTransform(fftData_.data(), true);

    // fftData_[0..kFFTSize/2] now contains magnitude values
    const auto binWidth = static_cast<float>(sampleRate_) / static_cast<float>(kFFTSize);

    for (int b = 0; b < kNumBands; ++b)
    {
      const auto& range = kBandRanges[static_cast<size_t>(b)];

      int lowBin = std::max(1, static_cast<int>(std::ceil(range.lowHz / binWidth)));
      int highBin = std::min(kFFTSize / 2, static_cast<int>(std::floor(range.highHz / binWidth)));

      // At high sample rates, lowest bands may have zero bins -- use nearest
      if (lowBin > highBin)
      {
        float centerHz = (range.lowHz + range.highHz) * 0.5f;
        int nearest =
            juce::jlimit(1, kFFTSize / 2, static_cast<int>(std::round(centerHz / binWidth)));
        lowBin = nearest;
        highBin = nearest;
      }

      // Average power (RMS) across bins in this band
      float power = 0.0f;
      int count = 0;
      for (int bin = lowBin; bin <= highBin; ++bin)
      {
        float mag = fftData_[static_cast<size_t>(bin)];
        power += mag * mag;
        ++count;
      }

      float rms = (count > 0) ? std::sqrt(power / static_cast<float>(count)) : 0.0f;
      rms *= kNormFactor;

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

  std::array<float, kFFTSize> sampleBuffer_{};
  int writePos_ = 0;

  std::array<float, kFFTSize * 2> fftData_{};
  std::array<float, kNumBands> smoothedLevels_{};

  double sampleRate_ = 44100.0;

  static constexpr float kNormFactor = 2.0f / static_cast<float>(kFFTSize);
  static constexpr float kAttackAlpha = 0.7f;
  static constexpr float kDecayAlpha = 0.05f;

  SpectrumAnalyzer(const SpectrumAnalyzer&) = delete;
  SpectrumAnalyzer& operator=(const SpectrumAnalyzer&) = delete;
};
