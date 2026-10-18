#pragma once

#include <lv2/atom/atom.h>
#include <lv2/core/lv2.h>
#include <lv2/log/log.h>
#include <lv2/options/options.h>
#include <lv2/state/state.h>
#include <lv2/urid/urid.h>
#include <lv2/worker/worker.h>

#include <array>
#include <cstdint>
#include <deque>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "Ports.hpp"
#include "Uris.hpp"

namespace octob::lv2::test
{

struct HostConfig
{
  double sampleRate = 48000.0;
  uint32_t maxBlockLength = 256;
  bool provideUridMap = true;
  bool provideWorker = true;
  bool provideLog = true;
  bool provideOptions = true;
  std::string stateDirectory;
  size_t notifyBufferBytes = 64 * 1024;
  std::string uriThatFailsToMap;
  bool stateCallbacksThrow = false;
};

struct StoredStateValue
{
  std::string typeUri;
  std::vector<uint8_t> bytes;
};

using StoredState = std::map<std::string, StoredStateValue>;

struct FileNotification
{
  std::string propertyUri;
  std::string path;
};

class FakeLv2Host
{
 public:
  explicit FakeLv2Host(HostConfig config = {});
  ~FakeLv2Host();

  FakeLv2Host(const FakeLv2Host&) = delete;
  FakeLv2Host& operator=(const FakeLv2Host&) = delete;
  FakeLv2Host(FakeLv2Host&&) = delete;
  FakeLv2Host& operator=(FakeLv2Host&&) = delete;

  bool isInstantiated() const { return instance_ != nullptr; }

  void setControl(PortIndex index, float value);
  float control(PortIndex index) const;

  std::vector<float> process(const std::vector<float>& input, size_t blockSize);

  void sendFile(FileSlot slot, const std::string& path);
  void requestFiles();
  void runWorker();
  size_t pendingWorkCount() const { return pendingWork_.size(); }

  std::vector<FileNotification> takeNotifications();
  size_t malformedNotificationCount() const { return malformedNotificationCount_; }

  StoredState saveState();
  void restoreState(const StoredState& state);
  LV2_State_Status lastStateStatus() const { return lastStateStatus_; }

  const std::vector<std::string>& logMessages() const { return logMessages_; }
  bool hasLogContaining(const std::string& text) const;

  void setTrackRunAllocations(bool track) { trackRunAllocations_ = track; }

  LV2_URID map(const std::string& uri);
  std::string unmap(LV2_URID urid) const;

 private:
  struct PendingMessage
  {
    bool isGet;
    FileSlot slot;
    std::string path;
  };

  void runBlock(const float* input, float* output, uint32_t frames);
  void writeControlSequence();
  void readNotifySequence();

  static LV2_URID mapCallback(LV2_URID_Map_Handle handle, const char* uri);
  static const char* unmapCallback(LV2_URID_Unmap_Handle handle, LV2_URID urid);
  static LV2_Worker_Status scheduleWorkCallback(LV2_Worker_Schedule_Handle handle, uint32_t size,
                                                const void* data);
  static LV2_Worker_Status respondCallback(LV2_Worker_Respond_Handle handle, uint32_t size,
                                           const void* data);
  static int logPrintfCallback(LV2_Log_Handle handle, LV2_URID type, const char* fmt, ...);
  static int logVPrintfCallback(LV2_Log_Handle handle, LV2_URID type, const char* fmt,
                                va_list args);
  static LV2_State_Status storeCallback(LV2_State_Handle handle, uint32_t key, const void* value,
                                        size_t size, uint32_t type, uint32_t flags);
  static const void* retrieveCallback(LV2_State_Handle handle, uint32_t key, size_t* size,
                                      uint32_t* type, uint32_t* flags);
  static char* abstractPathCallback(LV2_State_Map_Path_Handle handle, const char* absolutePath);
  static char* absolutePathCallback(LV2_State_Map_Path_Handle handle, const char* abstractPath);
  static void freePathCallback(LV2_State_Free_Path_Handle handle, char* path);

  HostConfig config_;
  std::vector<std::string> uris_;
  std::map<std::string, LV2_URID> urids_;

  LV2_URID_Map uridMap_{};
  LV2_URID_Unmap uridUnmap_{};
  LV2_Worker_Schedule workerSchedule_{};
  LV2_Log_Log log_{};
  std::array<int32_t, 2> blockLengths_{};
  std::vector<LV2_Options_Option> options_;
  LV2_State_Map_Path mapPath_{};
  LV2_State_Free_Path freePath_{};

  std::vector<LV2_Feature> featureStorage_;
  std::vector<const LV2_Feature*> features_;

  const LV2_Descriptor* descriptor_ = nullptr;
  LV2_Handle instance_ = nullptr;
  const LV2_Worker_Interface* workerInterface_ = nullptr;
  const LV2_State_Interface* stateInterface_ = nullptr;

  std::array<float, port::NumPorts> controls_{};
  std::vector<float> inputBuffer_;
  std::vector<float> outputBuffer_;
  std::vector<uint8_t> controlBuffer_;
  std::vector<uint8_t> notifyBuffer_;

  std::vector<PendingMessage> pendingMessages_;
  std::deque<std::vector<uint8_t>> pendingWork_;
  std::deque<std::vector<uint8_t>> pendingResponses_;
  std::vector<FileNotification> notifications_;
  std::vector<std::string> logMessages_;
  StoredState* activeState_ = nullptr;
  const StoredState* activeRestoreState_ = nullptr;
  bool trackRunAllocations_ = false;
  size_t malformedNotificationCount_ = 0;
  LV2_State_Status lastStateStatus_ = LV2_STATE_SUCCESS;
};

}  // namespace octob::lv2::test
