#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <set>
#include <vector>

#include "LCDTestSupport.h"
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

std::vector<LCDDisplay*> displaysTopToBottom(juce::Component& editor)
{
  auto displays = visibleChildrenOfType<LCDDisplay>(editor);
  std::sort(displays.begin(), displays.end(), [](auto* a, auto* b)
            { return a->getBoundsInParent().getY() < b->getBoundsInParent().getY(); });
  return displays;
}

PopupToggleButton* onSwitchFor(juce::Component& editor, const juce::String& slotName)
{
  for (auto* button : visibleChildrenOfType<PopupToggleButton>(editor))
    if (screenReaderName(*button) == slotName + " On")
      return button;
  return nullptr;
}

void setParam(OctoBassProcessor& processor, const char* paramId, float value)
{
  auto* param = processor.getAPVTS().getParameter(paramId);
  ASSERT_NE(param, nullptr) << paramId;
  param->setValueNotifyingHost(value);
}

struct HostGestureRecorder : juce::AudioProcessorListener
{
  void audioProcessorParameterChanged(juce::AudioProcessor*, int, float) override {}
  void audioProcessorChanged(juce::AudioProcessor*, const ChangeDetails&) override {}
  void audioProcessorParameterChangeGestureBegin(juce::AudioProcessor*, int index) override
  {
    begun.push_back(index);
  }
  void audioProcessorParameterChangeGestureEnd(juce::AudioProcessor*, int index) override
  {
    ended.push_back(index);
  }

  std::vector<int> begun;
  std::vector<int> ended;
};

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

TEST_F(OctoBassEditorTest, EachNamSlotHasAnOnSwitchThatWorksWithoutAModel)
{
  auto editor = openEditor();
  ASSERT_NE(editor, nullptr);

  for (const juce::String slotName : {"NAM 1", "NAM 2"})
  {
    auto* onSwitch = onSwitchFor(*editor, slotName);
    ASSERT_NE(onSwitch, nullptr) << "No switch announced as '" << slotName << " On'";
    EXPECT_TRUE(onSwitch->isEnabled()) << slotName << " can be switched before a model is loaded";
    EXPECT_TRUE(onSwitch->getToggleState()) << slotName << " starts on";
    ASSERT_TRUE(onSwitch->popupTextProvider != nullptr);
    EXPECT_EQ(onSwitch->popupTextProvider(), "On");
  }
}

TEST_F(OctoBassEditorTest, FlippingAnOnSwitchSwitchesOnlyThatSlotAsOneHostGesture)
{
  auto editor = openEditor();
  ASSERT_NE(editor, nullptr);
  auto* slot2Switch = onSwitchFor(*editor, "NAM 2");
  ASSERT_NE(slot2Switch, nullptr);

  HostGestureRecorder host;
  processor.addListener(&host);
  // A click toggles the state with a synchronous click notification; triggerClick
  // would post the same thing to the message queue
  slot2Switch->setToggleState(false, juce::sendNotificationSync);
  processor.removeListener(&host);

  EXPECT_LT(processor.getAPVTS().getRawParameterValue("nam2On")->load(), 0.5f);
  EXPECT_GE(processor.getAPVTS().getRawParameterValue("namOn")->load(), 0.5f);
  EXPECT_EQ(slot2Switch->popupTextProvider(), "Off");

  // Hosts learn a MIDI mapping from the last parameter touched in a gesture
  const int slot2OnIndex = processor.getAPVTS().getParameter("nam2On")->getParameterIndex();
  EXPECT_EQ(host.begun, std::vector<int>{slot2OnIndex});
  EXPECT_EQ(host.ended, std::vector<int>{slot2OnIndex});
}

TEST_F(OctoBassEditorTest, SwitchingASlotOffFromTheHostDimsOnlyThatSlotsDisplay)
{
  juce::String err;
  ASSERT_TRUE(processor.loadNamModel(NamSlot::First, kSlot1Model.getFullPathName(), err)) << err;
  ASSERT_TRUE(processor.loadNamModel(NamSlot::Second, kSlot2Model.getFullPathName(), err)) << err;
  ASSERT_TRUE(processor.loadImpulseResponse(kImpulseResponse.getFullPathName(), err)) << err;

  auto editor = openEditor();
  ASSERT_NE(editor, nullptr);
  const auto displays = displaysTopToBottom(*editor);
  ASSERT_EQ(displays.size(), 3u);
  auto* slot1Display = displays[0];
  for (auto* display : displays)
    EXPECT_TRUE(lcd_test::looksLit(lcd_test::renderedBacklight(*display)))
        << "'" << screenReaderName(*display) << "' must be lit while its slot is on";

  // Automation and host-mapped MIDI arrive as parameter changes
  setParam(processor, "namOn", 0.0f);

  const auto dimmed = lcd_test::renderedBacklight(*slot1Display);
  EXPECT_TRUE(lcd_test::looksDimmed(dimmed));
  EXPECT_GE(lcd_test::contrastRatio(kLCDInkColour, dimmed), 4.5)
      << "WCAG 2.1 SC 1.4.3: the file name must stay readable while the slot is off";
  EXPECT_TRUE(slot1Display->isEnabled()) << "A switched-off slot can still load a model";
  auto* slot1Switch = onSwitchFor(*editor, "NAM 1");
  ASSERT_NE(slot1Switch, nullptr);
  EXPECT_FALSE(slot1Switch->getToggleState());
  EXPECT_TRUE(lcd_test::looksLit(lcd_test::renderedBacklight(*displays[1])));
  EXPECT_TRUE(lcd_test::looksLit(lcd_test::renderedBacklight(*displays[2])))
      << "The IR display has no on switch and never dims";

  setParam(processor, "namOn", 1.0f);
  EXPECT_TRUE(lcd_test::looksLit(lcd_test::renderedBacklight(*slot1Display)));
}

TEST_F(OctoBassEditorTest, SlotThatIsAlreadyOffOpensDimmedEvenWhenEmpty)
{
  setParam(processor, "nam2On", 0.0f);

  auto editor = openEditor();
  ASSERT_NE(editor, nullptr);
  const auto displays = displaysTopToBottom(*editor);
  ASSERT_EQ(displays.size(), 3u);

  EXPECT_TRUE(lcd_test::looksLit(lcd_test::renderedBacklight(*displays[0])));
  EXPECT_TRUE(lcd_test::looksDimmed(lcd_test::renderedBacklight(*displays[1])));
  auto* slot2Switch = onSwitchFor(*editor, "NAM 2");
  ASSERT_NE(slot2Switch, nullptr);
  EXPECT_FALSE(slot2Switch->getToggleState());
}
