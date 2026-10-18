#include "ReferenceChain.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "PortTable.hpp"

namespace octob::lv2::test
{

namespace
{

bool isOn(float value)
{
  return value >= 0.5f;
}

int choice(float value)
{
  return static_cast<int>(std::lround(value));
}

}  // namespace

ReferenceChain::ReferenceChain(double sampleRate, size_t maxBlockSize) : maxBlockSize_(maxBlockSize)
{
  processor_.setSampleRate(sampleRate);
  processor_.setMaxBlockSize(maxBlockSize);
  processor_.setGraphicEQLowCut(true, MinGraphicEQFreqHz);
  processor_.setGraphicEQHighCut(true, MaxGraphicEQFreqHz);
  for (const auto slot : kNamSlots)
    setNamQualityPercent(slot, DefaultNamQualityPercent);
}

void ReferenceChain::loadNamModel(NamSlot slot, const std::string& path)
{
  std::string error;
  if (!processor_.loadNamModel(slot, path, error))
    throw std::runtime_error("Reference failed to load " + path + ": " + error);
}

void ReferenceChain::clearNamModel(NamSlot slot)
{
  processor_.clearNamModel(slot);
}

void ReferenceChain::loadImpulseResponse(const std::string& path)
{
  std::string error;
  if (!processor_.loadImpulseResponse(path, error))
    throw std::runtime_error("Reference failed to load " + path + ": " + error);
}

void ReferenceChain::clearImpulseResponse()
{
  processor_.clearImpulseResponse();
}

void ReferenceChain::setNamQualityPercent(NamSlot slot, float percent)
{
  processor_.setNamQuality(slot, percent / 100.0f);
}

std::vector<float> ReferenceChain::process(const std::vector<float>& input, size_t blockSize,
                                           const ControlValues& controls)
{
  const size_t chunk = std::min(blockSize, maxBlockSize_);
  std::vector<float> output(input.size());
  for (size_t offset = 0; offset < input.size(); offset += chunk)
  {
    const size_t frames = std::min(chunk, input.size() - offset);
    applyControls(controls);
    processor_.processMono(input.data() + offset, output.data() + offset, frames);
  }
  return output;
}

void ReferenceChain::applyControls(const ControlValues& controls)
{
  processor_.setCrossoverFrequency(controls[port::Crossover]);
  processor_.setSquash(controls[port::Squash] / 100.0f);
  processor_.setCompressionMode(choice(controls[port::CompressionMode]));
  processor_.setLowBandLevel(controls[port::LowBandLevel]);
  processor_.setHighInputGain(controls[port::HighInputGain]);
  processor_.setHighOutputGain(controls[port::HighOutputGain]);
  processor_.setOutputGain(controls[port::OutputGain]);
  processor_.setDryWetMix(controls[port::DryWet] / 100.0f);
  processor_.setGateThreshold(controls[port::GateThreshold]);
  processor_.setHighBandMix(controls[port::HighBandMix] / 100.0f);

  for (const auto slot : kNamSlots)
  {
    processor_.setNamCalibrateInput(slot, isOn(controls[port::namCalibrateInput(slot)]));
    processor_.setNamInputCalibrationLevel(slot, controls[port::namInputLevel(slot)]);
    processor_.setNamOutputMode(slot, choice(controls[port::namOutputMode(slot)]));
    processor_.setNamEnabled(slot, isOn(controls[port::namOn(slot)]));
  }

  processor_.setLowBandSolo(isOn(controls[port::LowBandSolo]));
  processor_.setHighBandSolo(isOn(controls[port::HighBandSolo]));
}

ControlValues defaultControls()
{
  ControlValues controls{};
  for (const auto& spec : portTable())
    controls[spec.index] = spec.defaultValue;
  return controls;
}

std::vector<float> generateBassSignal(double sampleRate, double seconds)
{
  const auto length = static_cast<size_t>(sampleRate * seconds);
  std::vector<float> signal(length);
  for (size_t i = 0; i < length; ++i)
  {
    const double t = static_cast<double>(i) / sampleRate;
    const double envelope = 0.6 + 0.4 * std::sin(2.0 * Pi * 1.5 * t);
    signal[i] = static_cast<float>(envelope * (0.25 * std::sin(2.0 * Pi * 55.0 * t) +
                                               0.08 * std::sin(2.0 * Pi * 220.0 * t) +
                                               0.03 * std::sin(2.0 * Pi * 1760.0 * t)));
  }
  return signal;
}

std::string testDataPath(const std::string& fileName)
{
  return std::string(TEST_DATA_DIR) + "/" + fileName;
}

}  // namespace octob::lv2::test
