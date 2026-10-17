#include "FakeLv2Host.hpp"

#include <lv2/atom/forge.h>
#include <lv2/atom/util.h>
#include <lv2/buf-size/buf-size.h>
#include <lv2/patch/patch.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

#include "AllocationTracker.hpp"
#include "PortTable.hpp"

namespace octob::lv2::test
{

using octob::test::ScopedAllocationTracking;
using octob::test::ScopedAllocationTrackingPause;

namespace
{

constexpr size_t AtomBufferBytes = 64 * 1024;

char* duplicateString(const std::string& text)
{
  char* copy = static_cast<char*>(std::malloc(text.size() + 1));
  std::memcpy(copy, text.c_str(), text.size() + 1);
  return copy;
}

}  // namespace

FakeLv2Host::FakeLv2Host(HostConfig config)
    : config_(std::move(config)), controlBuffer_(AtomBufferBytes), notifyBuffer_(AtomBufferBytes)
{
  uridMap_ = {this, &FakeLv2Host::mapCallback};
  uridUnmap_ = {this, &FakeLv2Host::unmapCallback};
  workerSchedule_ = {this, &FakeLv2Host::scheduleWorkCallback};
  log_ = {this, &FakeLv2Host::logPrintfCallback, &FakeLv2Host::logVPrintfCallback};
  mapPath_ = {this, &FakeLv2Host::abstractPathCallback, &FakeLv2Host::absolutePathCallback};
  freePath_ = {this, &FakeLv2Host::freePathCallback};

  blockLengths_ = {static_cast<int32_t>(config_.maxBlockLength),
                   static_cast<int32_t>(config_.maxBlockLength)};
  const LV2_URID atomInt = map(LV2_ATOM__Int);
  options_ = {
      {LV2_OPTIONS_INSTANCE, 0, map(LV2_BUF_SIZE__maxBlockLength), sizeof(int32_t), atomInt,
       &blockLengths_[0]},
      {LV2_OPTIONS_INSTANCE, 0, map(LV2_BUF_SIZE__nominalBlockLength), sizeof(int32_t), atomInt,
       &blockLengths_[1]},
      {LV2_OPTIONS_INSTANCE, 0, 0, 0, 0, nullptr},
  };

  if (config_.provideUridMap)
  {
    featureStorage_.push_back({LV2_URID__map, &uridMap_});
    featureStorage_.push_back({LV2_URID__unmap, &uridUnmap_});
  }
  if (config_.provideWorker)
    featureStorage_.push_back({LV2_WORKER__schedule, &workerSchedule_});
  if (config_.provideLog)
    featureStorage_.push_back({LV2_LOG__log, &log_});
  if (config_.provideOptions)
  {
    featureStorage_.push_back({LV2_OPTIONS__options, options_.data()});
    featureStorage_.push_back({LV2_BUF_SIZE__boundedBlockLength, nullptr});
  }
  for (const auto& feature : featureStorage_)
    features_.push_back(&feature);
  features_.push_back(nullptr);

  for (const auto& spec : portTable())
    controls_[spec.index] = spec.defaultValue;

  descriptor_ = lv2_descriptor(0);
  if (descriptor_ == nullptr)
    throw std::runtime_error("Plugin exposes no LV2 descriptor");

  instance_ = descriptor_->instantiate(descriptor_, config_.sampleRate, LV2_BUNDLE_SOURCE_DIR,
                                       features_.data());
  if (instance_ == nullptr)
    return;

  if (descriptor_->extension_data != nullptr)
  {
    workerInterface_ = static_cast<const LV2_Worker_Interface*>(
        descriptor_->extension_data(LV2_WORKER__interface));
    stateInterface_ =
        static_cast<const LV2_State_Interface*>(descriptor_->extension_data(LV2_STATE__interface));
  }

  if (descriptor_->activate != nullptr)
    descriptor_->activate(instance_);
}

FakeLv2Host::~FakeLv2Host()
{
  if (instance_ == nullptr)
    return;
  if (descriptor_->deactivate != nullptr)
    descriptor_->deactivate(instance_);
  descriptor_->cleanup(instance_);
}

void FakeLv2Host::setControl(PortIndex index, float value)
{
  controls_.at(index) = value;
}

float FakeLv2Host::control(PortIndex index) const
{
  return controls_.at(index);
}

std::vector<float> FakeLv2Host::process(const std::vector<float>& input, size_t blockSize)
{
  std::vector<float> output(input.size());
  inputBuffer_.resize(blockSize);
  outputBuffer_.resize(blockSize);

  for (size_t offset = 0; offset < input.size(); offset += blockSize)
  {
    const size_t frames = std::min(blockSize, input.size() - offset);
    std::copy_n(input.data() + offset, frames, inputBuffer_.data());
    runBlock(inputBuffer_.data(), outputBuffer_.data(), static_cast<uint32_t>(frames));
    std::copy_n(outputBuffer_.data(), frames, output.data() + offset);
  }
  return output;
}

void FakeLv2Host::runBlock(const float* input, float* output, uint32_t frames)
{
  descriptor_->connect_port(instance_, port::AudioIn, const_cast<float*>(input));
  descriptor_->connect_port(instance_, port::AudioOut, output);
  descriptor_->connect_port(instance_, port::Control, controlBuffer_.data());
  descriptor_->connect_port(instance_, port::Notify, notifyBuffer_.data());
  for (const auto& spec : portTable())
    if (spec.kind == PortKind::ControlInput || spec.kind == PortKind::ControlOutput)
      descriptor_->connect_port(instance_, spec.index, &controls_[spec.index]);

  writeControlSequence();

  auto* notify = reinterpret_cast<LV2_Atom*>(notifyBuffer_.data());
  notify->size = static_cast<uint32_t>(notifyBuffer_.size() - sizeof(LV2_Atom));
  notify->type = map(LV2_ATOM__Chunk);

  if (trackRunAllocations_)
  {
    const ScopedAllocationTracking tracking;
    descriptor_->run(instance_, frames);
  }
  else
  {
    descriptor_->run(instance_, frames);
  }

  readNotifySequence();
  pendingMessages_.clear();
}

void FakeLv2Host::writeControlSequence()
{
  LV2_Atom_Forge forge;
  lv2_atom_forge_init(&forge, &uridMap_);
  lv2_atom_forge_set_buffer(&forge, controlBuffer_.data(), controlBuffer_.size());

  LV2_Atom_Forge_Frame sequenceFrame;
  lv2_atom_forge_sequence_head(&forge, &sequenceFrame, 0);
  for (const auto& message : pendingMessages_)
  {
    lv2_atom_forge_frame_time(&forge, 0);
    LV2_Atom_Forge_Frame objectFrame;
    if (message.isGet)
    {
      lv2_atom_forge_object(&forge, &objectFrame, 0, map(LV2_PATCH__Get));
    }
    else
    {
      lv2_atom_forge_object(&forge, &objectFrame, 0, map(LV2_PATCH__Set));
      lv2_atom_forge_key(&forge, map(LV2_PATCH__property));
      lv2_atom_forge_urid(&forge, map(fileParameter(message.slot).uri));
      lv2_atom_forge_key(&forge, map(LV2_PATCH__value));
      lv2_atom_forge_path(&forge, message.path.c_str(), static_cast<uint32_t>(message.path.size()));
    }
    lv2_atom_forge_pop(&forge, &objectFrame);
  }
  lv2_atom_forge_pop(&forge, &sequenceFrame);
}

void FakeLv2Host::readNotifySequence()
{
  const auto* sequence = reinterpret_cast<const LV2_Atom_Sequence*>(notifyBuffer_.data());
  if (sequence->atom.type != map(LV2_ATOM__Sequence))
    return;

  const LV2_URID patchSet = map(LV2_PATCH__Set);
  const LV2_URID patchProperty = map(LV2_PATCH__property);
  const LV2_URID patchValue = map(LV2_PATCH__value);
  const LV2_URID atomObject = map(LV2_ATOM__Object);
  const LV2_URID atomUrid = map(LV2_ATOM__URID);
  const LV2_URID atomPath = map(LV2_ATOM__Path);

  LV2_ATOM_SEQUENCE_FOREACH(sequence, event)
  {
    if (event->body.type != atomObject)
      continue;
    const auto* object = reinterpret_cast<const LV2_Atom_Object*>(&event->body);
    if (object->body.otype != patchSet)
      continue;

    const LV2_Atom* property = nullptr;
    const LV2_Atom* value = nullptr;
    lv2_atom_object_get(object, patchProperty, &property, patchValue, &value, 0);
    if (property == nullptr || value == nullptr || property->type != atomUrid ||
        value->type != atomPath)
      continue;

    const auto propertyUrid = reinterpret_cast<const LV2_Atom_URID*>(property)->body;
    const char* path = static_cast<const char*>(LV2_ATOM_BODY_CONST(value));
    notifications_.push_back({unmap(propertyUrid), std::string(path)});
  }
}

void FakeLv2Host::sendFile(FileSlot slot, const std::string& path)
{
  pendingMessages_.push_back({false, slot, path});
}

void FakeLv2Host::requestFiles()
{
  pendingMessages_.push_back({true, FileSlot::Nam1, {}});
}

void FakeLv2Host::runWorker()
{
  if (workerInterface_ == nullptr)
    return;

  while (!pendingWork_.empty())
  {
    const std::vector<uint8_t> job = std::move(pendingWork_.front());
    pendingWork_.pop_front();
    workerInterface_->work(instance_, &FakeLv2Host::respondCallback, this,
                           static_cast<uint32_t>(job.size()), job.data());
  }

  while (!pendingResponses_.empty())
  {
    const std::vector<uint8_t> response = std::move(pendingResponses_.front());
    pendingResponses_.pop_front();
    workerInterface_->work_response(instance_, static_cast<uint32_t>(response.size()),
                                    response.data());
  }

  if (workerInterface_->end_run != nullptr)
    workerInterface_->end_run(instance_);
}

std::vector<FileNotification> FakeLv2Host::takeNotifications()
{
  return std::exchange(notifications_, {});
}

StoredState FakeLv2Host::saveState()
{
  StoredState state;
  if (stateInterface_ == nullptr)
    return state;

  const LV2_Feature mapPathFeature{LV2_STATE__mapPath, &mapPath_};
  const LV2_Feature freePathFeature{LV2_STATE__freePath, &freePath_};
  const LV2_Feature* stateFeatures[] = {&mapPathFeature, &freePathFeature, nullptr};

  activeState_ = &state;
  stateInterface_->save(instance_, &FakeLv2Host::storeCallback, this,
                        LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE, stateFeatures);
  activeState_ = nullptr;
  return state;
}

void FakeLv2Host::restoreState(const StoredState& state)
{
  if (stateInterface_ == nullptr)
    return;

  const LV2_Feature mapPathFeature{LV2_STATE__mapPath, &mapPath_};
  const LV2_Feature freePathFeature{LV2_STATE__freePath, &freePath_};
  const LV2_Feature* stateFeatures[] = {&mapPathFeature, &freePathFeature, nullptr};

  activeRestoreState_ = &state;
  stateInterface_->restore(instance_, &FakeLv2Host::retrieveCallback, this,
                           LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE, stateFeatures);
  activeRestoreState_ = nullptr;
}

bool FakeLv2Host::hasLogContaining(const std::string& text) const
{
  return std::any_of(logMessages_.begin(), logMessages_.end(), [&text](const std::string& message)
                     { return message.find(text) != std::string::npos; });
}

LV2_URID FakeLv2Host::map(const std::string& uri)
{
  const ScopedAllocationTrackingPause pause;
  if (const auto found = urids_.find(uri); found != urids_.end())
    return found->second;
  uris_.push_back(uri);
  const auto urid = static_cast<LV2_URID>(uris_.size());
  urids_.emplace(uri, urid);
  return urid;
}

std::string FakeLv2Host::unmap(LV2_URID urid) const
{
  if (urid == 0 || urid > uris_.size())
    return {};
  return uris_[urid - 1];
}

LV2_URID FakeLv2Host::mapCallback(LV2_URID_Map_Handle handle, const char* uri)
{
  return static_cast<FakeLv2Host*>(handle)->map(uri);
}

const char* FakeLv2Host::unmapCallback(LV2_URID_Unmap_Handle handle, LV2_URID urid)
{
  const auto* host = static_cast<const FakeLv2Host*>(handle);
  if (urid == 0 || urid > host->uris_.size())
    return nullptr;
  return host->uris_[urid - 1].c_str();
}

LV2_Worker_Status FakeLv2Host::scheduleWorkCallback(LV2_Worker_Schedule_Handle handle,
                                                    uint32_t size, const void* data)
{
  const ScopedAllocationTrackingPause pause;
  const auto* bytes = static_cast<const uint8_t*>(data);
  static_cast<FakeLv2Host*>(handle)->pendingWork_.emplace_back(bytes, bytes + size);
  return LV2_WORKER_SUCCESS;
}

LV2_Worker_Status FakeLv2Host::respondCallback(LV2_Worker_Respond_Handle handle, uint32_t size,
                                               const void* data)
{
  const auto* bytes = static_cast<const uint8_t*>(data);
  static_cast<FakeLv2Host*>(handle)->pendingResponses_.emplace_back(bytes, bytes + size);
  return LV2_WORKER_SUCCESS;
}

int FakeLv2Host::logPrintfCallback(LV2_Log_Handle handle, LV2_URID type, const char* fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  const int written = logVPrintfCallback(handle, type, fmt, args);
  va_end(args);
  return written;
}

int FakeLv2Host::logVPrintfCallback(LV2_Log_Handle handle, LV2_URID type, const char* fmt,
                                    va_list args)
{
  const ScopedAllocationTrackingPause pause;
  auto* host = static_cast<FakeLv2Host*>(handle);
  std::array<char, 2048> message{};
  const int written = std::vsnprintf(message.data(), message.size(), fmt, args);
  host->logMessages_.push_back(host->unmap(type) + ": " + message.data());
  return written;
}

LV2_State_Status FakeLv2Host::storeCallback(LV2_State_Handle handle, uint32_t key,
                                            const void* value, size_t size, uint32_t type,
                                            uint32_t /*flags*/)
{
  auto* host = static_cast<FakeLv2Host*>(handle);
  const auto* bytes = static_cast<const uint8_t*>(value);
  (*host->activeState_)[host->unmap(key)] = {host->unmap(type),
                                             std::vector<uint8_t>(bytes, bytes + size)};
  return LV2_STATE_SUCCESS;
}

const void* FakeLv2Host::retrieveCallback(LV2_State_Handle handle, uint32_t key, size_t* size,
                                          uint32_t* type, uint32_t* flags)
{
  auto* host = static_cast<FakeLv2Host*>(handle);
  const auto found = host->activeRestoreState_->find(host->unmap(key));
  if (found == host->activeRestoreState_->end())
    return nullptr;

  *size = found->second.bytes.size();
  *type = host->map(found->second.typeUri);
  *flags = LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE;
  return found->second.bytes.data();
}

char* FakeLv2Host::abstractPathCallback(LV2_State_Map_Path_Handle handle, const char* absolutePath)
{
  const auto* host = static_cast<const FakeLv2Host*>(handle);
  const std::string path(absolutePath);
  const std::string prefix = host->config_.stateDirectory + "/";
  if (!host->config_.stateDirectory.empty() && path.rfind(prefix, 0) == 0)
    return duplicateString(path.substr(prefix.size()));
  return duplicateString(path);
}

char* FakeLv2Host::absolutePathCallback(LV2_State_Map_Path_Handle handle, const char* abstractPath)
{
  const auto* host = static_cast<const FakeLv2Host*>(handle);
  const std::string path(abstractPath);
  if (path.empty() || path.front() == '/' || host->config_.stateDirectory.empty())
    return duplicateString(path);
  return duplicateString(host->config_.stateDirectory + "/" + path);
}

void FakeLv2Host::freePathCallback(LV2_State_Free_Path_Handle /*handle*/, char* path)
{
  std::free(path);
}

}  // namespace octob::lv2::test
