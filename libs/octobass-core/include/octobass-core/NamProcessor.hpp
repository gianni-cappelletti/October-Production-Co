#pragma once

#include <bit>
#include <cstdint>
#include <memory>
#include <string>

namespace octob
{

// Post-NAM output leveling, mirroring the NAM Gateway plugin's output modes
enum class NamOutputMode : int
{
  Raw = 0,
  Normalized = 1,
  Calibrated = 2,
};

// Level metadata carried by a .nam file. Levels are in dBu (RMS of a 1 kHz
// sine peaking at 0 dBFS); loudness is in dB relative to the NAM convention.
struct NamModelMetadata
{
  bool hasInputLevel = false;
  double inputLevelDbu = 0.0;
  bool hasOutputLevel = false;
  double outputLevelDbu = 0.0;
  bool hasLoudness = false;
  double loudnessDb = 0.0;

  // Values are copied verbatim from the model file (never computed), so
  // bitwise equality is the right change-detection; it also avoids the
  // float-equality warning a defaulted comparison would emit
  bool operator==(const NamModelMetadata& other) const
  {
    const auto sameBits = [](double a, double b)
    { return std::bit_cast<std::uint64_t>(a) == std::bit_cast<std::uint64_t>(b); };
    return hasInputLevel == other.hasInputLevel && sameBits(inputLevelDbu, other.inputLevelDbu) &&
           hasOutputLevel == other.hasOutputLevel &&
           sameBits(outputLevelDbu, other.outputLevelDbu) && hasLoudness == other.hasLoudness &&
           sameBits(loudnessDb, other.loudnessDb);
  }
};

class NamProcessor
{
 public:
  NamProcessor();
  ~NamProcessor();

  NamProcessor(const NamProcessor&) = delete;
  NamProcessor& operator=(const NamProcessor&) = delete;
  NamProcessor(NamProcessor&&) noexcept;
  NamProcessor& operator=(NamProcessor&&) noexcept;

  bool loadModel(const std::string& filepath, std::string& errorMessage);
  void clearModel();
  bool isModelLoaded() const;
  std::string getCurrentModelPath() const;

  // Quality/CPU trade-off for slimmable models: 0.0 = slimmest (lowest CPU),
  // 1.0 = full quality. No-op for models that do not support slimming.
  // Persists across model loads. Call from the message thread only --
  // SetSlimmableSize is thread-safe against process() but not real-time safe.
  void setQuality(double quality);
  double getQuality() const;

  // Number of discrete quality levels the loaded model exposes:
  //   0 = no model loaded
  //   1 = model loaded, no quality options (setQuality is a no-op)
  //  >1 = number of distinct configurations setQuality can select
  // Call from the message thread only.
  int getNumQualityLevels() const;

  // Level metadata of the loaded (or staged) model; all-false defaults when
  // no model is loaded. Call from the message thread only.
  NamModelMetadata getModelMetadata() const;

  // Calibration controls. Real-time safe, callable per audio block.
  // When enabled and the model reports an input level, the input is trimmed
  // by (level - model input level) dB before the model, mirroring the NAM
  // Gateway plugin. The output mode applies the corresponding post-model
  // gain; modes whose metadata is missing fall back to no adjustment.
  void setCalibrateInput(bool enabled);
  void setInputCalibrationLevel(float levelDbu);
  void setOutputMode(NamOutputMode mode);

  void setSampleRate(double sampleRate);
  void setMaxBlockSize(size_t maxBlockSize);

  // Processes audio. When no model is loaded, copies input to output.
  void process(const float* input, float* output, size_t numFrames);

  void reset();

  int getLatencySamples() const;
  double getExpectedSampleRate() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace octob
