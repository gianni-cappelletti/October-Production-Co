#include "PluginProcessor.h"

#include <algorithm>
#include <vector>

#include "LegacyEQBands.h"
#include "PluginEditor.h"

namespace
{

constexpr int kGraphicEQStateFormat = 2;

// Convert legacy fixed-band EQ state (eqBandGain0..23) into node parameters.
// The largest-magnitude gains win when more bands are set than node slots exist.
void migrateLegacyGraphicEQState(juce::ValueTree& state)
{
  if (static_cast<int>(state.getProperty("eqFormat", 1)) >= kGraphicEQStateFormat)
    return;

  struct LegacyBand
  {
    int band;
    float gainDb;
  };

  std::vector<LegacyBand> legacyBands;
  for (const auto& child : state)
  {
    auto id = child.getProperty("id").toString();
    if (id.startsWith("eqNode"))
    {
      DBG("EQ state already contains node parameters, skipping legacy migration");
      return;
    }

    if (id.startsWith("eqBandGain"))
    {
      int band = id.substring(10).getIntValue();
      float gainDb = static_cast<float>(child.getProperty("value", 0.0f));
      if (band >= 0 && band < legacyeq::kNumBands && std::fabs(gainDb) >= 0.05f)
        legacyBands.push_back({band, gainDb});
    }
  }

  // Mark the tree as current-format so a later restore of this same tree does
  // not rescan for legacy bands
  state.setProperty("eqFormat", kGraphicEQStateFormat, nullptr);

  if (legacyBands.empty())
    return;

  std::stable_sort(legacyBands.begin(), legacyBands.end(),
                   [](const LegacyBand& a, const LegacyBand& b)
                   { return std::fabs(a.gainDb) > std::fabs(b.gainDb); });

  if (legacyBands.size() > static_cast<size_t>(octob::kGraphicEQNumNodes))
  {
    DBG("Legacy EQ state has " + juce::String(legacyBands.size()) + " active bands, keeping the " +
        juce::String(octob::kGraphicEQNumNodes) + " largest");
    legacyBands.resize(static_cast<size_t>(octob::kGraphicEQNumNodes));
  }

  // Assign slots in ascending band order so node layout follows the frequency axis
  std::stable_sort(legacyBands.begin(), legacyBands.end(),
                   [](const LegacyBand& a, const LegacyBand& b) { return a.band < b.band; });

  auto setParam = [&state](const juce::String& id, float value)
  {
    juce::ValueTree param("PARAM");
    param.setProperty("id", id, nullptr);
    param.setProperty("value", value, nullptr);
    state.appendChild(param, nullptr);
  };

  for (size_t slot = 0; slot < legacyBands.size(); ++slot)
  {
    float freqHz = legacyeq::kCenterFreqs[static_cast<size_t>(legacyBands[slot].band)];
    freqHz = juce::jlimit(octob::MinGraphicEQFreqHz, octob::MaxGraphicEQFreqHz, freqHz);

    auto slotStr = juce::String(slot);
    setParam("eqNodeActive" + slotStr, 1.0f);
    setParam("eqNodeFreq" + slotStr, freqHz);
    setParam("eqNodeGain" + slotStr, legacyBands[slot].gainDb);
  }

  DBG("Migrated " + juce::String(legacyBands.size()) + " legacy EQ bands to nodes");
}

}  // namespace

