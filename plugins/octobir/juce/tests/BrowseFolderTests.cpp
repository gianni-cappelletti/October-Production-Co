#include <gtest/gtest.h>

#include <memory>
#include <ostream>

#include "BrowseFolderMemory.h"
#include "PluginProcessor.h"

namespace juce
{
inline void PrintTo(const File& file, std::ostream* os)
{
  *os << '"' << file.getFullPathName().toStdString() << '"';
}
}  // namespace juce

using FileSlot = OctobIRProcessor::FileSlot;

namespace
{
const juce::File kSourceIR{juce::String(TEST_DATA_DIR) + "/INPUT_ir_a.wav"};

juce::File homeFolder()
{
  return juce::File::getSpecialLocation(juce::File::userHomeDirectory);
}
}  // namespace

class OctobIRBrowseFolderTest : public ::testing::Test
{
 protected:
  void SetUp() override
  {
    root_ = juce::File::getSpecialLocation(juce::File::tempDirectory)
                .getNonexistentChildFile("octobir-browse-folder-test", "");
    ASSERT_TRUE(root_.createDirectory());
  }

  void TearDown() override { root_.deleteRecursively(); }

  juce::File settingsFile() const
  {
    return root_.getChildFile("settings").getChildFile("OctobIR.settings");
  }

  std::unique_ptr<OctobIRProcessor> newInstance() const
  {
    return std::make_unique<OctobIRProcessor>(settingsFile());
  }

  juce::File makeFolder(const juce::String& name) const
  {
    auto folder = root_.getChildFile("library").getChildFile(name);
    EXPECT_TRUE(folder.createDirectory());
    return folder;
  }

  static juce::File copyIRInto(const juce::File& folder, const juce::String& name = "cab.wav")
  {
    auto file = folder.getChildFile(name);
    EXPECT_TRUE(kSourceIR.copyFileTo(file));
    return file;
  }

  static juce::File writeInvalidIRInto(const juce::File& folder)
  {
    auto file = folder.getChildFile("not-audio.wav");
    EXPECT_TRUE(file.replaceWithText("this is not a wav file"));
    return file;
  }

  static bool pick(OctobIRProcessor& instance, FileSlot slot, const juce::File& file)
  {
    juce::String error;
    return instance.loadPickedFile(slot, file, error);
  }

  juce::File root_;
};

TEST_F(OctobIRBrowseFolderTest, FirstEverBrowse_OpensHomeFolder)
{
  auto instance = newInstance();
  EXPECT_EQ(instance->getBrowseStartFolder(FileSlot::IR1), homeFolder());
  EXPECT_EQ(instance->getBrowseStartFolder(FileSlot::IR2), homeFolder());
}

TEST_F(OctobIRBrowseFolderTest, FreshInstanceInNewSession_OpensFolderPickedInPreviousSession)
{
  const auto celestion = makeFolder("Celestion");
  {
    auto previousSession = newInstance();
    ASSERT_TRUE(pick(*previousSession, FileSlot::IR1, copyIRInto(celestion)));
  }

  auto freshInstance = newInstance();
  EXPECT_EQ(freshInstance->getBrowseStartFolder(FileSlot::IR1), celestion);
}

TEST_F(OctobIRBrowseFolderTest, ClearedSlot_OpensRememberedFolder)
{
  const auto celestion = makeFolder("Celestion");
  auto instance = newInstance();
  ASSERT_TRUE(pick(*instance, FileSlot::IR1, copyIRInto(celestion)));

  instance->clearImpulseResponse1();

  EXPECT_EQ(instance->getBrowseStartFolder(FileSlot::IR1), celestion);
}

TEST_F(OctobIRBrowseFolderTest, PickInOneInstance_IsSeenByAnotherLiveInstanceWithEmptySlot)
{
  const auto mesa = makeFolder("Mesa");
  auto instanceA = newInstance();
  auto instanceB = newInstance();
  EXPECT_EQ(instanceA->getBrowseStartFolder(FileSlot::IR1), homeFolder());

  ASSERT_TRUE(pick(*instanceB, FileSlot::IR1, copyIRInto(mesa)));

  EXPECT_EQ(instanceA->getBrowseStartFolder(FileSlot::IR1), mesa);
}

