#include <gtest/gtest.h>

#include <fstream>
#include <regex>
#include <set>
#include <sstream>
#include <string>

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
