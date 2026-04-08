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
