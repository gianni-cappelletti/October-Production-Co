#include <gtest/gtest.h>

#include <memory>
#include <set>
#include <vector>

#include "PluginEditor.h"

using octob::NamSlot;

namespace
{

const juce::String kDataDir = TEST_DATA_DIR;
const juce::File kSlot1Model{kDataDir + "/INPUT_octobass_hm2_a1.nam"};
const juce::File kSlot2Model{kDataDir + "/INPUT_calibration_linear.nam"};
const juce::File kImpulseResponse{kDataDir + "/INPUT_ir_a.wav"};

bool isControl(juce::Component& component)
{
  return dynamic_cast<juce::Button*>(&component) != nullptr ||
         dynamic_cast<juce::Slider*>(&component) != nullptr ||
         dynamic_cast<LCDDisplay*>(&component) != nullptr;
}

std::vector<juce::Component*> visibleControls(juce::Component& editor)
{
  std::vector<juce::Component*> controls;
  for (auto* child : editor.getChildren())
    if (child->isVisible() && isControl(*child))
      controls.push_back(child);
  return controls;
}

template <typename ComponentType>
std::vector<ComponentType*> visibleChildrenOfType(juce::Component& editor)
{
  std::vector<ComponentType*> matches;
  for (auto* child : editor.getChildren())
    if (auto* match = dynamic_cast<ComponentType*>(child); match != nullptr && child->isVisible())
      matches.push_back(match);
  return matches;
}

// getAccessibilityHandler() needs an on-screen window, so the handler a screen
// reader would query is built directly
juce::String screenReaderName(juce::Component& component)
{
  const auto handler = component.createAccessibilityHandler();
  return handler != nullptr ? handler->getTitle() : juce::String();
}

LCDDisplay* displayShowing(juce::Component& editor, const juce::String& text)
{
  for (auto* display : visibleChildrenOfType<LCDDisplay>(editor))
    if (screenReaderName(*display) == text)
      return display;
  return nullptr;
}

}  // namespace

class OctoBassEditorTest : public ::testing::Test
{
 protected:
  void SetUp() override { processor.prepareToPlay(44100.0, 512); }

  std::unique_ptr<OctoBassEditor> openEditor()
  {
    return std::unique_ptr<OctoBassEditor>(dynamic_cast<OctoBassEditor*>(processor.createEditor()));
  }

  OctoBassProcessor processor;
};

TEST_F(OctoBassEditorTest, ControlsNeverOverlapOrLeaveTheWindowAtAnySize)
{
  auto editor = openEditor();
  ASSERT_NE(editor, nullptr);

  for (const double scale : {0.75, 1.0, 1.5})
  {
    editor->setSize(juce::roundToInt(OctoBassEditor::kDesignWidth * scale),
                    juce::roundToInt(OctoBassEditor::kDesignHeight * scale));
    const auto controls = visibleControls(*editor);
    ASSERT_FALSE(controls.empty());

    for (size_t i = 0; i < controls.size(); ++i)
    {
      // Scaled bounds are rounded outward, so neighbours may share an edge pixel
      const auto bounds = controls[i]->getBoundsInParent().reduced(1);
      EXPECT_TRUE(editor->getLocalBounds().contains(bounds))
          << "'" << screenReaderName(*controls[i]) << "' leaves the window at scale " << scale;

      for (size_t j = i + 1; j < controls.size(); ++j)
        EXPECT_FALSE(bounds.intersects(controls[j]->getBoundsInParent().reduced(1)))
            << "'" << screenReaderName(*controls[i]) << "' overlaps '"
            << screenReaderName(*controls[j]) << "' at scale " << scale;
    }
  }
}

TEST_F(OctoBassEditorTest, EveryButtonHasADistinctScreenReaderName)
{
  auto editor = openEditor();
  ASSERT_NE(editor, nullptr);

  std::set<juce::String> names;
  for (auto* button : visibleChildrenOfType<juce::Button>(*editor))
  {
    const auto name = screenReaderName(*button);
    EXPECT_TRUE(name.isNotEmpty()) << "A button has no screen reader name";
    EXPECT_TRUE(names.insert(name).second) << "Two buttons are both announced as '" << name << "'";
  }
}

TEST_F(OctoBassEditorTest, EmptySlotDisplaysSayWhichSlotIsEmpty)
{
  auto editor = openEditor();
  ASSERT_NE(editor, nullptr);

  const auto displays = visibleChildrenOfType<LCDDisplay>(*editor);
  ASSERT_EQ(displays.size(), 3u) << "One display per NAM slot plus one for the IR";

  std::set<juce::String> names;
  for (auto* display : displays)
    EXPECT_TRUE(names.insert(screenReaderName(*display)).second)
        << "Two empty slots are both announced as '" << screenReaderName(*display) << "'";
}

TEST_F(OctoBassEditorTest, ShowsEachSlotsFileInSignalFlowOrder)
{
  juce::String err;
  ASSERT_TRUE(processor.loadNamModel(NamSlot::First, kSlot1Model.getFullPathName(), err)) << err;
  ASSERT_TRUE(processor.loadNamModel(NamSlot::Second, kSlot2Model.getFullPathName(), err)) << err;
  ASSERT_TRUE(processor.loadImpulseResponse(kImpulseResponse.getFullPathName(), err)) << err;

  auto editor = openEditor();
  ASSERT_NE(editor, nullptr);

  auto* slot1Display = displayShowing(*editor, kSlot1Model.getFileName());
  auto* slot2Display = displayShowing(*editor, kSlot2Model.getFileName());
  auto* irDisplay = displayShowing(*editor, kImpulseResponse.getFileName());
  ASSERT_NE(slot1Display, nullptr);
  ASSERT_NE(slot2Display, nullptr);
  ASSERT_NE(irDisplay, nullptr);

  EXPECT_LT(slot1Display->getBoundsInParent().getY(), slot2Display->getBoundsInParent().getY());
  EXPECT_LT(slot2Display->getBoundsInParent().getY(), irDisplay->getBoundsInParent().getY());
}

TEST_F(OctoBassEditorTest, ClearingASlotOnlyEmptiesThatSlotsDisplay)
{
  juce::String err;
  ASSERT_TRUE(processor.loadNamModel(NamSlot::First, kSlot1Model.getFullPathName(), err)) << err;
  ASSERT_TRUE(processor.loadNamModel(NamSlot::Second, kSlot2Model.getFullPathName(), err)) << err;

  auto editor = openEditor();
  ASSERT_NE(editor, nullptr);

  juce::Button* clearSlot2 = nullptr;
  for (auto* button : visibleChildrenOfType<juce::Button>(*editor))
    if (screenReaderName(*button).containsIgnoreCase("clear") &&
        screenReaderName(*button).contains("2"))
      clearSlot2 = button;
  ASSERT_NE(clearSlot2, nullptr) << "No clear button announced for NAM slot 2";
  ASSERT_TRUE(clearSlot2->onClick != nullptr);

  // triggerClick posts to the message queue; onClick is the same handler, run synchronously
  clearSlot2->onClick();

  EXPECT_FALSE(processor.isNamModelLoaded(NamSlot::Second));
  EXPECT_TRUE(processor.isNamModelLoaded(NamSlot::First));
  EXPECT_NE(displayShowing(*editor, kSlot1Model.getFileName()), nullptr);
  EXPECT_EQ(displayShowing(*editor, kSlot2Model.getFileName()), nullptr);
}