OctoBassProcessor::OctoBassProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::mono(), true)
                         .withOutput("Output", juce::AudioChannelSet::mono(), true)),
      apvts_(*this, nullptr, "OctoBassParams", createParameterLayout())
{
  for (int i = 0; i < octob::kGraphicEQNumNodes; ++i)
  {
    auto idx = static_cast<size_t>(i);
    auto slot = juce::String(i);
    eqNodeParams_[idx].active = apvts_.getRawParameterValue("eqNodeActive" + slot);
    eqNodeParams_[idx].freq = apvts_.getRawParameterValue("eqNodeFreq" + slot);
    eqNodeParams_[idx].gain = apvts_.getRawParameterValue("eqNodeGain" + slot);
  }
  eqLowCutActiveParam_ = apvts_.getRawParameterValue("eqLowCutActive");
  eqLowCutFreqParam_ = apvts_.getRawParameterValue("eqLowCutFreq");
  eqHighCutActiveParam_ = apvts_.getRawParameterValue("eqHighCutActive");
  eqHighCutFreqParam_ = apvts_.getRawParameterValue("eqHighCutFreq");

  crossoverParam_ = apvts_.getRawParameterValue("crossoverFrequency");
  squashParam_ = apvts_.getRawParameterValue("squash");
  compressionModeParam_ = apvts_.getRawParameterValue("compressionMode");
  lowBandLevelParam_ = apvts_.getRawParameterValue("lowBandLevel");
  highInputGainParam_ = apvts_.getRawParameterValue("highInputGain");
  highOutputGainParam_ = apvts_.getRawParameterValue("highOutputGain");
  outputGainParam_ = apvts_.getRawParameterValue("outputGain");
  dryWetMixParam_ = apvts_.getRawParameterValue("dryWetMix");
  gateThresholdParam_ = apvts_.getRawParameterValue("gateThreshold");
  highBandMixParam_ = apvts_.getRawParameterValue("highBandMix");
  lowBandSoloParam_ = apvts_.getRawParameterValue("lowBandSolo");
  highBandSoloParam_ = apvts_.getRawParameterValue("highBandSolo");

  namQualityParam_ = apvts_.getRawParameterValue("namQuality");
  apvts_.addParameterListener("namQuality", this);
}

OctoBassProcessor::~OctoBassProcessor()
{
  apvts_.removeParameterListener("namQuality", this);
}

