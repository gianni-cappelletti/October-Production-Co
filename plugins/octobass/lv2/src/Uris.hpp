#pragma once

#include <array>
#include <cstdint>

namespace octob::lv2
{

constexpr const char* PluginUri =
    "https://github.com/gianni-cappelletti/October-Production-Co/plugins/octobass";

enum class FileSlot : uint8_t
{
  Nam1 = 0,
  Nam2 = 1,
  CabinetIr = 2,
};
constexpr unsigned NumFileSlots = 3;
constexpr std::array<FileSlot, NumFileSlots> FileSlots{FileSlot::Nam1, FileSlot::Nam2,
                                                       FileSlot::CabinetIr};

struct FileParameter
{
  const char* uri;
  const char* label;
  const char* modFileTypes;
  const char* extension;
};

constexpr std::array<FileParameter, NumFileSlots> FileParameters{{
    {"https://github.com/gianni-cappelletti/October-Production-Co/plugins/octobass#nam1Model",
     "NAM 1 Model", "nammodel", ".nam"},
    {"https://github.com/gianni-cappelletti/October-Production-Co/plugins/octobass#nam2Model",
     "NAM 2 Model", "nammodel", ".nam"},
    {"https://github.com/gianni-cappelletti/October-Production-Co/plugins/octobass#cabinetIr",
     "Cabinet IR", "cabsim", ".wav"},
}};

constexpr const FileParameter& fileParameter(FileSlot slot)
{
  return FileParameters[static_cast<unsigned>(slot)];
}

}  // namespace octob::lv2
