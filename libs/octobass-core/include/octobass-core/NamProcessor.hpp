#pragma once

#include <memory>
#include <string>

namespace octob
{

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
