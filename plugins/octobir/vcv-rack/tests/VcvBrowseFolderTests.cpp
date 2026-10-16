#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include "opc-vcv-ir.hpp"

namespace fs = std::filesystem;

namespace
{
const fs::path kSourceIR = fs::path(TEST_DATA_DIR) / "INPUT_ir_a.wav";

std::string toUtf8(const fs::path& path)
{
  const std::u8string u8 = path.u8string();
  return {u8.begin(), u8.end()};
}

fs::path homeFolder()
{
#ifdef _WIN32
  const wchar_t* home = _wgetenv(L"USERPROFILE");
#else
  const char* home = std::getenv("HOME");
#endif
  return home != nullptr ? fs::path(home) : fs::path();
}

::testing::AssertionResult isFolder(const std::string& actual, const fs::path& expected)
{
  std::error_code ec;
  if (!actual.empty() &&
      fs::equivalent(fs::path(std::u8string(actual.begin(), actual.end())), expected, ec))
    return ::testing::AssertionSuccess();
  return ::testing::AssertionFailure()
         << "browser opens at \"" << actual << "\", expected \"" << toUtf8(expected) << "\"";
}

void startRackWithSavedSettings(json_t* savedSettings)
{
  browseFoldersFromJson(savedSettings);
}

void startRackWithNoSavedSettings()
{
  json_t* emptySettings = json_object();
  browseFoldersFromJson(emptySettings);
  json_decref(emptySettings);
}
}  // namespace

class VcvBrowseFolderTest : public ::testing::Test
{
 protected:
  void SetUp() override
  {
    root_ = fs::temp_directory_path() /
            (std::string("octobir-vcv-browse-test-") +
             ::testing::UnitTest::GetInstance()->current_test_info()->name());
    fs::remove_all(root_);
    ASSERT_TRUE(fs::create_directories(root_));
    startRackWithNoSavedSettings();
  }

  void TearDown() override
  {
    std::error_code ec;
    fs::remove_all(root_, ec);
    startRackWithNoSavedSettings();
  }

  fs::path makeFolder(const fs::path& name) const
  {
    const fs::path folder = root_ / "library" / name;
    EXPECT_TRUE(fs::create_directories(folder));
    return folder;
  }

  static std::string copyIRInto(const fs::path& folder)
  {
    const fs::path file = folder / "cab.wav";
    EXPECT_TRUE(fs::copy_file(kSourceIR, file));
    return toUtf8(file);
  }

  fs::path root_;
};

TEST_F(VcvBrowseFolderTest, FirstEverBrowse_OpensHomeFolder)
{
  OpcVcvIr module;
  EXPECT_TRUE(isFolder(module.getBrowseStartFolder(false), homeFolder()));
  EXPECT_TRUE(isFolder(module.getBrowseStartFolder(true), homeFolder()));
}

#ifdef _WIN32
TEST_F(VcvBrowseFolderTest, FirstEverBrowse_WithNonAsciiUserProfile_OpensHomeFolder)
{
  const fs::path nonAsciiHome = makeFolder(fs::path(u8"José 山田"));
  const std::wstring originalHome = homeFolder().wstring();
  ASSERT_EQ(_wputenv_s(L"USERPROFILE", nonAsciiHome.c_str()), 0);

  const std::string startFolder = OpcVcvIr().getBrowseStartFolder(false);

  _wputenv_s(L"USERPROFILE", originalHome.c_str());
  EXPECT_TRUE(isFolder(startFolder, nonAsciiHome));
}
#endif

TEST_F(VcvBrowseFolderTest, NewModuleInSameRackSession_OpensFolderPickedInAnotherModule)
{
  const fs::path celestion = makeFolder("Celestion");
  {
    OpcVcvIr previousModule;
    previousModule.loadPickedFile(false, copyIRInto(celestion));
  }

  OpcVcvIr newModule;
  EXPECT_TRUE(isFolder(newModule.getBrowseStartFolder(false), celestion));
}

TEST_F(VcvBrowseFolderTest, NewModuleAfterRackRestart_OpensFolderPickedInPreviousSession)
{
  const fs::path celestion = makeFolder("Celestion");
  {
    OpcVcvIr previousModule;
    previousModule.loadPickedFile(false, copyIRInto(celestion));
  }
  json_t* savedSettings = browseFoldersToJson();

  startRackWithNoSavedSettings();
  ASSERT_TRUE(isFolder(OpcVcvIr().getBrowseStartFolder(false), homeFolder()));
  startRackWithSavedSettings(savedSettings);
  json_decref(savedSettings);

  OpcVcvIr newModule;
  EXPECT_TRUE(isFolder(newModule.getBrowseStartFolder(false), celestion));
}

TEST_F(VcvBrowseFolderTest, ClearedSlot_OpensRememberedFolder)
{
  const fs::path celestion = makeFolder("Celestion");
  OpcVcvIr module;
  module.loadPickedFile(false, copyIRInto(celestion));

  module.clearIR1();

  EXPECT_TRUE(isFolder(module.getBrowseStartFolder(false), celestion));
}

