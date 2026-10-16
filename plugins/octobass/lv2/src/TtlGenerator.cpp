#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

#include "PluginTtl.hpp"

namespace
{

bool writeFile(const std::string& path, const std::string& contents)
{
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output)
  {
    std::fprintf(stderr, "Cannot write %s\n", path.c_str());
    return false;
  }
  output << contents;
  std::fprintf(stderr, "Wrote %s\n", path.c_str());
  return true;
}

}  // namespace

int main(int argc, char** argv)
{
  if (argc != 2)
  {
    std::fprintf(stderr, "Usage: %s <bundle directory>\n", argv[0]);
    return EXIT_FAILURE;
  }

  const std::string bundle = argv[1];
  const bool ok = writeFile(bundle + "/octobass.ttl", octob::lv2::generatePluginTtl()) &&
                  writeFile(bundle + "/presets.ttl", octob::lv2::generatePresetsTtl());
  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