juce::AudioProcessorValueTreeState::ParameterLayout OctoBassProcessor::createParameterLayout()
{
  juce::AudioProcessorValueTreeState::ParameterLayout layout;

  layout.add(std::make_unique<juce::AudioParameterFloat>(
      "crossoverFrequency", "Crossover",
      juce::NormalisableRange<float>(octob::MinCrossoverFrequency, octob::MaxCrossoverFrequency,
                                     1.0f),
      octob::DefaultCrossoverFrequency, juce::String(),
      juce::AudioProcessorParameter::genericParameter,
      [](float value, int) { return juce::String(static_cast<int>(value)) + " Hz"; }));

  layout.add(std::make_unique<juce::AudioParameterFloat>(
      "squash", "Squash",
      juce::NormalisableRange<float>(octob::MinSquashAmount, octob::MaxSquashAmount, 0.01f),
      octob::DefaultSquashAmount, juce::String(), juce::AudioProcessorParameter::genericParameter,
      [](float value, int) { return juce::String(static_cast<int>(value * 100.0f)) + "%"; }));

  layout.add(std::make_unique<juce::AudioParameterChoice>(
      "compressionMode", "Compression Mode", juce::StringArray("Tight", "Smooth", "Punch", "Glue"),
      octob::DefaultCompressionMode));

  layout.add(std::make_unique<juce::AudioParameterFloat>(
      "lowBandLevel", "Low Level",
      juce::NormalisableRange<float>(octob::MinBandLevelDb, octob::MaxBandLevelDb, 0.1f),
      octob::DefaultBandLevelDb, juce::String(), juce::AudioProcessorParameter::genericParameter,
      [](float value, int) { return juce::String(value, 1) + " dB"; }));

  layout.add(std::make_unique<juce::AudioParameterFloat>(
      "highInputGain", "High Input Gain",
      juce::NormalisableRange<float>(octob::MinHighInputGainDb, octob::MaxHighInputGainDb, 0.1f),
      octob::DefaultHighInputGainDb, juce::String(),
      juce::AudioProcessorParameter::genericParameter,
      [](float value, int) { return juce::String(value, 1) + " dB"; }));

  layout.add(std::make_unique<juce::AudioParameterFloat>(
      "highOutputGain", "High Output Gain",
      juce::NormalisableRange<float>(octob::MinHighOutputGainDb, octob::MaxHighOutputGainDb, 0.1f),
      octob::DefaultHighOutputGainDb, juce::String(),
      juce::AudioProcessorParameter::genericParameter,
      [](float value, int) { return juce::String(value, 1) + " dB"; }));

  layout.add(std::make_unique<juce::AudioParameterFloat>(
      "outputGain", "Output Gain",
      juce::NormalisableRange<float>(octob::MinOutputGainDb, octob::MaxOutputGainDb, 0.1f),
      octob::DefaultOutputGainDb, juce::String(), juce::AudioProcessorParameter::genericParameter,
      [](float value, int) { return juce::String(value, 1) + " dB"; }));

  layout.add(std::make_unique<juce::AudioParameterFloat>(
      "dryWetMix", "Dry/Wet", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
      octob::DefaultDryWetMix, juce::String(), juce::AudioProcessorParameter::genericParameter,
      [](float value, int) { return juce::String(static_cast<int>(value * 100.0f)) + "%"; }));

  layout.add(std::make_unique<juce::AudioParameterFloat>(
      "gateThreshold", "Gate",
      juce::NormalisableRange<float>(octob::MinGateThresholdDb, octob::MaxGateThresholdDb, 0.1f),
      octob::DefaultGateThresholdDb, juce::String(),
      juce::AudioProcessorParameter::genericParameter, [](float value, int)
      { return value <= -90.0f ? juce::String("Off") : juce::String(value, 1) + " dB"; }));

  layout.add(std::make_unique<juce::AudioParameterFloat>(
      "highBandMix", "High Blend", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
      octob::DefaultHighBandMix, juce::String(), juce::AudioProcessorParameter::genericParameter,
      [](float value, int) { return juce::String(static_cast<int>(value * 100.0f)) + "%"; }));

  layout.add(std::make_unique<juce::AudioParameterBool>("lowBandSolo", "Low Solo", false));
  layout.add(std::make_unique<juce::AudioParameterBool>("highBandSolo", "High Solo", false));

  for (int i = 0; i < octob::kGraphicEQNumNodes; ++i)
  {
    auto slot = juce::String(i);
    auto name = "EQ Node " + juce::String(i + 1);

    layout.add(
        std::make_unique<juce::AudioParameterBool>("eqNodeActive" + slot, name + " Active", false));

    // Skew of 0.25 gives a log-like response so automation matches the log-spaced display
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "eqNodeFreq" + slot, name + " Freq",
        juce::NormalisableRange<float>(octob::MinGraphicEQFreqHz, octob::MaxGraphicEQFreqHz, 0.0f,
                                       0.25f),
        octob::DefaultGraphicEQFreqHz, juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float value, int)
        {
          return value >= 1000.0f ? juce::String(value / 1000.0f, 2) + " kHz"
                                  : juce::String(value, 1) + " Hz";
        }));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "eqNodeGain" + slot, name + " Gain",
        juce::NormalisableRange<float>(octob::MinGraphicEQGainDb, octob::MaxGraphicEQGainDb, 0.1f),
        octob::DefaultGraphicEQGainDb, juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { return juce::String(value, 1) + " dB"; }));
  }

  auto freqToText = [](float value, int)
  {
    return value >= 1000.0f ? juce::String(value / 1000.0f, 2) + " kHz"
                            : juce::String(value, 1) + " Hz";
  };
  auto freqRange = juce::NormalisableRange<float>(octob::MinGraphicEQFreqHz,
                                                  octob::MaxGraphicEQFreqHz, 0.0f, 0.25f);

  // Cuts default to active at the frequency extremes: audibly transparent there,
  // but visible in the display so users can find them
  layout.add(std::make_unique<juce::AudioParameterBool>("eqLowCutActive", "EQ Low Cut", true));
  layout.add(std::make_unique<juce::AudioParameterFloat>(
      "eqLowCutFreq", "EQ Low Cut Freq", freqRange, octob::MinGraphicEQFreqHz, juce::String(),
      juce::AudioProcessorParameter::genericParameter, freqToText));

  layout.add(std::make_unique<juce::AudioParameterBool>("eqHighCutActive", "EQ High Cut", true));
  layout.add(std::make_unique<juce::AudioParameterFloat>(
      "eqHighCutFreq", "EQ High Cut Freq", freqRange, octob::MaxGraphicEQFreqHz, juce::String(),
      juce::AudioProcessorParameter::genericParameter, freqToText));

  layout.add(std::make_unique<juce::AudioParameterFloat>(
      "namQuality", "NAM Quality", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
      octob::DefaultNamQuality, juce::String(), juce::AudioProcessorParameter::genericParameter,
      [](float value, int) { return juce::String(static_cast<int>(value * 100.0f)) + "%"; }));

  return layout;
}

void OctoBassProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
  bassProcessor_.setSampleRate(sampleRate);
  bassProcessor_.setMaxBlockSize(static_cast<size_t>(samplesPerBlock));
  spectrumFifo_.reset();
}

void OctoBassProcessor::releaseResources() {}

bool OctoBassProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
  return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono() &&
         layouts.getMainInputChannelSet() == juce::AudioChannelSet::mono();
}

void OctoBassProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                     juce::MidiBuffer& /*midiMessages*/)
{
  juce::ScopedNoDenormals noDenormals;
  bassProcessor_.setCrossoverFrequency(crossoverParam_->load());
  bassProcessor_.setSquash(squashParam_->load());
  bassProcessor_.setCompressionMode(static_cast<int>(compressionModeParam_->load()));
  bassProcessor_.setLowBandLevel(lowBandLevelParam_->load());
  bassProcessor_.setHighInputGain(highInputGainParam_->load());
  bassProcessor_.setHighOutputGain(highOutputGainParam_->load());
  bassProcessor_.setOutputGain(outputGainParam_->load());
  bassProcessor_.setDryWetMix(dryWetMixParam_->load());
  bassProcessor_.setGateThreshold(gateThresholdParam_->load());
  bassProcessor_.setHighBandMix(highBandMixParam_->load());

  for (int i = 0; i < octob::kGraphicEQNumNodes; ++i)
  {
    const auto& node = eqNodeParams_[static_cast<size_t>(i)];
    bassProcessor_.setGraphicEQNode(i, node.active->load() >= 0.5f, node.freq->load(),
                                    node.gain->load());
  }
  bassProcessor_.setGraphicEQLowCut(eqLowCutActiveParam_->load() >= 0.5f,
                                    eqLowCutFreqParam_->load());
  bassProcessor_.setGraphicEQHighCut(eqHighCutActiveParam_->load() >= 0.5f,
                                     eqHighCutFreqParam_->load());

  // Solo: enforce mutual exclusivity -- if both are on, the newly engaged one
  // wins. The losing parameter is corrected on the message thread because
  // setValueNotifyingHost is not real-time safe; this block already plays the
  // corrected combination so there is no audible gap.
  bool lowSolo = lowBandSoloParam_->load() >= 0.5f;
  bool highSolo = highBandSoloParam_->load() >= 0.5f;
  if (lowSolo && highSolo)
  {
    if (!prevLowSolo_)
    {
      pendingSoloCorrection_.store(SoloCorrection::ClearHigh, std::memory_order_release);
      highSolo = false;
    }
    else
    {
      pendingSoloCorrection_.store(SoloCorrection::ClearLow, std::memory_order_release);
      lowSolo = false;
    }
    triggerAsyncUpdate();
  }
  prevLowSolo_ = lowSolo;
  prevHighSolo_ = highSolo;
  bassProcessor_.setLowBandSolo(lowSolo);
  bassProcessor_.setHighBandSolo(highSolo);

  bassProcessor_.processMono(buffer.getReadPointer(0), buffer.getWritePointer(0),
                             static_cast<size_t>(buffer.getNumSamples()));

  // Push output samples into spectrum analyzer FIFO
  {
    const float* output = buffer.getReadPointer(0);
    const int numSamples = buffer.getNumSamples();
    const auto scope = spectrumFifo_.write(numSamples);

    if (scope.blockSize1 > 0)
      std::copy(output, output + scope.blockSize1, spectrumFifoBuffer_.data() + scope.startIndex1);

    if (scope.blockSize2 > 0)
      std::copy(output + scope.blockSize1, output + scope.blockSize1 + scope.blockSize2,
                spectrumFifoBuffer_.data() + scope.startIndex2);
  }

  if (bassProcessor_.getLatencySamples() != AudioProcessor::getLatencySamples())
    triggerAsyncUpdate();
}