TEST_F(VcvBrowseFolderTest, MissingRememberedFolder_FallsBackToHome_AndReturnsWhenFolderIsBack)
{
  const fs::path externalDrive = makeFolder("External Drive IRs");
  {
    OpcVcvIr previousModule;
    previousModule.loadPickedFile(false, copyIRInto(externalDrive));
  }

  const fs::path unplugged = externalDrive.parent_path() / "unplugged";
  fs::rename(externalDrive, unplugged);
  EXPECT_TRUE(isFolder(OpcVcvIr().getBrowseStartFolder(false), homeFolder()));

  json_t* savedSettings = browseFoldersToJson();
  startRackWithNoSavedSettings();
  startRackWithSavedSettings(savedSettings);
  json_decref(savedSettings);

  fs::rename(unplugged, externalDrive);
  EXPECT_TRUE(isFolder(OpcVcvIr().getBrowseStartFolder(false), externalDrive));
}

TEST_F(VcvBrowseFolderTest, LoadedFile_OpensItsFolderEvenIfAnotherModulePickedElsewhereLater)
{
  const fs::path celestion = makeFolder("Celestion");
  const fs::path mesa = makeFolder("Mesa");
  OpcVcvIr moduleA;
  OpcVcvIr moduleB;

  moduleA.loadPickedFile(false, copyIRInto(celestion));
  moduleB.loadPickedFile(false, copyIRInto(mesa));

  EXPECT_TRUE(isFolder(moduleA.getBrowseStartFolder(false), celestion));
  EXPECT_TRUE(isFolder(moduleB.getBrowseStartFolder(false), mesa));
}

TEST_F(VcvBrowseFolderTest, LoadedFilesFolderMissing_FallsBackToRememberedFolder)
{
  const fs::path celestion = makeFolder("Celestion");
  const fs::path mesa = makeFolder("Mesa");
  OpcVcvIr moduleA;
  OpcVcvIr moduleB;
  moduleA.loadPickedFile(false, copyIRInto(celestion));
  moduleB.loadPickedFile(false, copyIRInto(mesa));

  fs::remove_all(celestion);

  EXPECT_TRUE(isFolder(moduleA.getBrowseStartFolder(false), mesa));
}

TEST_F(VcvBrowseFolderTest, NeverUsedSlot_OpensFolderOfTheIRRestoredInTheOtherSlot)
{
  const fs::path collaboratorFolder = makeFolder("Collaborator");
  const std::string collaboratorIR = copyIRInto(collaboratorFolder);
  json_t* patchState = json_object();
  json_object_set_new(patchState, "ir1Path", json_string(collaboratorIR.c_str()));
  OpcVcvIr restoredModule;
  restoredModule.dataFromJson(patchState);
  json_decref(patchState);
  ASSERT_EQ(restoredModule.getLoadedFilePath(false), collaboratorIR);

  EXPECT_TRUE(isFolder(restoredModule.getBrowseStartFolder(true), collaboratorFolder));
}

TEST_F(VcvBrowseFolderTest, NeverUsedSlot_PrefersOtherSlotsLoadedIROverItsRememberedFolder)
{
  const fs::path celestion = makeFolder("Celestion");
  const fs::path mesa = makeFolder("Mesa");
  {
    OpcVcvIr previousModule;
    previousModule.loadPickedFile(true, copyIRInto(celestion));
  }

  OpcVcvIr module;
  module.loadIR2(copyIRInto(mesa));

  EXPECT_TRUE(isFolder(module.getBrowseStartFolder(false), mesa));
}

TEST_F(VcvBrowseFolderTest, UsedSlot_KeepsItsOwnFolderWhenTheOtherSlotHasAnIRLoaded)
{
  const fs::path celestion = makeFolder("Celestion");
  const fs::path mesa = makeFolder("Mesa");
  {
    OpcVcvIr previousModule;
    previousModule.loadPickedFile(true, copyIRInto(celestion));
  }

  OpcVcvIr module;
  module.loadIR(copyIRInto(mesa));

  EXPECT_TRUE(isFolder(module.getBrowseStartFolder(true), celestion));
}

TEST_F(VcvBrowseFolderTest, OtherSlotsLoadedIRFolderMissing_FallsBackToOtherSlotsRememberedFolder)
{
  const fs::path celestion = makeFolder("Celestion");
  const fs::path movedAway = makeFolder("Moved Away");
  {
    OpcVcvIr previousModule;
    previousModule.loadPickedFile(false, copyIRInto(celestion));
  }

  OpcVcvIr module;
  module.loadIR(copyIRInto(movedAway));
  fs::remove_all(movedAway);

  EXPECT_TRUE(isFolder(module.getBrowseStartFolder(true), celestion));
}

TEST_F(VcvBrowseFolderTest, NeverUsedSlot_OpensFolderRememberedForOtherIRSlot)
{
  const fs::path celestion = makeFolder("Celestion");
  {
    OpcVcvIr previousModule;
    previousModule.loadPickedFile(false, copyIRInto(celestion));
  }

  EXPECT_TRUE(isFolder(OpcVcvIr().getBrowseStartFolder(true), celestion));
}

