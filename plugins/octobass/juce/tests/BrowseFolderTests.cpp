#include <gtest/gtest.h>

#include <memory>
#include <ostream>

#include "PluginProcessor.h"

namespace juce
{
inline void PrintTo(const File& file, std::ostream* os)
{
  *os << '"' << file.getFullPathName().toStdString() << '"';
}
}  // namespace juce

using FileSlot = OctoBassProcessor::FileSlot;

namespace
{
const juce::File kSourceIR{juce::String(TEST_DATA_DIR) + "/INPUT_ir_a.wav"};
const juce::File kSourceNam{juce::String(TEST_DATA_DIR) + "/INPUT_octobass_hm2_a1.nam"};

juce::File homeFolder()
{
  return juce::File::getSpecialLocation(juce::File::userHomeDirectory);
}
}  // namespace

class OctoBassBrowseFolderTest : public ::testing::Test
{
 protected:
  void SetUp() override
  {
    root_ = juce::File::getSpecialLocation(juce::File::tempDirectory)
                .getNonexistentChildFile("octobass-browse-folder-test", "");
    ASSERT_TRUE(root_.createDirectory());
  }

  void TearDown() override { root_.deleteRecursively(); }

  juce::File settingsFile() const
  {
    return root_.getChildFile("settings").getChildFile("OctoBASS.settings");
  }

  std::unique_ptr<OctoBassProcessor> newInstance() const
  {
    return std::make_unique<OctoBassProcessor>(settingsFile());
  }

  juce::File makeFolder(const juce::String& name) const
  {
    auto folder = root_.getChildFile("library").getChildFile(name);
    EXPECT_TRUE(folder.createDirectory());
    return folder;
  }

  static juce::File copyInto(const juce::File& source, const juce::File& folder)
  {
    auto file = folder.getChildFile(source.getFileName());
    EXPECT_TRUE(source.copyFileTo(file));
    return file;
  }

  static bool pick(OctoBassProcessor& instance, FileSlot slot, const juce::File& file)
  {
    juce::String error;
    return instance.loadPickedFile(slot, file, error);
  }

  juce::File root_;
};

TEST_F(OctoBassBrowseFolderTest, FirstEverBrowse_OpensHomeFolder)
{
  auto instance = newInstance();
  EXPECT_EQ(instance->getBrowseStartFolder(FileSlot::Nam1), homeFolder());
  EXPECT_EQ(instance->getBrowseStartFolder(FileSlot::ImpulseResponse), homeFolder());
}

TEST_F(OctoBassBrowseFolderTest, FreshInstanceInNewSession_OpensFoldersPickedInPreviousSession)
{
  const auto captures = makeFolder("NAM Captures");
  const auto cabinets = makeFolder("Bass Cabs");
  {
    auto previousSession = newInstance();
    previousSession->prepareToPlay(44100.0, 512);
    ASSERT_TRUE(pick(*previousSession, FileSlot::Nam1, copyInto(kSourceNam, captures)));
    ASSERT_TRUE(pick(*previousSession, FileSlot::ImpulseResponse, copyInto(kSourceIR, cabinets)));
  }

  auto freshInstance = newInstance();
  EXPECT_EQ(freshInstance->getBrowseStartFolder(FileSlot::Nam1), captures);
  EXPECT_EQ(freshInstance->getBrowseStartFolder(FileSlot::ImpulseResponse), cabinets);
}

TEST_F(OctoBassBrowseFolderTest, NamAndIRSlots_DoNotShareRememberedFolders)
{
  const auto captures = makeFolder("NAM Captures");
  {
    auto previousSession = newInstance();
    previousSession->prepareToPlay(44100.0, 512);
    ASSERT_TRUE(pick(*previousSession, FileSlot::Nam1, copyInto(kSourceNam, captures)));
  }

  EXPECT_EQ(newInstance()->getBrowseStartFolder(FileSlot::ImpulseResponse), homeFolder());
}

