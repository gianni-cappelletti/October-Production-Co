#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <vector>

#include "LCDTestSupport.h"
#include "PluginEditor.h"

namespace
{

const juce::String kDataDir = TEST_DATA_DIR;
const juce::File kIrA{kDataDir + "/INPUT_ir_a.wav"};
const juce::File kIrB{kDataDir + "/INPUT_ir_b.wav"};

std::vector<LCDDisplay*> displaysLeftToRight(juce::Component& editor)
{
  std::vector<LCDDisplay*> displays;
  for (auto* child : editor.getChildren())
    if (auto* display = dynamic_cast<LCDDisplay*>(child); display != nullptr && child->isVisible())
      displays.push_back(display);
  std::sort(displays.begin(), displays.end(), [](auto* a, auto* b)
            { return a->getBoundsInParent().getX() < b->getBoundsInParent().getX(); });
  return displays;
}

}  // namespace

class OctobIREditorTest : public ::testing::Test
{
 protected:
  void SetUp() override { processor.prepareToPlay(44100.0, 512); }

  std::unique_ptr<juce::AudioProcessorEditor> openEditor()
  {
    return std::unique_ptr<juce::AudioProcessorEditor>(processor.createEditor());
  }

  // Automation and host-mapped MIDI arrive as parameter changes
  void setEnabledFromHost(const char* paramId, bool enabled)
  {
    auto* param = processor.getAPVTS().getParameter(paramId);
    ASSERT_NE(param, nullptr) << paramId;
    param->setValueNotifyingHost(enabled ? 1.0f : 0.0f);
  }

  OctobIRProcessor processor;
};

TEST_F(OctobIREditorTest, EmptySlotsAreDisabledSoBothDisplaysOpenDimmed)
{
  auto editor = openEditor();
  ASSERT_NE(editor, nullptr);
  const auto displays = displaysLeftToRight(*editor);
  ASSERT_EQ(displays.size(), 2u) << "One display per IR slot";

  for (auto* display : displays)
    EXPECT_TRUE(lcd_test::looksDimmed(lcd_test::renderedBacklight(*display)));
}

TEST_F(OctobIREditorTest, LoadingAnIrLightsOnlyThatSlotsDisplay)
{
  auto editor = openEditor();
  ASSERT_NE(editor, nullptr);
  const auto displays = displaysLeftToRight(*editor);
  ASSERT_EQ(displays.size(), 2u);

  juce::String err;
  ASSERT_TRUE(processor.loadImpulseResponse1(kIrA.getFullPathName(), err)) << err;

  EXPECT_TRUE(lcd_test::looksLit(lcd_test::renderedBacklight(*displays[0])));
  EXPECT_TRUE(lcd_test::looksDimmed(lcd_test::renderedBacklight(*displays[1])));
}

TEST_F(OctobIREditorTest, DisablingASlotDimsItsDisplayButKeepsItReadable)
{
  juce::String err;
  ASSERT_TRUE(processor.loadImpulseResponse1(kIrA.getFullPathName(), err)) << err;
  ASSERT_TRUE(processor.loadImpulseResponse2(kIrB.getFullPathName(), err)) << err;

  auto editor = openEditor();
  ASSERT_NE(editor, nullptr);
  const auto displays = displaysLeftToRight(*editor);
  ASSERT_EQ(displays.size(), 2u);
  for (auto* display : displays)
    EXPECT_TRUE(lcd_test::looksLit(lcd_test::renderedBacklight(*display)));

  setEnabledFromHost("irAEnable", false);

  const auto dimmed = lcd_test::renderedBacklight(*displays[0]);
  EXPECT_TRUE(lcd_test::looksDimmed(dimmed));
  EXPECT_GE(lcd_test::contrastRatio(kLCDInkColour, dimmed), 4.5)
      << "WCAG 2.1 SC 1.4.3: the file name must stay readable while the slot is disabled";
  EXPECT_TRUE(displays[0]->isEnabled()) << "A disabled slot can still load an IR";
  EXPECT_TRUE(lcd_test::looksLit(lcd_test::renderedBacklight(*displays[1])));

  setEnabledFromHost("irAEnable", true);
  EXPECT_TRUE(lcd_test::looksLit(lcd_test::renderedBacklight(*displays[0])));
}