juce::AudioProcessorEditor* OctoBassProcessor::createEditor()
{
  return new OctoBassEditor(*this);
}

bool OctoBassProcessor::hasEditor() const
{
  return true;
}

const juce::String OctoBassProcessor::getName() const
{
  return JucePlugin_Name;
}

bool OctoBassProcessor::acceptsMidi() const
{
  return false;
}

bool OctoBassProcessor::producesMidi() const
{
  return false;
}

bool OctoBassProcessor::isMidiEffect() const
{
  return false;
}

double OctoBassProcessor::getTailLengthSeconds() const
{
  const double sr = getSampleRate();
  if (sr <= 0.0)
    return 0.0;
  return static_cast<double>(bassProcessor_.getLatencySamples()) / sr;
}

int OctoBassProcessor::getNumPrograms()
{
  return 1;
}

int OctoBassProcessor::getCurrentProgram()
{
  return 0;
}

void OctoBassProcessor::setCurrentProgram(int /*index*/) {}

const juce::String OctoBassProcessor::getProgramName(int /*index*/)
{
  return {};
}

void OctoBassProcessor::changeProgramName(int /*index*/, const juce::String& /*newName*/) {}

bool OctoBassProcessor::loadNamModel(const juce::String& filepath, juce::String& errorMessage)
{
  std::string err;
  if (bassProcessor_.loadNamModel(filepath.toStdString(), err))
  {
    currentNamModelPath_ = filepath;
    DBG("Loaded NAM model: " + filepath);
    errorMessage.clear();
    return true;
  }
  DBG("Failed to load NAM model: " + juce::String(err));
  errorMessage = juce::String(err);
  return false;
}

void OctoBassProcessor::clearNamModel()
{
  bassProcessor_.clearNamModel();
  currentNamModelPath_.clear();
}

bool OctoBassProcessor::isNamModelLoaded() const
{
  return bassProcessor_.isNamModelLoaded();
}

juce::String OctoBassProcessor::getCurrentNamModelPath() const
{
  return currentNamModelPath_;
}

bool OctoBassProcessor::loadImpulseResponse(const juce::String& filepath,
                                            juce::String& errorMessage)
{
  std::string err;
  if (bassProcessor_.loadImpulseResponse(filepath.toStdString(), err))
  {
    currentIRPath_ = filepath;
    DBG("Loaded IR: " + filepath +
        " (Latency: " + juce::String(bassProcessor_.getLatencySamples()) + " samples)");
    triggerAsyncUpdate();
    errorMessage.clear();
    return true;
  }
  DBG("Failed to load IR: " + juce::String(err));
  errorMessage = juce::String(err);
  return false;
}

