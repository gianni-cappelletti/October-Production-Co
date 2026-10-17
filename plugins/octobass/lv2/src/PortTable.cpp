#include "PortTable.hpp"

#include <octobass-core/NamProcessor.hpp>
#include <string>

namespace octob::lv2
{

namespace
{

PortSpec control(PortIndex index, std::string symbol, std::string name, float minimum,
                 float maximum, float defaultValue, PortUnit unit, PortStyle style, PortGroup group)
{
  PortSpec spec{index, PortKind::ControlInput, std::move(symbol), std::move(name)};
  spec.minimum = minimum;
  spec.maximum = maximum;
  spec.defaultValue = defaultValue;
  spec.unit = unit;
  spec.displayDecimals = unit == PortUnit::Percent ? 0 : 1;
  spec.style = style;
  spec.group = group;
  return spec;
}

PortSpec toggle(PortIndex index, std::string symbol, std::string name, bool defaultOn,
                PortGroup group)
{
  return control(index, std::move(symbol), std::move(name), 0.0f, 1.0f, defaultOn ? 1.0f : 0.0f,
                 PortUnit::None, PortStyle::Toggle, group);
}

PortSpec enumeration(PortIndex index, std::string symbol, std::string name,
                     std::vector<std::string> labels, int defaultIndex, PortGroup group)
{
  PortSpec spec = control(index, std::move(symbol), std::move(name), 0.0f,
                          static_cast<float>(labels.size() - 1), static_cast<float>(defaultIndex),
                          PortUnit::None, PortStyle::Enumeration, group);
  for (size_t i = 0; i < labels.size(); ++i)
    spec.scalePoints.push_back({std::move(labels[i]), static_cast<float>(i)});
  return spec;
}

std::vector<PortSpec> buildPortTable()
{
  std::vector<PortSpec> ports;
  ports.reserve(port::NumPorts);

  ports.push_back({port::AudioIn, PortKind::AudioInput, "in", "In"});
  ports.push_back({port::AudioOut, PortKind::AudioOutput, "out", "Out"});

  PortSpec controlPort{port::Control, PortKind::AtomInput, "control", "Control"};
  controlPort.designation = PortDesignation::Control;
  ports.push_back(controlPort);

  PortSpec notifyPort{port::Notify, PortKind::AtomOutput, "notify", "Notify"};
  notifyPort.designation = PortDesignation::Control;
  ports.push_back(notifyPort);

  PortSpec latencyPort{port::Latency, PortKind::ControlOutput, "latency", "Latency"};
  latencyPort.maximum = 192000.0f;
  latencyPort.unit = PortUnit::Frames;
  latencyPort.designation = PortDesignation::Latency;
  ports.push_back(latencyPort);

  PortSpec enabledPort = toggle(port::Enabled, "enabled", "Enabled", true, PortGroup::None);
  enabledPort.designation = PortDesignation::Enabled;
  ports.push_back(enabledPort);

  const auto main = PortGroup::Main;
  PortSpec crossover = control(port::Crossover, "crossover", "Crossover", MinCrossoverFrequency,
                               MaxCrossoverFrequency, DefaultCrossoverFrequency, PortUnit::Hertz,
                               PortStyle::Linear, main);
  crossover.displayDecimals = 0;
  ports.push_back(crossover);
  ports.push_back(control(port::Squash, "squash", "Squash", MinSquashAmount * 100.0f,
                          MaxSquashAmount * 100.0f, DefaultSquashAmount * 100.0f, PortUnit::Percent,
                          PortStyle::Linear, main));
  ports.push_back(enumeration(port::CompressionMode, "compression_mode", "Compression Mode",
                              {"Tight", "Smooth", "Punch", "Glue"}, DefaultCompressionMode, main));
  ports.push_back(control(port::LowBandLevel, "low_level", "Low Level", MinBandLevelDb,
                          MaxBandLevelDb, DefaultBandLevelDb, PortUnit::Decibels, PortStyle::Linear,
                          main));
  ports.push_back(control(port::HighInputGain, "high_input_gain", "High Input Gain",
                          MinHighInputGainDb, MaxHighInputGainDb, DefaultHighInputGainDb,
                          PortUnit::Decibels, PortStyle::Linear, main));
  ports.push_back(control(port::HighOutputGain, "high_output_gain", "High Output Gain",
                          MinHighOutputGainDb, MaxHighOutputGainDb, DefaultHighOutputGainDb,
                          PortUnit::Decibels, PortStyle::Linear, main));
  ports.push_back(control(port::OutputGain, "output_gain", "Output Gain", MinOutputGainDb,
                          MaxOutputGainDb, DefaultOutputGainDb, PortUnit::Decibels,
                          PortStyle::Linear, main));
  ports.push_back(control(port::DryWet, "dry_wet", "Dry/Wet", 0.0f, 100.0f,
                          DefaultDryWetMix * 100.0f, PortUnit::Percent, PortStyle::Linear, main));
  PortSpec gate =
      control(port::GateThreshold, "gate", "Gate", MinGateThresholdDb, MaxGateThresholdDb,
              DefaultGateThresholdDb, PortUnit::Decibels, PortStyle::Linear, main);
  gate.scalePoints.push_back({"Off", MinGateThresholdDb});
  ports.push_back(gate);
  ports.push_back(control(port::HighBandMix, "high_blend", "High Blend", 0.0f, 100.0f,
                          DefaultHighBandMix * 100.0f, PortUnit::Percent, PortStyle::Linear, main));
  ports.push_back(toggle(port::LowBandSolo, "low_solo", "Low Solo", false, main));
  ports.push_back(toggle(port::HighBandSolo, "high_solo", "High Solo", false, main));

  for (const auto slot : kNamSlots)
  {
    const int slotNumber = static_cast<int>(slot) + 1;
    const std::string symbol = "nam" + std::to_string(slotNumber);
    const std::string name = "NAM " + std::to_string(slotNumber);
    const auto group = slot == NamSlot::First ? PortGroup::Nam1 : PortGroup::Nam2;

    ports.push_back(toggle(port::namOn(slot), symbol + "_on", name + " On", true, group));
    ports.push_back(control(port::namQuality(slot), symbol + "_quality", name + " Quality", 0.0f,
                            100.0f, DefaultNamQualityPercent, PortUnit::Percent, PortStyle::Linear,
                            group));
    PortSpec calibrateInput = toggle(port::namCalibrateInput(slot), symbol + "_calibrate_input",
                                     name + " Calibrate Input", false, group);
    calibrateInput.shortName = name + " Calibrate";
    ports.push_back(calibrateInput);
    PortSpec inputLevel =
        control(port::namInputLevel(slot), symbol + "_input_level", name + " Input Level",
                MinNamInputCalibrationLevelDbu, MaxNamInputCalibrationLevelDbu,
                DefaultNamInputCalibrationLevelDbu, PortUnit::DecibelsU, PortStyle::Linear, group);
    inputLevel.shortName = name + " Input Lvl";
    ports.push_back(inputLevel);
    PortSpec outputMode = enumeration(port::namOutputMode(slot), symbol + "_output_mode",
                                      name + " Output Mode", {"Raw", "Normalized", "Calibrated"},
                                      static_cast<int>(NamOutputMode::Normalized), group);
    outputMode.shortName = name + " Out Mode";
    ports.push_back(outputMode);
  }

  return ports;
}

}  // namespace

const std::vector<PortSpec>& portTable()
{
  static const std::vector<PortSpec> table = buildPortTable();
  return table;
}

const PortSpec& portSpec(PortIndex index)
{
  return portTable().at(index);
}

}  // namespace octob::lv2
