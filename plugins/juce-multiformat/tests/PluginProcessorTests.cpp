#include <gtest/gtest.h>

#include "PluginEditor.h"
#include "PluginProcessor.h"

static const std::string kIrAPath = std::string(TEST_DATA_DIR) + "/INPUT_ir_a.wav";
static const std::string kIrBPath = std::string(TEST_DATA_DIR) + "/INPUT_ir_b.wav";

class PluginProcessorTest : public ::testing::Test
{
 protected:
  OctobIRProcessor processor;
};

// IR path management

TEST_F(PluginProcessorTest, LoadIR1_UpdatesPath)
{
  juce::String err;
  ASSERT_TRUE(processor.loadImpulseResponse1(kIrAPath, err)) << err;
  EXPECT_EQ(processor.getCurrentIR1Path(), juce::String(kIrAPath));
}

TEST_F(PluginProcessorTest, LoadIR2_UpdatesPath)
{
  juce::String err;
  ASSERT_TRUE(processor.loadImpulseResponse2(kIrBPath, err)) << err;
  EXPECT_EQ(processor.getCurrentIR2Path(), juce::String(kIrBPath));
}

TEST_F(PluginProcessorTest, ClearIR1_ClearsPath)
{
  juce::String err;
  ASSERT_TRUE(processor.loadImpulseResponse1(kIrAPath, err));
  processor.clearImpulseResponse1();
  EXPECT_TRUE(processor.getCurrentIR1Path().isEmpty());
}

TEST_F(PluginProcessorTest, LoadIR1_AutoEnablesIRA)
{
  juce::String err;
  ASSERT_TRUE(processor.loadImpulseResponse1(kIrAPath, err));
  float enabled = processor.getAPVTS().getRawParameterValue("irAEnable")->load();
  EXPECT_GT(enabled, 0.5f);
}

// The plugin's reported latency must match the core's reported latency after the first
// process call. For IRs with pre-delay (peak offset > 0), this will be non-zero.
// The test IRs happen to have 0 peak offset, but the synchronization must still hold.
TEST_F(PluginProcessorTest, LoadIR1_PluginLatencyMatchesCore)
{
  processor.prepareToPlay(44100.0, 512);
  juce::String err;
  ASSERT_TRUE(processor.loadImpulseResponse1(kIrAPath, err));

  juce::AudioBuffer<float> buf(2, 512);
  buf.clear();
  juce::MidiBuffer midi;
  processor.processBlock(buf, midi);

  EXPECT_EQ(processor.getLatencySamples(), processor.getIRProcessor().getLatencySamples());
}

// Swap: path exchange

TEST_F(PluginProcessorTest, SwapExchangesPaths)
{
  juce::String err;
  ASSERT_TRUE(processor.loadImpulseResponse1(kIrAPath, err));
  ASSERT_TRUE(processor.loadImpulseResponse2(kIrBPath, err));

  processor.swapImpulseResponses();

  EXPECT_EQ(processor.getCurrentIR1Path(), juce::String(kIrBPath));
  EXPECT_EQ(processor.getCurrentIR2Path(), juce::String(kIrAPath));
}

// Swap: enable state preservation (slot-relative, not IR-relative)

TEST_F(PluginProcessorTest, SwapPreservesEnableStates_BothEnabled)
{
  juce::String err;
  ASSERT_TRUE(processor.loadImpulseResponse1(kIrAPath, err));
  ASSERT_TRUE(processor.loadImpulseResponse2(kIrBPath, err));
  // Both slots auto-enabled by loading.

  processor.swapImpulseResponses();

  float aEnabled = processor.getAPVTS().getRawParameterValue("irAEnable")->load();
  float bEnabled = processor.getAPVTS().getRawParameterValue("irBEnable")->load();
  EXPECT_GT(aEnabled, 0.5f) << "Slot A should remain enabled after swap";
  EXPECT_GT(bEnabled, 0.5f) << "Slot B should remain enabled after swap";
}

