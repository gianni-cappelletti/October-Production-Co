#include "octobass-core/NamProcessor.hpp"

#include <NAM/container.h>
#include <NAM/convnet.h>
#include <NAM/dsp.h>
#include <NAM/get_dsp.h>
#include <NAM/lstm.h>
#include <NAM/slimmable.h>
#include <NAM/wavenet/model.h>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <json.hpp>
#include <mutex>
#include <set>
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

// NAM core does not expose the granularity of SetSlimmableSize through the
// SlimmableModel interface, so the level count is recovered from the model
// file's JSON config using the same rules the architectures apply:
//  - SlimmableContainer activates one of its submodels, so the level count is
//    the submodel count (container.cpp, SetSlimmableSize).
//  - A slimmable WaveNet maps quality to floor(quality * N) over each layer
//    array's N allowed channel counts (wavenet/slimmable.cpp,
//    ratio_to_channels), so the levels are the union of the i/N breakpoints
//    across arrays, plus one.
int countSlimmableWavenetLevels(const nlohmann::json& config)
{
  // IEEE-754 division is correctly rounded, so equal fractions from different
  // arrays (1/2 and 2/4) collapse to the same breakpoint
  std::set<double> breakpoints;

  if (!config.contains("layers") || !config["layers"].is_array())
    return 1;

  for (const auto& layer : config["layers"])
  {
    if (!layer.contains("slimmable") || !layer["slimmable"].is_object())
      continue;

    const auto& slim = layer["slimmable"];
    std::size_t numAllowed = 0;
    if (slim.contains("kwargs") && slim["kwargs"].contains("allowed_channels"))
      numAllowed = slim["kwargs"]["allowed_channels"].size();
    else if (layer.contains("channels"))
      // SlimmableWavenetConfig::create assumes [1..channels] when the
      // allowed_channels list is missing
      numAllowed = layer["channels"].get<std::size_t>();

    for (std::size_t i = 1; i < numAllowed; ++i)
      breakpoints.insert(static_cast<double>(i) / static_cast<double>(numAllowed));
  }

  return static_cast<int>(breakpoints.size()) + 1;
}

int countQualityLevels(const std::string& filepath)
{
  try
  {
    std::ifstream stream(filepath);
    const auto json = nlohmann::json::parse(stream);
    const auto architecture = json.value("architecture", std::string{});

    if (architecture == "SlimmableContainer")
      return static_cast<int>(json.at("config").at("submodels").size());
    if (architecture == "WaveNet")
      return countSlimmableWavenetLevels(json.at("config"));
    return 1;
  }
  catch (const std::exception&)
  {
    // The model itself loaded, so it has at least the slim/full distinction
    // that made it a SlimmableModel
    return 2;
  }
}
}  // namespace

namespace octob
{

struct NamProcessor::Impl
{
  // Active model, only touched by the audio thread (after initial setup)
  std::unique_ptr<nam::DSP> model;
  std::string modelPath;
  double sampleRate = 44100.0;
  int maxBlockSize = 0;
  std::vector<NAM_SAMPLE> inputBuffer;
  std::vector<NAM_SAMPLE> outputBuffer;

  // Thread-safe model swap: message thread stages a fully-prepared model here,
  // audio thread picks it up at the start of the next process() call. The
  // mutex guards the unique_ptr handoff itself (pendingModel, the paths, and
  // the move into model); the atomics let the audio thread skip the lock
  // entirely when nothing is staged.
  std::mutex swapMutex;
  std::unique_ptr<nam::DSP> pendingModel;
  std::string pendingModelPath;
  std::atomic<bool> hasPendingModel{false};
  std::atomic<bool> pendingClear{false};

  double quality = 1.0;
  // Message-thread only, like the load/clear calls that update it
  int numQualityLevels = 0;

  void applyQuality(nam::DSP* target) const
  {
    if (auto* slimmable = dynamic_cast<nam::SlimmableModel*>(target))
      slimmable->SetSlimmableSize(quality);
  }

  void resetModel()
  {
    if (model && maxBlockSize > 0)
    {
      model->ResetAndPrewarm(sampleRate, maxBlockSize);
    }
  }