TEST_F(OctobIRBrowseFolderTest, MissingRememberedFolder_FallsBackToHome_AndReturnsWhenFolderIsBack)
{
  const auto externalDrive = makeFolder("External Drive IRs");
  {
    auto previousSession = newInstance();
    ASSERT_TRUE(pick(*previousSession, FileSlot::IR1, copyIRInto(externalDrive)));
  }

  const auto unplugged = externalDrive.getSiblingFile("unplugged");
  ASSERT_TRUE(externalDrive.moveFileTo(unplugged));
  EXPECT_EQ(newInstance()->getBrowseStartFolder(FileSlot::IR1), homeFolder());

  ASSERT_TRUE(unplugged.moveFileTo(externalDrive));
  EXPECT_EQ(newInstance()->getBrowseStartFolder(FileSlot::IR1), externalDrive);
}

TEST_F(OctobIRBrowseFolderTest, LoadedFile_OpensItsFolderEvenIfAnotherInstancePickedElsewhereLater)
{
  const auto celestion = makeFolder("Celestion");
  const auto mesa = makeFolder("Mesa");
  auto instanceA = newInstance();
  auto instanceB = newInstance();

  ASSERT_TRUE(pick(*instanceA, FileSlot::IR1, copyIRInto(celestion)));
  ASSERT_TRUE(pick(*instanceB, FileSlot::IR1, copyIRInto(mesa)));

  EXPECT_EQ(instanceA->getBrowseStartFolder(FileSlot::IR1), celestion);
  EXPECT_EQ(instanceB->getBrowseStartFolder(FileSlot::IR1), mesa);
}

TEST_F(OctobIRBrowseFolderTest, LoadedFileDeletedButFolderExists_OpensItsFolder)
{
  const auto celestion = makeFolder("Celestion");
  const auto mesa = makeFolder("Mesa");
  auto instanceA = newInstance();
  auto instanceB = newInstance();
  const auto loadedFile = copyIRInto(celestion);
  ASSERT_TRUE(pick(*instanceA, FileSlot::IR1, loadedFile));
  ASSERT_TRUE(pick(*instanceB, FileSlot::IR1, copyIRInto(mesa)));

  ASSERT_TRUE(loadedFile.deleteFile());

  EXPECT_EQ(instanceA->getBrowseStartFolder(FileSlot::IR1), celestion);
}

TEST_F(OctobIRBrowseFolderTest, LoadedFilesFolderMissing_FallsBackToRememberedFolder)
{
  const auto celestion = makeFolder("Celestion");
  const auto mesa = makeFolder("Mesa");
  auto instanceA = newInstance();
  auto instanceB = newInstance();
  ASSERT_TRUE(pick(*instanceA, FileSlot::IR1, copyIRInto(celestion)));
  ASSERT_TRUE(pick(*instanceB, FileSlot::IR1, copyIRInto(mesa)));

  ASSERT_TRUE(celestion.deleteRecursively());

  EXPECT_EQ(instanceA->getBrowseStartFolder(FileSlot::IR1), mesa);
}

TEST_F(OctobIRBrowseFolderTest, NeverUsedSlot_OpensFolderOfTheIRRestoredInTheOtherSlot)
{
  const auto collaboratorFolder = makeFolder("Collaborator");
  juce::MemoryBlock projectState;
  {
    auto collaboratorInstance = newInstance();
    juce::String error;
    ASSERT_TRUE(collaboratorInstance->loadImpulseResponse1(
        copyIRInto(collaboratorFolder).getFullPathName(), error));
    collaboratorInstance->getStateInformation(projectState);
  }

  auto restoredInstance = newInstance();
  restoredInstance->setStateInformation(projectState.getData(),
                                        static_cast<int>(projectState.getSize()));
  restoredInstance->handleUpdateNowIfNeeded();
  ASSERT_TRUE(
      restoredInstance->getCurrentIR1Path().startsWith(collaboratorFolder.getFullPathName()));

  EXPECT_EQ(restoredInstance->getBrowseStartFolder(FileSlot::IR2), collaboratorFolder);
}

TEST_F(OctobIRBrowseFolderTest, NeverUsedSlot_PrefersOtherSlotsLoadedIROverItsRememberedFolder)
{
  const auto celestion = makeFolder("Celestion");
  const auto mesa = makeFolder("Mesa");
  {
    auto previousSession = newInstance();
    ASSERT_TRUE(pick(*previousSession, FileSlot::IR2, copyIRInto(celestion)));
  }

  auto instance = newInstance();
  juce::String error;
  ASSERT_TRUE(instance->loadImpulseResponse2(copyIRInto(mesa).getFullPathName(), error));

  EXPECT_EQ(instance->getBrowseStartFolder(FileSlot::IR1), mesa);
}

