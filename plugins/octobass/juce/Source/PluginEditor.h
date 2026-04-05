#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "OctoberLookAndFeel.h"
#include "PluginProcessor.h"

class OctoBassEditor : public juce::AudioProcessorEditor
{
 public:
  static constexpr int kDesignWidth = 580;
  static constexpr int kDesignHeight = 400;

  explicit OctoBassEditor(OctoBassProcessor&);
  ~OctoBassEditor() override;

  void paint(juce::Graphics&) override;
  void resized() override;

 private:
  OctoberLookAndFeel laf_;
  OctoBassProcessor& audioProcessor;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OctoBassEditor)
};
