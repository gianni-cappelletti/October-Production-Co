#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "PluginTtl.hpp"
#include "PortTable.hpp"

using namespace octob::lv2;

namespace
{

std::string readFile(const std::string& path)
{
  std::ifstream file(path);
  std::stringstream contents;
  contents << file.rdbuf();
  return contents.str();
}

}  // namespace

TEST(Lv2Bundle, ManifestAdvertisesMonorepoVersionAsStableLv2Version)
{
  const std::string version = readFile(MONOREPO_VERSION_FILE);
  std::smatch parts;
  ASSERT_TRUE(std::regex_search(version, parts, std::regex(R"(^(\d+)\.(\d+)\.(\d+))"))) << version;
  const int major = std::stoi(parts[1]);
  const int minor = std::stoi(parts[2]);
  const int patch = std::stoi(parts[3]);

  const std::string manifest = readFile(std::string(LV2_BUNDLE_BUILD_DIR) + "/manifest.ttl");
  EXPECT_NE(manifest.find("lv2:minorVersion " + std::to_string(2 * (major * 100 + minor)) + " "),
            std::string::npos)
      << manifest;
  EXPECT_NE(manifest.find("lv2:microVersion " + std::to_string(2 * patch) + " "), std::string::npos)
      << manifest;

  const std::string pluginTtl = readFile(std::string(LV2_BUNDLE_SOURCE_DIR) + "/octobass.ttl");
  EXPECT_EQ(pluginTtl.find("lv2:minorVersion"), std::string::npos);
  EXPECT_EQ(pluginTtl.find("lv2:microVersion"), std::string::npos);
}

TEST(Lv2Bundle, CommittedPluginTtlMatchesPortTable)
{
  const std::string committed = readFile(std::string(LV2_BUNDLE_SOURCE_DIR) + "/octobass.ttl");
  ASSERT_FALSE(committed.empty());
  EXPECT_EQ(committed, generatePluginTtl()) << "Regenerate with: make octobass-lv2-ttl";
}

TEST(Lv2Bundle, CommittedDefaultPresetMatchesPortDefaults)
{
  const std::string committed = readFile(std::string(LV2_BUNDLE_SOURCE_DIR) + "/presets.ttl");
  ASSERT_FALSE(committed.empty());
  EXPECT_EQ(committed, generatePresetsTtl()) << "Regenerate with: make octobass-lv2-ttl";
}

TEST(Lv2Bundle, PortTableHasContiguousIndicesAndUniqueSymbols)
{
  const auto& table = portTable();
  ASSERT_EQ(table.size(), port::NumPorts);

  std::set<std::string> symbols;
  for (PortIndex index = 0; index < table.size(); ++index)
  {
    EXPECT_EQ(table[index].index, index);
    EXPECT_TRUE(symbols.insert(table[index].symbol).second) << table[index].symbol;
  }
}

TEST(Lv2Bundle, ControlDefaultsLieWithinTheirRanges)
{
  for (const auto& spec : portTable())
  {
    if (spec.kind != PortKind::ControlInput)
      continue;
    EXPECT_LE(spec.minimum, spec.defaultValue) << spec.symbol;
    EXPECT_GE(spec.maximum, spec.defaultValue) << spec.symbol;
    EXPECT_LT(spec.minimum, spec.maximum) << spec.symbol;
  }
}

TEST(Lv2Bundle, ExposesNoEqualizerControls)
{
  for (const auto& spec : portTable())
    EXPECT_NE(spec.symbol.rfind("eq", 0), 0u) << spec.symbol;
  EXPECT_EQ(generatePluginTtl().find("#eq>"), std::string::npos);
}

TEST(Lv2Bundle, EveryPortNameFitsModDisplaysAndStaysDistinct)
{
  constexpr size_t ModDisplayCharacters = 16;
  const std::string ttl = generatePluginTtl();
  const std::regex portStart(R"(lv2:index \d+)");
  const std::regex name(R"re(lv2:name "([^"]*)")re");
  const std::regex shortName(R"re(lv2:shortName "([^"]*)")re");

  std::vector<size_t> starts;
  for (auto it = std::sregex_iterator(ttl.begin(), ttl.end(), portStart);
       it != std::sregex_iterator(); ++it)
    starts.push_back(static_cast<size_t>(it->position()));
  ASSERT_EQ(starts.size(), portTable().size());
  starts.push_back(ttl.size());

  std::set<std::string> displayedNames;
  for (size_t i = 0; i + 1 < starts.size(); ++i)
  {
    const std::string port = ttl.substr(starts[i], starts[i + 1] - starts[i]);
    std::smatch match;
    ASSERT_TRUE(std::regex_search(port, match, name)) << port;
    std::string displayed = match[1];
    if (std::regex_search(port, match, shortName))
      displayed = match[1];

    EXPECT_LE(displayed.size(), ModDisplayCharacters) << displayed;
    EXPECT_TRUE(displayedNames.insert(displayed).second) << displayed;
  }
}

TEST(Lv2Bundle, ShipsLicenseAndThirdPartyNotices)
{
  const std::string bundle = LV2_BUNDLE_BUILD_DIR;
  EXPECT_NE(readFile(bundle + "/LICENSE").find("GNU GENERAL PUBLIC LICENSE"), std::string::npos);

  const std::string notices = readFile(bundle + "/THIRD_PARTY_NOTICES.txt");
  for (const char* component :
       {"NeuralAmpModelerCore", "Eigen", "nlohmann/json", "pffft", "WDL", "Press Start 2P", "LV2"})
    EXPECT_NE(notices.find(component), std::string::npos) << component;
  EXPECT_NE(notices.find("Redistributions in binary form must reproduce"), std::string::npos)
      << "pffft requires its license text in binary distributions";
}

TEST(Lv2Bundle, AssembledModguiHoldsExactlyTheCommittedFiles)
{
  const auto fileNames = [](const std::filesystem::path& directory)
  {
    std::set<std::string> names;
    for (const auto& entry : std::filesystem::directory_iterator(directory))
      names.insert(entry.path().filename().string());
    return names;
  };

  EXPECT_EQ(fileNames(std::filesystem::path(LV2_BUNDLE_BUILD_DIR) / "modgui"),
            fileNames(std::filesystem::path(LV2_BUNDLE_SOURCE_DIR) / "modgui"));
}
