#pragma once

#include <juce_data_structures/juce_data_structures.h>

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

  [[nodiscard]] juce::File getStartFolder(const Slot& slot, const juce::File& loadedFile,
                                          const juce::File& sameKindLoadedFile) const;
  void rememberPickedFile(const Slot& slot, const juce::File& pickedFile);

 private:
  juce::File settingsFile_;
};
