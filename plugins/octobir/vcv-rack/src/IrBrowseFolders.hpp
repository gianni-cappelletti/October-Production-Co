#pragma once

#ifdef OCTOBIR_VCV_HEADLESS_TEST
#include "rack_stub.hpp"
#else
#include <rack.hpp>
#endif

#include <cstdlib>
#include <mutex>
#include <string>

// Plugin-wide memory of the folder each IR slot last picked a file from,
// persisted through Rack's settingsToJson/settingsFromJson plugin callbacks.
struct IrBrowseFolders
{
  std::mutex mutex;
  std::string ir1Folder;
  std::string ir2Folder;
};

inline IrBrowseFolders& irBrowseFolders()
{
  static IrBrowseFolders folders;
  return folders;
}

inline std::string userHomeFolder()
{
#ifdef _WIN32
  const char* home = std::getenv("USERPROFILE");
#else
  const char* home = std::getenv("HOME");
#endif
  return home != nullptr ? std::string(home) : std::string();
}

inline json_t* browseFoldersToJson()
{
  IrBrowseFolders& folders = irBrowseFolders();
  std::lock_guard<std::mutex> lock(folders.mutex);
  json_t* rootJ = json_object();
  if (!folders.ir1Folder.empty())
    json_object_set_new(rootJ, "ir1Folder", json_string(folders.ir1Folder.c_str()));
  if (!folders.ir2Folder.empty())
    json_object_set_new(rootJ, "ir2Folder", json_string(folders.ir2Folder.c_str()));
  return rootJ;
}

inline std::string browseFolderFromJson(const json_t* rootJ, const char* key)
{
  const json_t* folderJ = json_object_get(rootJ, key);
  if (folderJ == nullptr)
    return "";
  if (json_is_string(folderJ))
    return json_string_value(folderJ);

  WARN("Ignoring non-string %s in plugin settings", key);
  return "";
}

inline void browseFoldersFromJson(const json_t* rootJ)
{
  IrBrowseFolders& folders = irBrowseFolders();
  std::lock_guard<std::mutex> lock(folders.mutex);
  folders.ir1Folder = browseFolderFromJson(rootJ, "ir1Folder");
  folders.ir2Folder = browseFolderFromJson(rootJ, "ir2Folder");
  INFO("Restored IR browse folders: IR A \"%s\", IR B \"%s\"", folders.ir1Folder.c_str(),
       folders.ir2Folder.c_str());
}

inline void rememberPickedIRFolder(bool ir2, const std::string& filePath)
{
  const std::string folder = rack::system::getDirectory(filePath);
  if (folder.empty())
  {
    WARN("Not remembering browse folder for unparseable path %s", filePath.c_str());
    return;
  }

  IrBrowseFolders& folders = irBrowseFolders();
  std::lock_guard<std::mutex> lock(folders.mutex);
  std::string& slotFolder = ir2 ? folders.ir2Folder : folders.ir1Folder;
  slotFolder = folder;
  INFO("Remembered IR %s browse folder %s", ir2 ? "B" : "A", folder.c_str());
}

inline std::string resolveIRBrowseStartFolder(bool ir2, const std::string& loadedFilePath)
{
  const char* slotName = ir2 ? "B" : "A";

  if (!loadedFilePath.empty())
  {
    const std::string loadedFolder = rack::system::getDirectory(loadedFilePath);
    if (rack::system::isDirectory(loadedFolder))
    {
      INFO("Browse IR %s: opening folder of loaded file %s", slotName, loadedFolder.c_str());
      return loadedFolder;
    }
  }

  IrBrowseFolders& folders = irBrowseFolders();
  std::lock_guard<std::mutex> lock(folders.mutex);
  const std::string& slotFolder = ir2 ? folders.ir2Folder : folders.ir1Folder;
  const std::string& otherSlotFolder = ir2 ? folders.ir1Folder : folders.ir2Folder;

  if (!slotFolder.empty())
  {
    if (rack::system::isDirectory(slotFolder))
    {
      INFO("Browse IR %s: opening remembered folder %s", slotName, slotFolder.c_str());
      return slotFolder;
    }
    INFO("Browse IR %s: remembered folder %s is missing, opening home", slotName,
         slotFolder.c_str());
    return userHomeFolder();
  }

  if (!otherSlotFolder.empty() && rack::system::isDirectory(otherSlotFolder))
  {
    INFO("Browse IR %s: opening folder remembered for the other slot %s", slotName,
         otherSlotFolder.c_str());
    return otherSlotFolder;
  }

  INFO("Browse IR %s: no usable folder remembered, opening home", slotName);
  return userHomeFolder();
}
