#pragma once

#include <cstdint>
#include <octobass-core/Types.hpp>

namespace octob::lv2
{

using PortIndex = uint32_t;

namespace port
{

constexpr PortIndex AudioIn = 0;
constexpr PortIndex AudioOut = 1;
constexpr PortIndex Control = 2;
constexpr PortIndex Notify = 3;
constexpr PortIndex Latency = 4;
constexpr PortIndex Enabled = 5;

constexpr PortIndex Crossover = 6;
constexpr PortIndex Squash = 7;
constexpr PortIndex CompressionMode = 8;
constexpr PortIndex LowBandLevel = 9;
constexpr PortIndex HighInputGain = 10;
constexpr PortIndex HighOutputGain = 11;
constexpr PortIndex OutputGain = 12;
constexpr PortIndex DryWet = 13;
constexpr PortIndex GateThreshold = 14;
constexpr PortIndex HighBandMix = 15;
constexpr PortIndex LowBandSolo = 16;
constexpr PortIndex HighBandSolo = 17;

constexpr PortIndex EqLowCutActive = 18;
constexpr PortIndex EqLowCutFreq = 19;
constexpr PortIndex EqHighCutActive = 20;
constexpr PortIndex EqHighCutFreq = 21;

constexpr PortIndex EqNodeFirst = 22;
constexpr PortIndex PortsPerEqNode = 3;
constexpr PortIndex eqNodeActive(int node)
{
  return EqNodeFirst + PortsPerEqNode * static_cast<PortIndex>(node);
}
constexpr PortIndex eqNodeFreq(int node)
{
  return eqNodeActive(node) + 1;
}
constexpr PortIndex eqNodeGain(int node)
{
  return eqNodeActive(node) + 2;
}

constexpr PortIndex NamFirst = eqNodeActive(kGraphicEQNumNodes);
constexpr PortIndex PortsPerNamSlot = 5;
constexpr PortIndex namOn(NamSlot slot)
{
  return NamFirst + PortsPerNamSlot * static_cast<PortIndex>(slot);
}
constexpr PortIndex namQuality(NamSlot slot)
{
  return namOn(slot) + 1;
}
constexpr PortIndex namCalibrateInput(NamSlot slot)
{
  return namOn(slot) + 2;
}
constexpr PortIndex namInputLevel(NamSlot slot)
{
  return namOn(slot) + 3;
}
constexpr PortIndex namOutputMode(NamSlot slot)
{
  return namOn(slot) + 4;
}

constexpr PortIndex NumPorts = NamFirst + PortsPerNamSlot * kNumNamSlots;

}  // namespace port

constexpr float DefaultNamQualityPercent = 49.0f;

constexpr float BypassRampSeconds = 0.01f;

}  // namespace octob::lv2