  void consumePending()
  {
    if (!pendingClear.load(std::memory_order_acquire) &&
        !hasPendingModel.load(std::memory_order_acquire))
      return;

    // try_lock keeps the audio thread non-blocking: if the message thread is
    // mid-stage, the swap is simply picked up on the next block
    std::unique_lock<std::mutex> lock(swapMutex, std::try_to_lock);
    if (!lock.owns_lock())
      return;

    if (pendingClear.load(std::memory_order_acquire))
    {
      model.reset();
      modelPath.clear();
      pendingClear.store(false, std::memory_order_release);
    }
    if (hasPendingModel.load(std::memory_order_acquire))
    {
      model = std::move(pendingModel);
      modelPath = std::move(pendingModelPath);
      hasPendingModel.store(false, std::memory_order_release);
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
    auto newModel = nam::get_dsp(std::filesystem::path(filepath));
    if (!newModel)
    {
      // Filename only: the full path may end up in user-facing dialogs
      errorMessage = "Failed to create NAM model from file: " +
                     std::filesystem::path(filepath).filename().string();
      return false;
    }

    const bool isSlimmable = dynamic_cast<nam::SlimmableModel*>(newModel.get()) != nullptr;
    impl_->numQualityLevels = isSlimmable ? countQualityLevels(filepath) : 1;

    impl_->applyQuality(newModel.get());

    if (impl_->maxBlockSize > 0)
    {
      newModel->ResetAndPrewarm(impl_->sampleRate, impl_->maxBlockSize);
    }

    {
      const std::lock_guard<std::mutex> lock(impl_->swapMutex);
      impl_->pendingModel = std::move(newModel);
      impl_->pendingModelPath = filepath;
      impl_->hasPendingModel.store(true, std::memory_order_release);
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
  const std::lock_guard<std::mutex> lock(impl_->swapMutex);
  impl_->hasPendingModel.store(false, std::memory_order_release);
  impl_->pendingModel.reset();
  impl_->pendingModelPath.clear();
  impl_->pendingClear.store(true, std::memory_order_release);
  impl_->numQualityLevels = 0;
}

bool NamProcessor::isModelLoaded() const
{
  const std::lock_guard<std::mutex> lock(impl_->swapMutex);
  if (impl_->pendingClear.load(std::memory_order_acquire))
    return false;
  if (impl_->hasPendingModel.load(std::memory_order_acquire))
    return true;
  return impl_->model != nullptr;
}

std::string NamProcessor::getCurrentModelPath() const
{
  const std::lock_guard<std::mutex> lock(impl_->swapMutex);
  if (impl_->pendingClear.load(std::memory_order_acquire))
    return {};
  if (impl_->hasPendingModel.load(std::memory_order_acquire))
    return impl_->pendingModelPath;
  return impl_->modelPath;
}

void NamProcessor::setQuality(double quality)
{
  quality = std::max(0.0, std::min(1.0, quality));

  if (impl_->quality == quality)
    return;

  impl_->quality = quality;

  // Apply to the staged model if one is waiting, otherwise the active one.
  // The lock pins both unique_ptrs against a concurrent audio-thread swap;
  // SetSlimmableSize is internally synchronized against concurrent process().
  const std::lock_guard<std::mutex> lock(impl_->swapMutex);
  if (impl_->hasPendingModel.load(std::memory_order_acquire))
    impl_->applyQuality(impl_->pendingModel.get());
  else
    impl_->applyQuality(impl_->model.get());
}

double NamProcessor::getQuality() const
{
  return impl_->quality;
}

int NamProcessor::getNumQualityLevels() const
{
  return impl_->numQualityLevels;
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
  impl_->consumePending();

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
  const std::lock_guard<std::mutex> lock(impl_->swapMutex);
  if (impl_->hasPendingModel.load(std::memory_order_acquire) && impl_->pendingModel)
    return impl_->pendingModel->GetExpectedSampleRate();
  if (impl_->model)
    return impl_->model->GetExpectedSampleRate();
  return 0.0;
}

}  // namespace octob