TEST_F(OctoBassBrowseFolderTest,
       LoadedNamModel_OpensItsFolderEvenIfAnotherInstancePickedElsewhereLater)
{
  const auto ampCaptures = makeFolder("Amp Captures");
  const auto pedalCaptures = makeFolder("Pedal Captures");
  auto instanceA = newInstance();
  auto instanceB = newInstance();
  instanceA->prepareToPlay(44100.0, 512);
  instanceB->prepareToPlay(44100.0, 512);

  ASSERT_TRUE(pick(*instanceA, FileSlot::Nam1, copyInto(kSourceNam, ampCaptures)));
  ASSERT_TRUE(pick(*instanceB, FileSlot::Nam1, copyInto(kSourceNam, pedalCaptures)));

  EXPECT_EQ(instanceA->getBrowseStartFolder(FileSlot::Nam1), ampCaptures);
}

TEST_F(OctoBassBrowseFolderTest, ClearedNamSlot_OpensRememberedFolder)
{
  const auto captures = makeFolder("NAM Captures");
  auto instance = newInstance();
  instance->prepareToPlay(44100.0, 512);
  ASSERT_TRUE(pick(*instance, FileSlot::Nam1, copyInto(kSourceNam, captures)));

  instance->clearNamModel(octob::NamSlot::First);

  EXPECT_EQ(instance->getBrowseStartFolder(FileSlot::Nam1), captures);
}

TEST_F(OctoBassBrowseFolderTest, PickedNamModelThatFailsToLoad_StillRemembersItsFolder)
{
  const auto downloads = makeFolder("Downloads");
  const auto brokenModel = downloads.getChildFile("broken.nam");
  ASSERT_TRUE(brokenModel.replaceWithText("{ not a nam model"));
  {
    auto previousSession = newInstance();
    previousSession->prepareToPlay(44100.0, 512);
    EXPECT_FALSE(pick(*previousSession, FileSlot::Nam1, brokenModel));
  }

  EXPECT_EQ(newInstance()->getBrowseStartFolder(FileSlot::Nam1), downloads);
}

TEST_F(OctoBassBrowseFolderTest, RestoringAProject_DoesNotChangeRememberedFolders)
{
  const auto captures = makeFolder("NAM Captures");
  const auto cabinets = makeFolder("Bass Cabs");
  const auto collaboratorFolder = makeFolder("Collaborator");
  {
    auto previousSession = newInstance();
    previousSession->prepareToPlay(44100.0, 512);
    ASSERT_TRUE(pick(*previousSession, FileSlot::Nam1, copyInto(kSourceNam, captures)));
    ASSERT_TRUE(pick(*previousSession, FileSlot::ImpulseResponse, copyInto(kSourceIR, cabinets)));
  }

  juce::MemoryBlock projectState;
  {
    auto collaboratorInstance = newInstance();
    collaboratorInstance->prepareToPlay(44100.0, 512);
    juce::String error;
    ASSERT_TRUE(collaboratorInstance->loadNamModel(
        octob::NamSlot::First, copyInto(kSourceNam, collaboratorFolder).getFullPathName(), error));
    ASSERT_TRUE(collaboratorInstance->loadImpulseResponse(
        copyInto(kSourceIR, collaboratorFolder).getFullPathName(), error));
    collaboratorInstance->getStateInformation(projectState);
  }

  auto restoredInstance = newInstance();
  restoredInstance->prepareToPlay(44100.0, 512);
  restoredInstance->setStateInformation(projectState.getData(),
                                        static_cast<int>(projectState.getSize()));
  restoredInstance->handleUpdateNowIfNeeded();
  ASSERT_TRUE(restoredInstance->getCurrentNamModelPath(octob::NamSlot::First)
                  .startsWith(collaboratorFolder.getFullPathName()));
  ASSERT_TRUE(
      restoredInstance->getCurrentIRPath().startsWith(collaboratorFolder.getFullPathName()));

  auto freshInstance = newInstance();
  EXPECT_EQ(freshInstance->getBrowseStartFolder(FileSlot::Nam1), captures);
  EXPECT_EQ(freshInstance->getBrowseStartFolder(FileSlot::ImpulseResponse), cabinets);
}