TEST_F(OctobIRBrowseFolderTest, UsedSlot_KeepsItsOwnFolderWhenTheOtherSlotHasAnIRLoaded)
{
  const auto celestion = makeFolder("Celestion");
  const auto mesa = makeFolder("Mesa");
  {
    auto previousSession = newInstance();
    ASSERT_TRUE(pick(*previousSession, FileSlot::IR2, copyIRInto(celestion)));
  }

  auto instance = newInstance();
  juce::String error;
  ASSERT_TRUE(instance->loadImpulseResponse1(copyIRInto(mesa).getFullPathName(), error));

  EXPECT_EQ(instance->getBrowseStartFolder(FileSlot::IR2), celestion);
}

TEST_F(OctobIRBrowseFolderTest,
       OtherSlotsLoadedIRFolderMissing_FallsBackToOtherSlotsRememberedFolder)
{
  const auto celestion = makeFolder("Celestion");
  const auto movedAway = makeFolder("Moved Away");
  {
    auto previousSession = newInstance();
    ASSERT_TRUE(pick(*previousSession, FileSlot::IR1, copyIRInto(celestion)));
  }

  auto instance = newInstance();
  juce::String error;
  ASSERT_TRUE(instance->loadImpulseResponse1(copyIRInto(movedAway).getFullPathName(), error));
  ASSERT_TRUE(movedAway.deleteRecursively());

  EXPECT_EQ(instance->getBrowseStartFolder(FileSlot::IR2), celestion);
}

TEST_F(OctobIRBrowseFolderTest, NeverUsedSlot_OpensFolderRememberedForOtherIRSlot)
{
  const auto celestion = makeFolder("Celestion");
  {
    auto previousSession = newInstance();
    ASSERT_TRUE(pick(*previousSession, FileSlot::IR1, copyIRInto(celestion)));
  }

  EXPECT_EQ(newInstance()->getBrowseStartFolder(FileSlot::IR2), celestion);
}

TEST_F(OctobIRBrowseFolderTest, UsedSlots_EachOpenTheirOwnRememberedFolder)
{
  const auto celestion = makeFolder("Celestion");
  const auto mesa = makeFolder("Mesa");
  {
    auto previousSession = newInstance();
    ASSERT_TRUE(pick(*previousSession, FileSlot::IR1, copyIRInto(celestion)));
    ASSERT_TRUE(pick(*previousSession, FileSlot::IR2, copyIRInto(mesa)));
  }

  auto freshInstance = newInstance();
  EXPECT_EQ(freshInstance->getBrowseStartFolder(FileSlot::IR1), celestion);
  EXPECT_EQ(freshInstance->getBrowseStartFolder(FileSlot::IR2), mesa);
}

TEST_F(OctobIRBrowseFolderTest, UsedSlotWithMissingFolder_DoesNotBorrowOtherSlotsFolder)
{
  const auto celestion = makeFolder("Celestion");
  const auto mesa = makeFolder("Mesa");
  {
    auto previousSession = newInstance();
    ASSERT_TRUE(pick(*previousSession, FileSlot::IR1, copyIRInto(celestion)));
    ASSERT_TRUE(pick(*previousSession, FileSlot::IR2, copyIRInto(mesa)));
  }

  ASSERT_TRUE(mesa.deleteRecursively());

  EXPECT_EQ(newInstance()->getBrowseStartFolder(FileSlot::IR2), homeFolder());
}

TEST_F(OctobIRBrowseFolderTest, PickedFileThatFailsToLoad_StillRemembersItsFolder)
{
  const auto downloads = makeFolder("Downloads");
  {
    auto previousSession = newInstance();
    EXPECT_FALSE(pick(*previousSession, FileSlot::IR1, writeInvalidIRInto(downloads)));
  }

  EXPECT_EQ(newInstance()->getBrowseStartFolder(FileSlot::IR1), downloads);
}

