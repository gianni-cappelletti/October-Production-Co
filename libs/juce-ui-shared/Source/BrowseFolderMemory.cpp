#include "BrowseFolderMemory.h"

namespace
{
constexpr int kLockTimeoutMs = 2000;

juce::String folderKey(const juce::String& slotId)
{
  return "lastFolder." + slotId;
}

juce::PropertiesFile::Options settingsOptions()
{
  juce::PropertiesFile::Options options;
  options.storageFormat = juce::PropertiesFile::storeAsXML;
  options.millisecondsBeforeSaving = -1;
  return options;
}

juce::File existingFolder(const juce::String& path)
{
  if (path.isEmpty() || !juce::File::isAbsolutePath(path))
    return {};

  juce::File folder(path);
  return folder.isDirectory() ? folder : juce::File();
}

juce::File folderOfLoadedFile(const juce::File& loadedFile)
{
  if (loadedFile == juce::File())
    return {};

  const auto folder = loadedFile.getParentDirectory();
  return folder.isDirectory() ? folder : juce::File();
}

juce::File homeFolder()
{
  return juce::File::getSpecialLocation(juce::File::userHomeDirectory);
}
}  // namespace

BrowseFolderMemory::BrowseFolderMemory(const juce::File& settingsFile) : settingsFile_(settingsFile)
{
}

juce::File BrowseFolderMemory::getDefaultSettingsFile(const juce::String& pluginName)
{
  auto appData = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
#if JUCE_MAC
  appData = appData.getChildFile("Application Support");
#endif
  return appData.getChildFile("October Production Co").getChildFile(pluginName + ".settings");
}

juce::File BrowseFolderMemory::getStartFolder(const Slot& slot, const juce::File& loadedFile,
                                              const juce::File& sameKindLoadedFile) const
{
  if (auto folder = folderOfLoadedFile(loadedFile); folder != juce::File())
  {
    DBG("Browse " + slot.id + ": opening folder of loaded file " + loadedFile.getFullPathName());
    return folder;
  }

  auto options = settingsOptions();
  options.doNotSave = true;
  const juce::PropertiesFile settings(settingsFile_, options);

  const auto rememberedPath = settings.getValue(folderKey(slot.id));
  if (rememberedPath.isNotEmpty())
  {
    if (auto folder = existingFolder(rememberedPath); folder != juce::File())
    {
      DBG("Browse " + slot.id + ": opening remembered folder " + rememberedPath);
      return folder;
    }

    DBG("Browse " + slot.id + ": remembered folder is missing, opening home: " + rememberedPath);
    return homeFolder();
  }

  if (slot.sameKindSlotId.isEmpty())
  {
    DBG("Browse " + slot.id + ": no usable folder remembered, opening home");
    return homeFolder();
  }

  if (auto folder = folderOfLoadedFile(sameKindLoadedFile); folder != juce::File())
  {
    DBG("Browse " + slot.id + ": opening folder of file loaded in " + slot.sameKindSlotId + " " +
        sameKindLoadedFile.getFullPathName());
    return folder;
  }

  const auto siblingPath = settings.getValue(folderKey(slot.sameKindSlotId));
  if (auto folder = existingFolder(siblingPath); folder != juce::File())
  {
    DBG("Browse " + slot.id + ": opening folder remembered for " + slot.sameKindSlotId + " " +
        siblingPath);
    return folder;
  }

  DBG("Browse " + slot.id + ": no usable folder remembered, opening home");
  return homeFolder();
}

void BrowseFolderMemory::rememberPickedFile(const Slot& slot, const juce::File& pickedFile)
{
  if (pickedFile == juce::File())
  {
    DBG("Browse " + slot.id + ": ignoring empty picked file");
    return;
  }

  if (const auto created = settingsFile_.getParentDirectory().createDirectory(); created.failed())
  {
    DBG("Browse " + slot.id + ": cannot create settings folder: " + created.getErrorMessage());
    return;
  }

  juce::InterProcessLock lock(
      "OctoberProductionCo_" +
      juce::String::toHexString(settingsFile_.getFullPathName().hashCode64()));
  const bool locked = lock.enter(kLockTimeoutMs);
  const juce::ScopeGuard unlock{[&lock, locked]
                                {
                                  if (locked)
                                    lock.exit();
                                }};
  if (!locked)
    DBG("Browse " + slot.id + ": settings lock timed out, saving without it");

  auto options = settingsOptions();
  options.processLock = locked ? &lock : nullptr;
  juce::PropertiesFile settings(settingsFile_, options);

  const auto folderPath = pickedFile.getParentDirectory().getFullPathName();
  settings.setValue(folderKey(slot.id), folderPath);

  if (settings.save())
    DBG("Browse " + slot.id + ": remembered folder " + folderPath);
  else
    DBG("Browse " + slot.id + ": failed to save settings to " + settingsFile_.getFullPathName());
}