TEST_F(PluginProcessorTest, SwapPreservesEnableStates_SlotADisabled)
{
  juce::String err;
  ASSERT_TRUE(processor.loadImpulseResponse1(kIrAPath, err));
  ASSERT_TRUE(processor.loadImpulseResponse2(kIrBPath, err));

  if (auto* param = processor.getAPVTS().getParameter("irAEnable"))
    param->setValueNotifyingHost(0.0f);

  processor.swapImpulseResponses();

  // Swap preserves slot enable states: slot A was disabled, so it stays disabled
  // even though slot A now holds IR B.
  float aEnabled = processor.getAPVTS().getRawParameterValue("irAEnable")->load();
  float bEnabled = processor.getAPVTS().getRawParameterValue("irBEnable")->load();
  EXPECT_LT(aEnabled, 0.5f) << "Slot A should remain disabled after swap";
  EXPECT_GT(bEnabled, 0.5f) << "Slot B should remain enabled after swap";
}

TEST_F(PluginProcessorTest, SwapPreservesBlend)
{
  juce::String err;
  ASSERT_TRUE(processor.loadImpulseResponse1(kIrAPath, err));
  ASSERT_TRUE(processor.loadImpulseResponse2(kIrBPath, err));

  auto* blendParam = processor.getAPVTS().getParameter("blend");
  ASSERT_NE(blendParam, nullptr);
  blendParam->setValueNotifyingHost(blendParam->convertTo0to1(0.3f));

  const float blendBefore = processor.getAPVTS().getRawParameterValue("blend")->load();

  processor.swapImpulseResponses();

  const float blendAfter = processor.getAPVTS().getRawParameterValue("blend")->load();
  EXPECT_NEAR(blendAfter, blendBefore, 0.01f);
}

TEST_F(PluginProcessorTest, SwapExchangesTrimGains)
{
  juce::String err;
  ASSERT_TRUE(processor.loadImpulseResponse1(kIrAPath, err));
  ASSERT_TRUE(processor.loadImpulseResponse2(kIrBPath, err));

  auto* trimAParam = processor.getAPVTS().getParameter("irATrimGain");
  auto* trimBParam = processor.getAPVTS().getParameter("irBTrimGain");
  ASSERT_NE(trimAParam, nullptr);
  ASSERT_NE(trimBParam, nullptr);
  trimAParam->setValueNotifyingHost(trimAParam->convertTo0to1(3.0f));
  trimBParam->setValueNotifyingHost(trimBParam->convertTo0to1(-3.0f));

  processor.swapImpulseResponses();

  EXPECT_NEAR(processor.getAPVTS().getRawParameterValue("irATrimGain")->load(), -3.0f, 0.01f);
  EXPECT_NEAR(processor.getAPVTS().getRawParameterValue("irBTrimGain")->load(), 3.0f, 0.01f);
}

TEST_F(PluginProcessorTest, SwapWithOneSlotEmpty_DoesNotCrash)
{
  juce::String err;
  ASSERT_TRUE(processor.loadImpulseResponse1(kIrAPath, err));
  // Slot 2 left empty.

  ASSERT_NO_THROW(processor.swapImpulseResponses());

  // IR A moved from slot 1 to slot 2; slot 1 is now empty.
  EXPECT_TRUE(processor.getCurrentIR1Path().isEmpty());
  EXPECT_EQ(processor.getCurrentIR2Path(), juce::String(kIrAPath));
}

// State serialization

