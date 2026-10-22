#pragma once

#include <lv2/atom/atom.h>
#include <lv2/atom/forge.h>
#include <lv2/core/lv2.h>
#include <lv2/log/logger.h>
#include <lv2/state/state.h>
#include <lv2/urid/urid.h>
#include <lv2/worker/worker.h>

#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <octobass-core/BassProcessor.hpp>
#include <optional>
#include <string>
#include <vector>

#include "BypassRamp.hpp"
#include "Ports.hpp"
#include "Uris.hpp"

namespace octob::lv2
{

constexpr size_t MaxPathBytes = 4096;
constexpr uint32_t FallbackMaxBlockLength = 4096;

using PathBuffer = std::array<char, MaxPathBytes>;

class LoadedPathsMailbox
{
 public:
  void publish(FileSlot slot, const std::string& path);
  bool tryCollect(std::array<PathBuffer, NumFileSlots>& paths,
                  std::array<bool, NumFileSlots>& changed);

 private:
  std::mutex mutex_;
  std::array<PathBuffer, NumFileSlots> staged_{};
  std::array<bool, NumFileSlots> dirty_{};
  std::atomic<bool> hasUpdate_{false};
};

class OctoBassPlugin
{
 public:
  static std::unique_ptr<OctoBassPlugin> create(double sampleRate,
                                                const LV2_Feature* const* features);

  void connectPort(PortIndex index, void* data);
  void activate();
  void run(uint32_t frames);

  LV2_Worker_Status work(const void* data, uint32_t size);

  LV2_State_Status save(LV2_State_Store_Function store, LV2_State_Handle handle,
                        const LV2_Feature* const* features);
  LV2_State_Status restore(LV2_State_Retrieve_Function retrieve, LV2_State_Handle handle,
                           const LV2_Feature* const* features);

  void logUnhandledException(const char* entryPoint, const char* what);

 private:
  struct Urids
  {
    LV2_URID atomPath;
    LV2_URID atomUrid;
    LV2_URID atomInt;
    LV2_URID patchSet;
    LV2_URID patchGet;
    LV2_URID patchProperty;
    LV2_URID patchValue;
    LV2_URID maxBlockLength;
    std::array<LV2_URID, NumFileSlots> fileParameters;
  };

  struct WorkMessage
  {
    FileSlot fileSlot;
    uint32_t pathLength;
  };

  OctoBassPlugin(double sampleRate, uint32_t maxBlockLength, LV2_URID_Map& map,
                 LV2_Worker_Schedule& schedule, LV2_Log_Log* log);

  void applyControls(bool lowSolo, bool highSolo);
  void resolveSolo(bool& lowSolo, bool& highSolo);
  void readControlMessages();
  void collectLoadedPaths();
  void writeNotifications();
  bool scheduleFileLoad(FileSlot slot, const char* path, uint32_t length);

  void loadFile(FileSlot slot, const std::string& path);
  void clearFile(FileSlot slot);
  std::string currentPath(FileSlot slot) const;
  std::optional<std::string> rejectionReason(FileSlot slot, const std::string& path) const;
  std::optional<FileSlot> fileSlotFor(LV2_URID property) const;

  float control(PortIndex index) const;

  Urids urids_{};
  LV2_URID_Map& map_;
  LV2_Worker_Schedule& schedule_;
  LV2_Log_Logger logger_{};
  LV2_Atom_Forge forge_{};

  BassProcessor bassProcessor_;
  BypassRamp bypassRamp_;
  uint32_t maxBlockLength_;
  std::vector<float> dryBuffer_;

  std::array<float*, port::NumPorts> ports_{};
  const LV2_Atom_Sequence* controlPort_ = nullptr;
  LV2_Atom_Sequence* notifyPort_ = nullptr;

  bool previousLowSoloPort_ = false;
  bool previousHighSoloPort_ = false;
  bool lowSoloWins_ = true;
  bool needsBypassSnap_ = true;

  std::array<uint8_t, sizeof(WorkMessage) + MaxPathBytes> workBuffer_{};
  std::array<PathBuffer, NumFileSlots> loadedPaths_{};
  std::array<bool, NumFileSlots> pendingNotify_{};
  LoadedPathsMailbox mailbox_;

  mutable std::mutex nonRealtimeMutex_;
};

}  // namespace octob::lv2
