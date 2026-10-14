#pragma once

#include <juce_data_structures/juce_data_structures.h>

// Remembers, per file slot, the folder a file was last picked from in the file
// browser. Shared by every plugin instance and DAW session for the current user
// through a small settings file, so it never lives in a project's saved state.
class BrowseFolderMemory
{
 public:
  struct Slot
  {
    juce::String id;
    juce::String sameKindSlotId;
  };

  explicit BrowseFolderMemory(const juce::File& settingsFile);

  [[nodiscard]] static juce::File getDefaultSettingsFile(const juce::String& pluginName);

  [[nodiscard]] juce::File getStartFolder(const Slot& slot, const juce::File& loadedFile) const;
  void rememberPickedFile(const Slot& slot, const juce::File& pickedFile);

 private:
  juce::File settingsFile_;
};
