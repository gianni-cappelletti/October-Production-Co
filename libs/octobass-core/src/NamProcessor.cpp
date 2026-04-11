#include "octobass-core/NamProcessor.hpp"

#include <NAM/container.h>
#include <NAM/convnet.h>
#include <NAM/dsp.h>
#include <NAM/get_dsp.h>
#include <NAM/lstm.h>
#include <NAM/wavenet.h>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <stdexcept>
#include <vector>

namespace
{
// Force the linker to retain every NAM architecture translation unit from the
// octobass-core static archive. Each architecture .cpp defines a file-scope
// ConfigParserHelper whose constructor registers the parser at program
// startup, but the linker only pulls an object file out of a static archive
// when something actually references one of its symbols. Without that
// reference the wavenet/lstm/convnet/etc. object files are stripped, their
// static constructors never run, and get_dsp() throws
// "No config parser registered for architecture: <name>".
//
// We satisfy the linker by reading the address of each create_config function
// through volatile sinks inside a function that is always called on the NAM
// load path. The volatile qualifier makes the reads observable side effects,
// which prevents both the per-TU optimizer and LTO from eliminating them, so
// the relocations against each create_config symbol survive into the final
// link regardless of optimization level.
void forceNamArchitectureLinkage()
{
  using CreateConfigFn = std::unique_ptr<nam::ModelConfig> (*)(const nlohmann::json&, double);

  static CreateConfigFn const kArchitectures[] = {
      &nam::linear::create_config,  &nam::lstm::create_config,      &nam::wavenet::create_config,
      &nam::convnet::create_config, &nam::container::create_config,
  };

  for (std::size_t i = 0; i < sizeof(kArchitectures) / sizeof(kArchitectures[0]); ++i)
  {
    CreateConfigFn volatile sink = kArchitectures[i];
    (void)sink;
  }
}
}  // namespace

namespace octob
{

struct NamProcessor::Impl
{
  std::unique_ptr<nam::DSP> model;
  std::string modelPath;
  double sampleRate = 44100.0;
  int maxBlockSize = 0;
  std::vector<NAM_SAMPLE> inputBuffer;
  std::vector<NAM_SAMPLE> outputBuffer;

  void resetModel()
  {
    if (model && maxBlockSize > 0)
    {
      model->ResetAndPrewarm(sampleRate, maxBlockSize);
    }
  }
};

NamProcessor::NamProcessor() : impl_(std::make_unique<Impl>()) {}

NamProcessor::~NamProcessor() = default;

NamProcessor::NamProcessor(NamProcessor&&) noexcept = default;
NamProcessor& NamProcessor::operator=(NamProcessor&&) noexcept = default;

bool NamProcessor::loadModel(const std::string& filepath, std::string& errorMessage)
{
  forceNamArchitectureLinkage();

  try
  {
    auto model = nam::get_dsp(std::filesystem::path(filepath));
    if (!model)
    {
      errorMessage = "Failed to create NAM model from file: " + filepath;
      return false;
    }

    impl_->model = std::move(model);
    impl_->modelPath = filepath;

    if (impl_->maxBlockSize > 0)
    {
      impl_->resetModel();
    }

    return true;
  }
  catch (const std::exception& e)
  {
    errorMessage = std::string("NAM model load error: ") + e.what();
    return false;
  }
}

void NamProcessor::clearModel()
{
  impl_->model.reset();
  impl_->modelPath.clear();
}

bool NamProcessor::isModelLoaded() const
{
  return impl_->model != nullptr;
}

std::string NamProcessor::getCurrentModelPath() const
{
  return impl_->modelPath;
}

void NamProcessor::setSampleRate(double sampleRate)
{
  impl_->sampleRate = sampleRate;
  impl_->resetModel();
}

void NamProcessor::setMaxBlockSize(size_t maxBlockSize)
{
  impl_->maxBlockSize = static_cast<int>(maxBlockSize);
  impl_->inputBuffer.resize(maxBlockSize);
  impl_->outputBuffer.resize(maxBlockSize);
  impl_->resetModel();
}

void NamProcessor::process(const float* input, float* output, size_t numFrames)
{
  if (!impl_->model || numFrames == 0)
  {
    if (input != output)
    {
      std::copy(input, input + numFrames, output);
    }
    return;
  }

#ifdef NAM_SAMPLE_FLOAT
  // NAM_SAMPLE is float, can use buffers directly with pointer indirection
  NAM_SAMPLE* inPtr = const_cast<NAM_SAMPLE*>(input);
  NAM_SAMPLE* outPtr = output;
  impl_->model->process(&inPtr, &outPtr, static_cast<int>(numFrames));
#else
  // NAM_SAMPLE is double, need conversion buffers
  auto& inBuf = impl_->inputBuffer;
  auto& outBuf = impl_->outputBuffer;

  for (size_t i = 0; i < numFrames; ++i)
    inBuf[i] = static_cast<NAM_SAMPLE>(input[i]);

  NAM_SAMPLE* inPtr = inBuf.data();
  NAM_SAMPLE* outPtr = outBuf.data();
  impl_->model->process(&inPtr, &outPtr, static_cast<int>(numFrames));

  for (size_t i = 0; i < numFrames; ++i)
    output[i] = static_cast<float>(outBuf[i]);
#endif
}

void NamProcessor::reset()
{
  impl_->resetModel();
}

int NamProcessor::getLatencySamples() const
{
  return 0;
}

double NamProcessor::getExpectedSampleRate() const
{
  if (impl_->model)
    return impl_->model->GetExpectedSampleRate();
  return 0.0;
}

}  // namespace octob