TEST_F(OctobIRBrowseFolderTest, RestoringAProject_DoesNotChangeRememberedFolder)
{
  const auto celestion = makeFolder("Celestion");
  const auto collaboratorFolder = makeFolder("Collaborator");
  {
    auto previousSession = newInstance();
    ASSERT_TRUE(pick(*previousSession, FileSlot::IR1, copyIRInto(celestion)));
  }

  juce::MemoryBlock projectState;
  {
    auto collaboratorInstance = newInstance();
    juce::String error;
    ASSERT_TRUE(collaboratorInstance->loadImpulseResponse1(
        copyIRInto(collaboratorFolder).getFullPathName(), error));
    collaboratorInstance->getStateInformation(projectState);
  }

  auto restoredInstance = newInstance();
  restoredInstance->setStateInformation(projectState.getData(),
                                        static_cast<int>(projectState.getSize()));
  restoredInstance->handleUpdateNowIfNeeded();
  ASSERT_TRUE(
      restoredInstance->getCurrentIR1Path().startsWith(collaboratorFolder.getFullPathName()));

  EXPECT_EQ(newInstance()->getBrowseStartFolder(FileSlot::IR1), celestion);
}

TEST_F(OctobIRBrowseFolderTest, SwappingIRs_DoesNotChangeRememberedFolders)
{
  const auto celestion = makeFolder("Celestion");
  const auto mesa = makeFolder("Mesa");
  {
    auto previousSession = newInstance();
    ASSERT_TRUE(pick(*previousSession, FileSlot::IR1, copyIRInto(celestion)));
    ASSERT_TRUE(pick(*previousSession, FileSlot::IR2, copyIRInto(mesa)));
    previousSession->swapImpulseResponses();
  }

  auto freshInstance = newInstance();
  EXPECT_EQ(freshInstance->getBrowseStartFolder(FileSlot::IR1), celestion);
  EXPECT_EQ(freshInstance->getBrowseStartFolder(FileSlot::IR2), mesa);
}

TEST_F(OctobIRBrowseFolderTest, TwoLiveInstancesPickingDifferentSlots_BothPicksAreRemembered)
{
  const auto celestion = makeFolder("Celestion");
  const auto mesa = makeFolder("Mesa");
  auto instanceA = newInstance();
  auto instanceB = newInstance();
  EXPECT_EQ(instanceA->getBrowseStartFolder(FileSlot::IR1), homeFolder());
  EXPECT_EQ(instanceB->getBrowseStartFolder(FileSlot::IR2), homeFolder());

  ASSERT_TRUE(pick(*instanceA, FileSlot::IR1, copyIRInto(celestion)));
  ASSERT_TRUE(pick(*instanceB, FileSlot::IR2, copyIRInto(mesa)));

  auto freshInstance = newInstance();
  EXPECT_EQ(freshInstance->getBrowseStartFolder(FileSlot::IR1), celestion);
  EXPECT_EQ(freshInstance->getBrowseStartFolder(FileSlot::IR2), mesa);
}

TEST_F(OctobIRBrowseFolderTest, FolderWithSpacesAndNonAsciiCharacters_IsRemembered)
{
  const auto ampeg =
      makeFolder(juce::CharPointer_UTF8("Bass IRs \xc3\xbc 8\xc3\x97"
                                        "10 'vintage'"));
  {
    auto previousSession = newInstance();
    pick(*previousSession, FileSlot::IR1, copyIRInto(ampeg));
  }

  EXPECT_EQ(newInstance()->getBrowseStartFolder(FileSlot::IR1), ampeg);
}

TEST_F(OctobIRBrowseFolderTest, CorruptSettingsFile_FallsBackToHome_ThenRemembersNextPick)
{
  ASSERT_TRUE(settingsFile().create());
  ASSERT_TRUE(settingsFile().replaceWithText("<<< not a settings file"));
  EXPECT_EQ(newInstance()->getBrowseStartFolder(FileSlot::IR1), homeFolder());

  const auto celestion = makeFolder("Celestion");
  {
    auto instance = newInstance();
    ASSERT_TRUE(pick(*instance, FileSlot::IR1, copyIRInto(celestion)));
  }

  EXPECT_EQ(newInstance()->getBrowseStartFolder(FileSlot::IR1), celestion);
}

TEST(BrowseFolderSettingsLocation, IsPerUserUnderCompanyFolder)
{
  const auto appData = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
#if JUCE_MAC
  const auto expected =
      appData.getChildFile("Application Support/October Production Co/OctobIR.settings");
#else
  const auto expected = appData.getChildFile("October Production Co/OctobIR.settings");
#endif

  EXPECT_EQ(BrowseFolderMemory::getDefaultSettingsFile("OctobIR"), expected);
}