TEST_F(PluginProcessorTest, StateRoundTrip_Parameters)
{
  processor.prepareToPlay(44100.0, 512);
  juce::String err;
  ASSERT_TRUE(processor.loadImpulseResponse1(kIrAPath, err));

  auto setParam = [&](const char* id, float value)
  {
    auto* p = processor.getAPVTS().getParameter(id);
    ASSERT_NE(p, nullptr) << "Parameter not found: " << id;
    p->setValueNotifyingHost(p->convertTo0to1(value));
  };

  setParam("irAEnable", 1.0f);
  setParam("irBEnable", 1.0f);
  setParam("dynamicMode", 1.0f);
  setParam("sidechainEnable", 1.0f);
  setParam("blend", 0.25f);
  setParam("threshold", -20.0f);
  setParam("rangeDb", 35.0f);
  setParam("kneeWidthDb", 10.0f);
  setParam("detectionMode", 1.0f);
  setParam("attackTime", 100.0f);
  setParam("releaseTime", 400.0f);
  setParam("outputGain", 6.0f);
  setParam("irATrimGain", -3.0f);
  setParam("irBTrimGain", 5.0f);

  juce::MemoryBlock state;
  processor.getStateInformation(state);

  OctobIRProcessor processor2;
  processor2.setStateInformation(state.getData(), static_cast<int>(state.getSize()));

  auto expectParam = [&](const char* id, float tolerance)
  {
    const float orig = processor.getAPVTS().getRawParameterValue(id)->load();
    const float restored = processor2.getAPVTS().getRawParameterValue(id)->load();
    EXPECT_NEAR(restored, orig, tolerance) << "Mismatch for parameter: " << id;
  };

  expectParam("irAEnable", 0.01f);
  expectParam("irBEnable", 0.01f);
  expectParam("dynamicMode", 0.01f);
  expectParam("sidechainEnable", 0.01f);
  expectParam("blend", 0.01f);
  expectParam("threshold", 0.1f);
  expectParam("rangeDb", 0.1f);
  expectParam("kneeWidthDb", 0.1f);
  expectParam("detectionMode", 0.01f);
  expectParam("attackTime", 1.0f);
  expectParam("releaseTime", 1.0f);
  expectParam("outputGain", 0.1f);
  expectParam("irATrimGain", 0.1f);
  expectParam("irBTrimGain", 0.1f);
}

// Editor size persistence

TEST_F(PluginProcessorTest, EditorOpensAtDesignSize)
{
  auto* editor = processor.createEditor();
  ASSERT_NE(editor, nullptr);

  EXPECT_EQ(editor->getWidth(), OctobIREditor::kDesignWidth);
  EXPECT_EQ(editor->getHeight(), OctobIREditor::kDesignHeight);

  delete editor;
}

TEST_F(PluginProcessorTest, EditorSizeRoundTrip)
{
  int resizedWidth = 0;
  int resizedHeight = 0;

  {
    auto* editor = processor.createEditor();
    ASSERT_NE(editor, nullptr);
    editor->setSize(700, 838);
    resizedWidth = editor->getWidth();
    resizedHeight = editor->getHeight();
    delete editor;
  }

  juce::MemoryBlock state;
  processor.getStateInformation(state);

  OctobIRProcessor processor2;
  processor2.setStateInformation(state.getData(), static_cast<int>(state.getSize()));

  EXPECT_EQ(processor2.lastEditorWidth_.load(), resizedWidth);
  EXPECT_EQ(processor2.lastEditorHeight_.load(), resizedHeight);

  auto* editor2 = processor2.createEditor();
  ASSERT_NE(editor2, nullptr);

  EXPECT_EQ(editor2->getWidth(), resizedWidth);
  EXPECT_EQ(editor2->getHeight(), resizedHeight);

  delete editor2;
}

TEST_F(PluginProcessorTest, EditorSizeDefaultsWhenNoSavedState)
{
  EXPECT_EQ(processor.lastEditorWidth_.load(), OctobIREditor::kDesignWidth);
  EXPECT_EQ(processor.lastEditorHeight_.load(), OctobIREditor::kDesignHeight);
}

TEST_F(PluginProcessorTest, EditorSizeNotCorruptedByConstruction)
{
  // Set a custom size via a first editor, then read back the constrainer-adjusted values
  auto* tempEditor = processor.createEditor();
  ASSERT_NE(tempEditor, nullptr);
  tempEditor->setSize(650, 778);
  const int customWidth = tempEditor->getWidth();
  const int customHeight = tempEditor->getHeight();
  delete tempEditor;

  processor.lastEditorWidth_.store(customWidth);
  processor.lastEditorHeight_.store(customHeight);

  auto* editor = processor.createEditor();
  ASSERT_NE(editor, nullptr);

  EXPECT_EQ(editor->getWidth(), customWidth);
  EXPECT_EQ(editor->getHeight(), customHeight);
  EXPECT_EQ(processor.lastEditorWidth_.load(), customWidth);
  EXPECT_EQ(processor.lastEditorHeight_.load(), customHeight);

  delete editor;
}
