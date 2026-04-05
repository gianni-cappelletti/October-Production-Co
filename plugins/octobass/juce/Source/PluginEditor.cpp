#include "PluginEditor.h"

#include <BinaryData.h>

OctoBassEditor::OctoBassEditor(OctoBassProcessor& p) : AudioProcessorEditor(&p), audioProcessor(p)
{
  setLookAndFeel(&laf_);
  setSize(kDesignWidth, kDesignHeight);
}

OctoBassEditor::~OctoBassEditor()
{
  setLookAndFeel(nullptr);
}

void OctoBassEditor::paint(juce::Graphics& g)
{
  g.fillAll(findColour(juce::ResizableWindow::backgroundColourId));

  auto logoImage =
      juce::ImageCache::getFromMemory(BinaryData::OctoberLogo_png, BinaryData::OctoberLogo_pngSize);

  if (logoImage.isValid())
  {
    const int logoHeight = 40;
    const float aspect = static_cast<float>(logoImage.getWidth()) / logoImage.getHeight();
    const int logoWidth = static_cast<int>(logoHeight * aspect);
    g.drawImage(logoImage, juce::Rectangle<float>(10.0f, 10.0f, static_cast<float>(logoWidth),
                                                  static_cast<float>(logoHeight)));
  }

  g.setColour(juce::Colour(0xff1a1a1a));
  g.setFont(juce::Font(24.0f));
  g.drawText("OctoBASS", getLocalBounds().removeFromTop(60), juce::Justification::centred);

  g.setFont(juce::Font(14.0f));
  g.drawText("v" + juce::String(JucePlugin_VersionString),
             getLocalBounds().removeFromTop(80).removeFromBottom(20), juce::Justification::centred);
}

void OctoBassEditor::resized() {}
