#include <gtest/gtest.h>

#include "opc-vcv-ir.hpp"

static const std::string kIrAPath = std::string(TEST_DATA_DIR) + "/INPUT_ir_a.wav";
static const std::string kIrBPath = std::string(TEST_DATA_DIR) + "/INPUT_ir_b.wav";

static const int kIrATrimIdx = static_cast<int>(OpcVcvIr::ParamId::IrATrimGainParam);
static const int kIrBTrimIdx = static_cast<int>(OpcVcvIr::ParamId::IrBTrimGainParam);

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

TEST(VcvModuleTest, ConstructorDoesNotCrash)
{
  OpcVcvIr module;
  EXPECT_TRUE(module.getLoadedFilePath(false).empty());
  EXPECT_TRUE(module.getLoadedFilePath(true).empty());
}

TEST(VcvModuleTest, ConstructorInitializesDefaultSampleRate)
{
  OpcVcvIr module;
  EXPECT_EQ(module.last_system_sample_rate_, 44100u);
}

// ---------------------------------------------------------------------------
// IR loading
// ---------------------------------------------------------------------------

TEST(VcvModuleTest, LoadIR_UpdatesPath)
{
  OpcVcvIr module;
  module.loadIR(kIrAPath);
  EXPECT_EQ(module.getLoadedFilePath(false), kIrAPath);
}

TEST(VcvModuleTest, LoadIR2_UpdatesPath)
{
  OpcVcvIr module;
  module.loadIR2(kIrBPath);
  EXPECT_EQ(module.getLoadedFilePath(true), kIrBPath);
}

TEST(VcvModuleTest, LoadIR_InvalidPath_ClearsPath)
{
  OpcVcvIr module;
  module.loadIR(kIrAPath);
  ASSERT_FALSE(module.getLoadedFilePath(false).empty());
  module.loadIR("/nonexistent/file.wav");
  EXPECT_TRUE(module.getLoadedFilePath(false).empty());
}

TEST(VcvModuleTest, LoadIR_SetsIsIR1Loaded)
{
  // ir1Loaded_ is promoted from the staging engine inside processMono/processStereo.
  // A process() call with no connected inputs takes the zero-output path and never
  // calls the core processing functions. Connect AudioInL so the pending IR is applied.
  OpcVcvIr module;
  module.loadIR(kIrAPath);
  ASSERT_FALSE(module.getLoadedFilePath(false).empty());

  module.inputs[static_cast<size_t>(OpcVcvIr::InputId::AudioInL)].connected = true;
  ProcessArgs args{44100.f, 1.f / 44100.f};
  module.process(args);
  EXPECT_TRUE(module.irProcessor_.isIR1Loaded());
}

// ---------------------------------------------------------------------------
// Sample rate
// ---------------------------------------------------------------------------

TEST(VcvModuleTest, OnSampleRateChange_UpdatesLastRate)
{
  OpcVcvIr module;
  SampleRateChangeEvent e{48000.f};
  module.onSampleRateChange(e);
  EXPECT_EQ(module.last_system_sample_rate_, 48000u);
}

// ---------------------------------------------------------------------------
// Swap
// ---------------------------------------------------------------------------

TEST(VcvModuleTest, Swap_ExchangesPaths)
{
  OpcVcvIr module;
  module.loadIR(kIrAPath);
  module.loadIR2(kIrBPath);

  module.swapImpulseResponses();

  EXPECT_EQ(module.getLoadedFilePath(false), kIrBPath);
  EXPECT_EQ(module.getLoadedFilePath(true), kIrAPath);
}

TEST(VcvModuleTest, Swap_ExchangesTrimGains)
{
  OpcVcvIr module;
  module.loadIR(kIrAPath);
  module.loadIR2(kIrBPath);
  module.params[kIrATrimIdx].setValue(3.f);
  module.params[kIrBTrimIdx].setValue(-3.f);

  module.swapImpulseResponses();

  EXPECT_NEAR(module.params[kIrATrimIdx].getValue(), -3.f, 0.001f);
  EXPECT_NEAR(module.params[kIrBTrimIdx].getValue(), 3.f, 0.001f);
}

TEST(VcvModuleTest, Swap_WithOneSlotEmpty_DoesNotCrash)
{
  OpcVcvIr module;
  module.loadIR(kIrAPath);

  ASSERT_NO_THROW(module.swapImpulseResponses());

  EXPECT_EQ(module.getLoadedFilePath(false), "");
  EXPECT_EQ(module.getLoadedFilePath(true), kIrAPath);
}

TEST(VcvModuleTest, Swap_BothSlotsEmpty_DoesNotCrash)
{
  OpcVcvIr module;
  ASSERT_NO_THROW(module.swapImpulseResponses());
}

// ---------------------------------------------------------------------------
// Serialization (round-trip)
// ---------------------------------------------------------------------------

TEST(VcvModuleTest, Serialization_PathsRoundTrip)
{
  OpcVcvIr writer;
  writer.loadIR(kIrAPath);
  writer.loadIR2(kIrBPath);

  json_t* state = writer.dataToJson();
  ASSERT_NE(state, nullptr);

  OpcVcvIr reader;
  reader.dataFromJson(state);
  json_decref(state);

  EXPECT_EQ(reader.getLoadedFilePath(false), kIrAPath);
  EXPECT_EQ(reader.getLoadedFilePath(true), kIrBPath);
}

TEST(VcvModuleTest, Serialization_EmptyPaths_DoesNotCrash)
{
  OpcVcvIr writer;
  json_t* state = writer.dataToJson();
  ASSERT_NE(state, nullptr);

  OpcVcvIr reader;
  ASSERT_NO_THROW(reader.dataFromJson(state));
  json_decref(state);

  EXPECT_TRUE(reader.getLoadedFilePath(false).empty());
  EXPECT_TRUE(reader.getLoadedFilePath(true).empty());
}
