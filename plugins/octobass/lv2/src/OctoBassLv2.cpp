#include "OctoBassLv2.hpp"

#include <lv2/atom/util.h>
#include <lv2/buf-size/buf-size.h>
#include <lv2/core/lv2_util.h>
#include <lv2/options/options.h>
#include <lv2/patch/patch.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>

namespace octob::lv2
{

namespace
{

bool isOn(float value)
{
  return value >= 0.5f;
}

int choice(float value)
{
  return static_cast<int>(std::lround(value));
}

NamSlot namSlotFor(FileSlot slot)
{
  return slot == FileSlot::Nam2 ? NamSlot::Second : NamSlot::First;
}

const char* slotName(FileSlot slot)
{
  return fileParameter(slot).label;
}

size_t slotIndex(FileSlot slot)
{
  return static_cast<size_t>(slot);
}

void copyPath(PathBuffer& destination, const char* path, size_t length)
{
  const size_t copied = std::min(length, MaxPathBytes - 1);
  std::memcpy(destination.data(), path, copied);
  destination[copied] = '\0';
}

bool hasExtension(const std::string& path, const char* extension)
{
  const size_t extensionLength = std::strlen(extension);
  if (path.size() < extensionLength)
    return false;
  return std::equal(path.end() - static_cast<std::ptrdiff_t>(extensionLength), path.end(),
                    extension,
                    [](char a, char b)
                    {
                      return std::tolower(static_cast<unsigned char>(a)) ==
                             std::tolower(static_cast<unsigned char>(b));
                    });
}

uint32_t readMaxBlockLength(const LV2_Options_Option* options, LV2_URID maxBlockLengthUrid,
                            LV2_URID atomInt)
{
  if (options == nullptr)
    return FallbackMaxBlockLength;

  for (const LV2_Options_Option* option = options; option->key != 0; ++option)
  {
    if (option->key == maxBlockLengthUrid && option->type == atomInt && option->value != nullptr)
    {
      const int32_t length = *static_cast<const int32_t*>(option->value);
      if (length > 0)
        return static_cast<uint32_t>(length);
    }
  }
  return FallbackMaxBlockLength;
}

}  // namespace

void LoadedPathsMailbox::publish(FileSlot slot, const std::string& path)
{
  const std::lock_guard<std::mutex> lock(mutex_);
  copyPath(staged_[slotIndex(slot)], path.c_str(), path.size());
  dirty_[slotIndex(slot)] = true;
  hasUpdate_.store(true, std::memory_order_release);
}

bool LoadedPathsMailbox::tryCollect(std::array<PathBuffer, NumFileSlots>& paths,
                                    std::array<bool, NumFileSlots>& changed)
{
  if (!hasUpdate_.load(std::memory_order_acquire))
    return false;

  const std::unique_lock<std::mutex> lock(mutex_, std::try_to_lock);
  if (!lock.owns_lock())
    return false;

  for (size_t i = 0; i < NumFileSlots; ++i)
  {
    if (!dirty_[i])
      continue;
    paths[i] = staged_[i];
    changed[i] = true;
    dirty_[i] = false;
  }
  hasUpdate_.store(false, std::memory_order_release);
  return true;
}

std::unique_ptr<OctoBassPlugin> OctoBassPlugin::create(double sampleRate,
                                                       const LV2_Feature* const* features)
{
  LV2_URID_Map* map = nullptr;
  LV2_Worker_Schedule* schedule = nullptr;
  LV2_Log_Log* log = nullptr;
  const LV2_Options_Option* options = nullptr;

  const char* missing = lv2_features_query(features, LV2_LOG__log, &log, false, LV2_URID__map, &map,
                                           true, LV2_WORKER__schedule, &schedule, true,
                                           LV2_OPTIONS__options, &options, false, nullptr);

  LV2_Log_Logger logger{};
  lv2_log_logger_init(&logger, map, log);
  if (missing != nullptr)
  {
    lv2_log_error(&logger, "OctoBASS: host does not provide required feature %s\n", missing);
    return nullptr;
  }

  const uint32_t maxBlockLength =
      readMaxBlockLength(options, map->map(map->handle, LV2_BUF_SIZE__maxBlockLength),
                         map->map(map->handle, LV2_ATOM__Int));
  if (options == nullptr)
    lv2_log_note(&logger,
                 "OctoBASS: host provides no block size options, processing in %u-frame chunks\n",
                 maxBlockLength);

  return std::unique_ptr<OctoBassPlugin>(
      new OctoBassPlugin(sampleRate, maxBlockLength, *map, *schedule, log));
}

OctoBassPlugin::OctoBassPlugin(double sampleRate, uint32_t maxBlockLength, LV2_URID_Map& map,
                               LV2_Worker_Schedule& schedule, LV2_Log_Log* log)
    : map_(map), schedule_(schedule), maxBlockLength_(maxBlockLength), dryBuffer_(maxBlockLength)
{
  const auto mapUri = [this](const char* uri) { return map_.map(map_.handle, uri); };
  urids_.atomPath = mapUri(LV2_ATOM__Path);
  urids_.atomUrid = mapUri(LV2_ATOM__URID);
  urids_.atomInt = mapUri(LV2_ATOM__Int);
  urids_.patchSet = mapUri(LV2_PATCH__Set);
  urids_.patchGet = mapUri(LV2_PATCH__Get);
  urids_.patchProperty = mapUri(LV2_PATCH__property);
  urids_.patchValue = mapUri(LV2_PATCH__value);
  urids_.maxBlockLength = mapUri(LV2_BUF_SIZE__maxBlockLength);
  for (const auto slot : FileSlots)
    urids_.fileParameters[slotIndex(slot)] = mapUri(fileParameter(slot).uri);

  lv2_log_logger_init(&logger_, &map_, log);
  lv2_atom_forge_init(&forge_, &map_);

  bassProcessor_.setSampleRate(sampleRate);
  bassProcessor_.setMaxBlockSize(maxBlockLength_);
  bassProcessor_.setGraphicEQLowCut(true, MinGraphicEQFreqHz);
  bassProcessor_.setGraphicEQHighCut(true, MaxGraphicEQFreqHz);
  for (const auto slot : kNamSlots)
  {
    bassProcessor_.setNamQuality(slot, DefaultNamQualityPercent / 100.0f);
    scheduledQualityPercent_[static_cast<size_t>(slot)] = DefaultNamQualityPercent;
  }
  bypassRamp_.prepare(sampleRate, BypassRampSeconds);

  lv2_log_note(&logger_, "OctoBASS: instantiated at %.0f Hz, max block %u frames\n", sampleRate,
               maxBlockLength_);
}

void OctoBassPlugin::connectPort(PortIndex index, void* data)
{
  if (index == port::Control)
    controlPort_ = static_cast<const LV2_Atom_Sequence*>(data);
  else if (index == port::Notify)
    notifyPort_ = static_cast<LV2_Atom_Sequence*>(data);
  else if (index < port::NumPorts)
    ports_[index] = static_cast<float*>(data);
}

void OctoBassPlugin::activate()
{
  bassProcessor_.reset();
  needsBypassSnap_ = true;
}

float OctoBassPlugin::control(PortIndex index) const
{
  return ports_[index] != nullptr ? *ports_[index] : 0.0f;
}

void OctoBassPlugin::run(uint32_t frames)
{
  readControlMessages();
  scheduleQualityChanges();
  collectLoadedPaths();

  const float* input = ports_[port::AudioIn];
  float* output = ports_[port::AudioOut];
  if (input != nullptr && output != nullptr)
  {
    bool lowSolo = isOn(control(port::LowBandSolo));
    bool highSolo = isOn(control(port::HighBandSolo));
    resolveSolo(lowSolo, highSolo);

    bypassRamp_.setEnabled(ports_[port::Enabled] == nullptr || isOn(control(port::Enabled)));
    if (needsBypassSnap_)
    {
      bypassRamp_.snapToTarget();
      needsBypassSnap_ = false;
    }

    for (uint32_t offset = 0; offset < frames; offset += maxBlockLength_)
    {
      const uint32_t chunk = std::min(maxBlockLength_, frames - offset);
      std::copy_n(input + offset, chunk, dryBuffer_.data());
      applyControls(lowSolo, highSolo);
      bassProcessor_.processMono(dryBuffer_.data(), output + offset, chunk);
      bypassRamp_.process(dryBuffer_.data(), output + offset, chunk);
    }
  }

  if (ports_[port::Latency] != nullptr)
    *ports_[port::Latency] = static_cast<float>(bassProcessor_.getLatencySamples());

  writeNotifications();
}

void OctoBassPlugin::resolveSolo(bool& lowSolo, bool& highSolo)
{
  const bool lowPort = lowSolo;
  const bool highPort = highSolo;
  if (lowSolo && highSolo)
  {
    if (!previousLowSoloPort_)
      lowSoloWins_ = true;
    else if (!previousHighSoloPort_)
      lowSoloWins_ = false;
    lowSolo = lowSoloWins_;
    highSolo = !lowSoloWins_;
  }
  previousLowSoloPort_ = lowPort;
  previousHighSoloPort_ = highPort;
}

void OctoBassPlugin::applyControls(bool lowSolo, bool highSolo)
{
  bassProcessor_.setCrossoverFrequency(control(port::Crossover));
  bassProcessor_.setSquash(control(port::Squash) / 100.0f);
  bassProcessor_.setCompressionMode(choice(control(port::CompressionMode)));
  bassProcessor_.setLowBandLevel(control(port::LowBandLevel));
  bassProcessor_.setHighInputGain(control(port::HighInputGain));
  bassProcessor_.setHighOutputGain(control(port::HighOutputGain));
  bassProcessor_.setOutputGain(control(port::OutputGain));
  bassProcessor_.setDryWetMix(control(port::DryWet) / 100.0f);
  bassProcessor_.setGateThreshold(control(port::GateThreshold));
  bassProcessor_.setHighBandMix(control(port::HighBandMix) / 100.0f);

  for (const auto slot : kNamSlots)
  {
    bassProcessor_.setNamCalibrateInput(slot, isOn(control(port::namCalibrateInput(slot))));
    bassProcessor_.setNamInputCalibrationLevel(slot, control(port::namInputLevel(slot)));
    bassProcessor_.setNamOutputMode(slot, choice(control(port::namOutputMode(slot))));
    bassProcessor_.setNamEnabled(slot, isOn(control(port::namOn(slot))));
  }

  bassProcessor_.setLowBandSolo(lowSolo);
  bassProcessor_.setHighBandSolo(highSolo);
}

std::optional<FileSlot> OctoBassPlugin::fileSlotFor(LV2_URID property) const
{
  for (const auto slot : FileSlots)
    if (urids_.fileParameters[slotIndex(slot)] == property)
      return slot;
  return std::nullopt;
}

void OctoBassPlugin::readControlMessages()
{
  if (controlPort_ == nullptr)
    return;

  LV2_ATOM_SEQUENCE_FOREACH(controlPort_, event)
  {
    if (!lv2_atom_forge_is_object_type(&forge_, event->body.type))
      continue;

    const auto* object = reinterpret_cast<const LV2_Atom_Object*>(&event->body);
    if (object->body.otype == urids_.patchGet)
    {
      pendingNotify_.fill(true);
      continue;
    }
    if (object->body.otype != urids_.patchSet)
      continue;

    const LV2_Atom* property = nullptr;
    const LV2_Atom* value = nullptr;
    lv2_atom_object_get(object, urids_.patchProperty, &property, urids_.patchValue, &value, 0);
    if (property == nullptr || property->type != urids_.atomUrid)
    {
      lv2_log_trace(&logger_, "OctoBASS: ignoring patch:Set without a URID property\n");
      continue;
    }

    const auto slot = fileSlotFor(reinterpret_cast<const LV2_Atom_URID*>(property)->body);
    if (!slot)
    {
      lv2_log_trace(&logger_, "OctoBASS: ignoring patch:Set for an unknown property\n");
      continue;
    }
    if (value == nullptr || value->type != urids_.atomPath)
    {
      lv2_log_trace(&logger_, "OctoBASS: ignoring %s patch:Set without a path value\n",
                    slotName(*slot));
      pendingNotify_[slotIndex(*slot)] = true;
      continue;
    }

    const auto* path = static_cast<const char*>(LV2_ATOM_BODY_CONST(value));
    const auto length = static_cast<uint32_t>(strnlen(path, value->size));
    if (!scheduleFileLoad(*slot, path, length))
      pendingNotify_[slotIndex(*slot)] = true;
  }
}

bool OctoBassPlugin::scheduleFileLoad(FileSlot slot, const char* path, uint32_t length)
{
  if (length >= MaxPathBytes)
  {
    lv2_log_trace(&logger_, "OctoBASS: %s path exceeds %zu bytes\n", slotName(slot), MaxPathBytes);
    return false;
  }

  const WorkMessage message{WorkKind::LoadFile, slot, namSlotFor(slot), 0.0f, length};
  std::memcpy(workBuffer_.data(), &message, sizeof(message));
  std::memcpy(workBuffer_.data() + sizeof(message), path, length);

  const auto size = static_cast<uint32_t>(sizeof(message) + length);
  if (schedule_.schedule_work(schedule_.handle, size, workBuffer_.data()) != LV2_WORKER_SUCCESS)
  {
    lv2_log_trace(&logger_, "OctoBASS: worker queue rejected the %s load\n", slotName(slot));
    return false;
  }
  return true;
}

void OctoBassPlugin::scheduleQualityChanges()
{
  for (const auto slot : kNamSlots)
  {
    const float requested = control(port::namQuality(slot));
    float& scheduled = scheduledQualityPercent_[static_cast<size_t>(slot)];
    if (requested == scheduled)
      continue;

    const WorkMessage message{WorkKind::SetNamQuality, FileSlot::Nam1, slot, requested, 0};
    if (schedule_.schedule_work(schedule_.handle, sizeof(message), &message) == LV2_WORKER_SUCCESS)
      scheduled = requested;
    else
      lv2_log_trace(&logger_, "OctoBASS: worker queue rejected a quality change, retrying\n");
  }
}

void OctoBassPlugin::collectLoadedPaths()
{
  std::array<bool, NumFileSlots> changed{};
  if (!mailbox_.tryCollect(loadedPaths_, changed))
    return;
  for (size_t i = 0; i < NumFileSlots; ++i)
    pendingNotify_[i] = pendingNotify_[i] || changed[i];
}

void OctoBassPlugin::writeNotifications()
{
  if (notifyPort_ == nullptr)
    return;

  const uint32_t capacity = notifyPort_->atom.size;
  lv2_atom_forge_set_buffer(&forge_, reinterpret_cast<uint8_t*>(notifyPort_), capacity);

  LV2_Atom_Forge_Frame sequenceFrame;
  lv2_atom_forge_sequence_head(&forge_, &sequenceFrame, 0);
  for (const auto slot : FileSlots)
  {
    bool& pending = pendingNotify_[slotIndex(slot)];
    if (!pending)
      continue;

    const PathBuffer& path = loadedPaths_[slotIndex(slot)];
    LV2_Atom_Forge_Frame objectFrame;
    if (lv2_atom_forge_frame_time(&forge_, 0) == 0 ||
        lv2_atom_forge_object(&forge_, &objectFrame, 0, urids_.patchSet) == 0)
      break;
    lv2_atom_forge_key(&forge_, urids_.patchProperty);
    lv2_atom_forge_urid(&forge_, urids_.fileParameters[slotIndex(slot)]);
    lv2_atom_forge_key(&forge_, urids_.patchValue);
    lv2_atom_forge_path(&forge_, path.data(), static_cast<uint32_t>(std::strlen(path.data())));
    lv2_atom_forge_pop(&forge_, &objectFrame);
    pending = false;
  }
  lv2_atom_forge_pop(&forge_, &sequenceFrame);
}

LV2_Worker_Status OctoBassPlugin::work(const void* data, uint32_t size)
{
  WorkMessage message{};
  if (size < sizeof(message))
  {
    lv2_log_error(&logger_, "OctoBASS: discarding malformed worker message (%u bytes)\n", size);
    return LV2_WORKER_ERR_UNKNOWN;
  }
  std::memcpy(&message, data, sizeof(message));

  const std::lock_guard<std::mutex> lock(nonRealtimeMutex_);
  switch (message.kind)
  {
    case WorkKind::SetNamQuality:
      bassProcessor_.setNamQuality(message.namSlot, message.qualityPercent / 100.0f);
      lv2_log_trace(&logger_, "OctoBASS: NAM %d quality set to %.0f%%\n",
                    static_cast<int>(message.namSlot) + 1, message.qualityPercent);
      return LV2_WORKER_SUCCESS;

    case WorkKind::LoadFile:
    {
      if (size < sizeof(message) + message.pathLength)
      {
        lv2_log_error(&logger_, "OctoBASS: discarding truncated %s load message\n",
                      slotName(message.fileSlot));
        return LV2_WORKER_ERR_UNKNOWN;
      }
      const std::string path(static_cast<const char*>(data) + sizeof(message), message.pathLength);
      if (path.empty())
        clearFile(message.fileSlot);
      else
        loadFile(message.fileSlot, path);
      mailbox_.publish(message.fileSlot, currentPath(message.fileSlot));
      return LV2_WORKER_SUCCESS;
    }
  }

  lv2_log_error(&logger_, "OctoBASS: discarding worker message of unknown kind\n");
  return LV2_WORKER_ERR_UNKNOWN;
}

std::optional<std::string> OctoBassPlugin::rejectionReason(FileSlot slot,
                                                           const std::string& path) const
{
  if (path.front() != '/')
    return "path is not absolute";
  if (!hasExtension(path, fileParameter(slot).extension))
    return std::string("expected a ") + fileParameter(slot).extension + " file";
  std::error_code error;
  if (!std::filesystem::is_regular_file(path, error))
    return "file not found";
  return std::nullopt;
}

void OctoBassPlugin::loadFile(FileSlot slot, const std::string& path)
{
  if (const auto reason = rejectionReason(slot, path))
  {
    lv2_log_error(&logger_, "OctoBASS: rejected %s file %s: %s\n", slotName(slot), path.c_str(),
                  reason->c_str());
    return;
  }

  std::string error;
  const bool loaded = slot == FileSlot::CabinetIr
                          ? bassProcessor_.loadImpulseResponse(path, error)
                          : bassProcessor_.loadNamModel(namSlotFor(slot), path, error);
  if (loaded)
    lv2_log_note(&logger_, "OctoBASS: loaded %s %s\n", slotName(slot), path.c_str());
  else
    lv2_log_error(&logger_, "OctoBASS: failed to load %s %s: %s\n", slotName(slot), path.c_str(),
                  error.c_str());
}

void OctoBassPlugin::clearFile(FileSlot slot)
{
  if (slot == FileSlot::CabinetIr)
    bassProcessor_.clearImpulseResponse();
  else
    bassProcessor_.clearNamModel(namSlotFor(slot));
  lv2_log_note(&logger_, "OctoBASS: cleared %s\n", slotName(slot));
}

std::string OctoBassPlugin::currentPath(FileSlot slot) const
{
  return slot == FileSlot::CabinetIr ? bassProcessor_.getCurrentIRPath()
                                     : bassProcessor_.getCurrentNamModelPath(namSlotFor(slot));
}

LV2_State_Status OctoBassPlugin::save(LV2_State_Store_Function store, LV2_State_Handle handle,
                                      const LV2_Feature* const* features)
{
  LV2_State_Map_Path* mapPath = nullptr;
  LV2_State_Free_Path* freePath = nullptr;
  lv2_features_query(features, LV2_STATE__mapPath, &mapPath, false, LV2_STATE__freePath, &freePath,
                     false, nullptr);

  const std::lock_guard<std::mutex> lock(nonRealtimeMutex_);
  for (const auto slot : FileSlots)
  {
    const std::string path = currentPath(slot);
    if (path.empty())
      continue;

    char* abstractPath =
        mapPath != nullptr ? mapPath->abstract_path(mapPath->handle, path.c_str()) : nullptr;
    const char* stored = abstractPath != nullptr ? abstractPath : path.c_str();
    const LV2_State_Status status =
        store(handle, urids_.fileParameters[slotIndex(slot)], stored, std::strlen(stored) + 1,
              urids_.atomPath, LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE);
    if (status != LV2_STATE_SUCCESS)
      lv2_log_error(&logger_, "OctoBASS: host failed to store the %s path\n", slotName(slot));

    if (abstractPath != nullptr)
    {
      if (freePath != nullptr)
        freePath->free_path(freePath->handle, abstractPath);
      else
        std::free(abstractPath);
    }
  }
  return LV2_STATE_SUCCESS;
}

LV2_State_Status OctoBassPlugin::restore(LV2_State_Retrieve_Function retrieve,
                                         LV2_State_Handle handle,
                                         const LV2_Feature* const* features)
{
  LV2_State_Map_Path* mapPath = nullptr;
  LV2_State_Free_Path* freePath = nullptr;
  lv2_features_query(features, LV2_STATE__mapPath, &mapPath, false, LV2_STATE__freePath, &freePath,
                     false, nullptr);

  const std::lock_guard<std::mutex> lock(nonRealtimeMutex_);
  for (const auto slot : FileSlots)
  {
    size_t size = 0;
    uint32_t type = 0;
    uint32_t flags = 0;
    const auto* value = static_cast<const char*>(
        retrieve(handle, urids_.fileParameters[slotIndex(slot)], &size, &type, &flags));

    std::string path;
    if (value != nullptr && type == urids_.atomPath && size > 0)
    {
      const std::string stored(value, strnlen(value, size));
      char* absolutePath =
          mapPath != nullptr ? mapPath->absolute_path(mapPath->handle, stored.c_str()) : nullptr;
      path = absolutePath != nullptr ? absolutePath : stored;
      if (absolutePath != nullptr)
      {
        if (freePath != nullptr)
          freePath->free_path(freePath->handle, absolutePath);
        else
          std::free(absolutePath);
      }
    }
    else if (value != nullptr)
    {
      lv2_log_warning(&logger_, "OctoBASS: ignoring restored %s value that is not a path\n",
                      slotName(slot));
    }

    if (path.empty())
    {
      if (!currentPath(slot).empty())
        clearFile(slot);
    }
    else
    {
      loadFile(slot, path);
      if (currentPath(slot) != path)
      {
        lv2_log_warning(&logger_, "OctoBASS: restored state has no usable %s, clearing it\n",
                        slotName(slot));
        clearFile(slot);
      }
    }
    mailbox_.publish(slot, currentPath(slot));
  }
  return LV2_STATE_SUCCESS;
}

}  // namespace octob::lv2