TEST_F(OctoBassBrowseFolderTest, TwoLiveInstancesPickingDifferentSlots_BothPicksAreRemembered)
{
  const auto captures = makeFolder("NAM Captures");
  const auto cabinets = makeFolder("Bass Cabs");
  auto instanceA = newInstance();
  auto instanceB = newInstance();
  instanceA->prepareToPlay(44100.0, 512);
  instanceB->prepareToPlay(44100.0, 512);
  EXPECT_EQ(instanceA->getBrowseStartFolder(FileSlot::Nam1), homeFolder());
  EXPECT_EQ(instanceB->getBrowseStartFolder(FileSlot::ImpulseResponse), homeFolder());

  ASSERT_TRUE(pick(*instanceA, FileSlot::Nam1, copyInto(kSourceNam, captures)));
  ASSERT_TRUE(pick(*instanceB, FileSlot::ImpulseResponse, copyInto(kSourceIR, cabinets)));

  auto freshInstance = newInstance();
  EXPECT_EQ(freshInstance->getBrowseStartFolder(FileSlot::Nam1), captures);
  EXPECT_EQ(freshInstance->getBrowseStartFolder(FileSlot::ImpulseResponse), cabinets);
}

TEST_F(OctoBassBrowseFolderTest, EachNamSlot_RemembersItsOwnFolder)
{
  const auto pedalCaptures = makeFolder("Pedal Captures");
  const auto ampCaptures = makeFolder("Amp Captures");
  {
    auto previousSession = newInstance();
    previousSession->prepareToPlay(44100.0, 512);
    ASSERT_TRUE(pick(*previousSession, FileSlot::Nam1, copyInto(kSourceNam, pedalCaptures)));
    ASSERT_TRUE(pick(*previousSession, FileSlot::Nam2, copyInto(kSourceNam, ampCaptures)));
  }

  auto freshInstance = newInstance();
  EXPECT_EQ(freshInstance->getBrowseStartFolder(FileSlot::Nam1), pedalCaptures);
  EXPECT_EQ(freshInstance->getBrowseStartFolder(FileSlot::Nam2), ampCaptures);
}

TEST_F(OctoBassBrowseFolderTest, NamSlotNeverPickedFrom_OpensTheOtherNamSlotsFolder)
{
  const auto captures = makeFolder("NAM Captures");
  {
    auto previousSession = newInstance();
    previousSession->prepareToPlay(44100.0, 512);
    ASSERT_TRUE(pick(*previousSession, FileSlot::Nam1, copyInto(kSourceNam, captures)));
  }
  EXPECT_EQ(newInstance()->getBrowseStartFolder(FileSlot::Nam2), captures);
  EXPECT_EQ(newInstance()->getBrowseStartFolder(FileSlot::ImpulseResponse), homeFolder());
}

TEST_F(OctoBassBrowseFolderTest, FirstNamSlotNeverPickedFrom_OpensTheSecondNamSlotsFolder)
{
  const auto captures = makeFolder("NAM Captures");
  {
    auto previousSession = newInstance();
    previousSession->prepareToPlay(44100.0, 512);
    ASSERT_TRUE(pick(*previousSession, FileSlot::Nam2, copyInto(kSourceNam, captures)));
  }
  EXPECT_EQ(newInstance()->getBrowseStartFolder(FileSlot::Nam1), captures);
}

TEST_F(OctoBassBrowseFolderTest, LoadedSecondNamModel_OpensItsFolder)
{
  const auto pedalCaptures = makeFolder("Pedal Captures");
  const auto ampCaptures = makeFolder("Amp Captures");
  auto instance = newInstance();
  instance->prepareToPlay(44100.0, 512);
  ASSERT_TRUE(pick(*instance, FileSlot::Nam1, copyInto(kSourceNam, pedalCaptures)));

  juce::String error;
  ASSERT_TRUE(instance->loadNamModel(octob::NamSlot::Second,
                                     copyInto(kSourceNam, ampCaptures).getFullPathName(), error));

  EXPECT_EQ(instance->getBrowseStartFolder(FileSlot::Nam2), ampCaptures);
}