TEST_F(VcvBrowseFolderTest, UsedSlots_EachOpenTheirOwnRememberedFolder)
{
  const fs::path celestion = makeFolder("Celestion");
  const fs::path mesa = makeFolder("Mesa");
  {
    OpcVcvIr previousModule;
    previousModule.loadPickedFile(false, copyIRInto(celestion));
    previousModule.loadPickedFile(true, copyIRInto(mesa));
  }

  OpcVcvIr newModule;
  EXPECT_TRUE(isFolder(newModule.getBrowseStartFolder(false), celestion));
  EXPECT_TRUE(isFolder(newModule.getBrowseStartFolder(true), mesa));
}

TEST_F(VcvBrowseFolderTest, UsedSlotWithMissingFolder_DoesNotBorrowOtherSlotsFolder)
{
  const fs::path celestion = makeFolder("Celestion");
  const fs::path mesa = makeFolder("Mesa");
  {
    OpcVcvIr previousModule;
    previousModule.loadPickedFile(false, copyIRInto(celestion));
    previousModule.loadPickedFile(true, copyIRInto(mesa));
  }

  fs::remove_all(mesa);

  EXPECT_TRUE(isFolder(OpcVcvIr().getBrowseStartFolder(true), homeFolder()));
}

TEST_F(VcvBrowseFolderTest, PickedFileThatFailsToLoad_StillRemembersItsFolder)
{
  const fs::path downloads = makeFolder("Downloads");
  const fs::path notAudio = downloads / "not-audio.wav";
  std::ofstream(notAudio) << "this is not a wav file";
  {
    OpcVcvIr previousModule;
    previousModule.loadPickedFile(false, toUtf8(notAudio));
    ASSERT_TRUE(previousModule.getLoadedFilePath(false).empty());
  }

  EXPECT_TRUE(isFolder(OpcVcvIr().getBrowseStartFolder(false), downloads));
}

TEST_F(VcvBrowseFolderTest, RestoringAPatch_DoesNotChangeRememberedFolder)
{
  const fs::path celestion = makeFolder("Celestion");
  const fs::path collaboratorFolder = makeFolder("Collaborator");
  {
    OpcVcvIr previousModule;
    previousModule.loadPickedFile(false, copyIRInto(celestion));
  }

  const std::string collaboratorIR = copyIRInto(collaboratorFolder);
  json_t* patchState = json_object();
  json_object_set_new(patchState, "ir1Path", json_string(collaboratorIR.c_str()));
  OpcVcvIr restoredModule;
  restoredModule.dataFromJson(patchState);
  json_decref(patchState);
  ASSERT_EQ(restoredModule.getLoadedFilePath(false), collaboratorIR);

  EXPECT_TRUE(isFolder(OpcVcvIr().getBrowseStartFolder(false), celestion));
}

TEST_F(VcvBrowseFolderTest, SwappingIRs_DoesNotChangeRememberedFolders)
{
  const fs::path celestion = makeFolder("Celestion");
  const fs::path mesa = makeFolder("Mesa");
  {
    OpcVcvIr previousModule;
    previousModule.loadPickedFile(false, copyIRInto(celestion));
    previousModule.loadPickedFile(true, copyIRInto(mesa));
    previousModule.swapImpulseResponses();
  }

  OpcVcvIr newModule;
  EXPECT_TRUE(isFolder(newModule.getBrowseStartFolder(false), celestion));
  EXPECT_TRUE(isFolder(newModule.getBrowseStartFolder(true), mesa));
}

TEST_F(VcvBrowseFolderTest, FolderWithSpacesAndNonAsciiCharacters_IsRemembered)
{
  const fs::path ampeg = makeFolder(fs::path(u8"Bass IRs ü 8×10 'vintage'"));
  {
    OpcVcvIr previousModule;
    previousModule.loadPickedFile(false, copyIRInto(ampeg));
  }
  json_t* savedSettings = browseFoldersToJson();
  startRackWithNoSavedSettings();
  startRackWithSavedSettings(savedSettings);
  json_decref(savedSettings);

  EXPECT_TRUE(isFolder(OpcVcvIr().getBrowseStartFolder(false), ampeg));
}

TEST_F(VcvBrowseFolderTest, MalformedSavedSettings_AreIgnored)
{
  json_t* savedSettings = json_object();
  json_object_set_new(savedSettings, "ir1Folder", json_integer(42));
  json_object_set_new(savedSettings, "ir2Folder", json_string(""));

  ASSERT_NO_FATAL_FAILURE(startRackWithSavedSettings(savedSettings));
  json_decref(savedSettings);

  OpcVcvIr module;
  EXPECT_TRUE(isFolder(module.getBrowseStartFolder(false), homeFolder()));
  EXPECT_TRUE(isFolder(module.getBrowseStartFolder(true), homeFolder()));
}

TEST_F(VcvBrowseFolderTest, MissingSavedSettings_AreIgnored)
{
  ASSERT_NO_FATAL_FAILURE(startRackWithSavedSettings(nullptr));

  EXPECT_TRUE(isFolder(OpcVcvIr().getBrowseStartFolder(false), homeFolder()));
}