void OctoBassProcessor::clearImpulseResponse()
{
  bassProcessor_.clearImpulseResponse();
  currentIRPath_.clear();
  triggerAsyncUpdate();
}

bool OctoBassProcessor::isIRLoaded() const
{
  return bassProcessor_.isIRLoaded();
}

juce::String OctoBassProcessor::getCurrentIRPath() const
{
  return currentIRPath_;
}

int OctoBassProcessor::getLatencySamples() const
{
  return bassProcessor_.getLatencySamples();
}

void OctoBassProcessor::getStateInformation(juce::MemoryBlock& destData)
{
  auto state = apvts_.copyState();
  state.setProperty("eqFormat", kGraphicEQStateFormat, nullptr);

  if (currentIRPath_.isNotEmpty())
    state.setProperty("irPath", currentIRPath_, nullptr);

  if (currentNamModelPath_.isNotEmpty())
    state.setProperty("namModelPath", currentNamModelPath_, nullptr);

  std::unique_ptr<juce::XmlElement> xml(state.createXml());
  copyXmlToBinary(*xml, destData);
}

void OctoBassProcessor::setStateInformation(const void* data, int sizeInBytes)
{
  std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
  if (xmlState != nullptr && xmlState->hasTagName(apvts_.state.getType()))
  {
    auto state = juce::ValueTree::fromXml(*xmlState);
    migrateLegacyGraphicEQState(state);
    apvts_.replaceState(state);

    {
      const juce::SpinLock::ScopedLockType lock(pendingStateLock_);
      pendingState_ = state;
    }
    triggerAsyncUpdate();
  }
}

void OctoBassProcessor::parameterChanged(const juce::String& parameterID, float /*newValue*/)
{
  // Defer to the message thread: SetSlimmableSize is not real-time safe and
  // host automation can fire this callback from the audio thread
  if (parameterID == "namQuality")
  {
    namQualityDirty_.store(true, std::memory_order_release);
    triggerAsyncUpdate();
  }
}

void OctoBassProcessor::handleAsyncUpdate()
{
  setLatencySamples(bassProcessor_.getLatencySamples());

  if (namQualityDirty_.exchange(false, std::memory_order_acq_rel))
    bassProcessor_.setNamQuality(namQualityParam_->load());

  switch (pendingSoloCorrection_.exchange(SoloCorrection::None, std::memory_order_acq_rel))
  {
    case SoloCorrection::ClearLow:
      apvts_.getParameter("lowBandSolo")->setValueNotifyingHost(0.0f);
      break;
    case SoloCorrection::ClearHigh:
      apvts_.getParameter("highBandSolo")->setValueNotifyingHost(0.0f);
      break;
    case SoloCorrection::None:
      break;
  }

  juce::ValueTree state;
  {
    const juce::SpinLock::ScopedLockType lock(pendingStateLock_);
    state = pendingState_;
    pendingState_ = juce::ValueTree();
  }

  if (!state.isValid())
    return;

  // Host state is untrusted: only restore paths that look like the files we
  // saved (absolute, expected extension, present on disk)
  auto restorableFile = [](const juce::String& path, const char* extension) -> juce::File
  {
    if (path.isEmpty() || !juce::File::isAbsolutePath(path))
      return {};
    juce::File file(path);
    if (!file.hasFileExtension(extension) || !file.existsAsFile())
    {
      DBG("Ignoring restored path with wrong extension or missing file: " + path);
      return {};
    }
    return file;
  };

  if (auto irFile = restorableFile(state.getProperty("irPath").toString(), "wav");
      irFile != juce::File())
  {
    juce::String err;
    loadImpulseResponse(irFile.getFullPathName(), err);
  }

  if (auto namFile = restorableFile(state.getProperty("namModelPath").toString(), "nam");
      namFile != juce::File())
  {
    juce::String err;
    loadNamModel(namFile.getFullPathName(), err);
  }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
  return new OctoBassProcessor();
}
