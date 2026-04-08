#include "octobass-core/NamProcessor.hpp"

#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <vector>

#include <NAM/convnet.h>
#include <NAM/container.h>
#include <NAM/get_dsp.h>
#include <NAM/lstm.h>
#include <NAM/wavenet.h>

// Force the linker to include NAM architecture object files from the static
// archive. Each .cpp has a static ConfigParserHelper that registers the
// architecture at startup; without an explicit symbol reference from a used
// translation unit the linker strips those objects and get_dsp() fails with
// "No config parser registered".
//
// We use a non-static function with external linkage that takes the address of
// a symbol from each architecture file. The function itself is never called,
// but the compiler must emit it (and its relocations) because it has external
// linkage, which forces the linker to pull in those object files.
void namForceArchitectureRegistrations()
{
  volatile void* p;
  p = reinterpret_cast<void*>(&nam::lstm::create_config);
  p = reinterpret_cast<void*>(&nam::wavenet::create_config);
  p = reinterpret_cast<void*>(&nam::convnet::create_config);
  p = reinterpret_cast<void*>(&nam::container::create_config);
  p = reinterpret_cast<void*>(&nam::linear::create_config);
  (void)p;
}

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

NamProcessor::NamProcessor()
    : impl_(std::make_unique<Impl>())
{
}

NamProcessor::~NamProcessor() = default;

NamProcessor::NamProcessor(NamProcessor&&) noexcept = default;
NamProcessor& NamProcessor::operator=(NamProcessor&&) noexcept = default;

bool NamProcessor::loadModel(const std::string& filepath, std::string& errorMessage)
{
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
