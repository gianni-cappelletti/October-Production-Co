#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Ports.hpp"

namespace octob::lv2
{

enum class PortKind : uint8_t
{
  AudioInput,
  AudioOutput,
  AtomInput,
  AtomOutput,
  ControlInput,
  ControlOutput,
};

enum class PortUnit : uint8_t
{
  None,
  Hertz,
  Decibels,
  DecibelsU,
  Percent,
  Frames,
};

enum class PortStyle : uint8_t
{
  Linear,
  Logarithmic,
  Toggle,
  Enumeration,
};

enum class PortDesignation : uint8_t
{
  None,
  Control,
  Latency,
  Enabled,
};

enum class PortGroup : uint8_t
{
  None,
  Main,
  Equalizer,
  Nam1,
  Nam2,
};

struct ScalePoint
{
  std::string label;
  float value;
};

struct PortSpec
{
  PortIndex index;
  PortKind kind;
  std::string symbol;
  std::string name;
  float minimum = 0.0f;
  float maximum = 0.0f;
  float defaultValue = 0.0f;
  PortUnit unit = PortUnit::None;
  int displayDecimals = 1;
  PortStyle style = PortStyle::Linear;
  PortDesignation designation = PortDesignation::None;
  PortGroup group = PortGroup::None;
  std::vector<ScalePoint> scalePoints{};
};

const std::vector<PortSpec>& portTable();

const PortSpec& portSpec(PortIndex index);

}  // namespace octob::lv2
