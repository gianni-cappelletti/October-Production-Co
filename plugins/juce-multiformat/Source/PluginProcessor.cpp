#include "PluginProcessor.h"

#include "PluginEditor.h"

OctobIRProcessor::OctobIRProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withInput("Sidechain", juce::AudioChannelSet::stereo(), false)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts_(*this, nullptr, "Parameters", createParameterLayout())
{
}

OctobIRProcessor::~OctobIRProcessor()
{
  cancelPendingUpdate();
}

juce::AudioProcessorValueTreeState::ParameterLayout OctobIRProcessor::createParameterLayout()
{
  juce::AudioProcessorValueTreeState::ParameterLayout layout;

  layout.add(std::make_unique<juce::AudioParameterBool>("irAEnable", "IR A Enable", false));

  layout.add(std::make_unique<juce::AudioParameterBool>("irBEnable", "IR B Enable", false));

  layout.add(std::make_unique<juce::AudioParameterBool>("dynamicMode", "Dynamic Mode", false));

  layout.add(
      std::make_unique<juce::AudioParameterBool>("sidechainEnable", "Sidechain Enable", false));

  layout.add(std::make_unique<juce::AudioParameterFloat>(
      "blend", "Static Blend", juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.0f,
      juce::String(), juce::AudioProcessorParameter::genericParameter,
      [](float value, int) { return juce::String(value, 2); }));

  layout.add(std::make_unique<juce::AudioParameterFloat>(
      "threshold", "Threshold", juce::NormalisableRange<float>(-60.0f, 0.0f, 0.1f), -30.0f,
      juce::String(), juce::AudioProcessorParameter::genericParameter,
      [](float value, int) { return juce::String(value, 1) + " dB"; }));

  layout.add(std::make_unique<juce::AudioParameterFloat>(
      "rangeDb", "Range", juce::NormalisableRange<float>(1.0f, 60.0f, 0.1f), 20.0f, juce::String(),
      juce::AudioProcessorParameter::genericParameter,
      [](float value, int) { return juce::String(value, 1) + " dB"; }));

  layout.add(std::make_unique<juce::AudioParameterFloat>(
      "kneeWidthDb", "Knee Width", juce::NormalisableRange<float>(0.0f, 20.0f, 0.1f), 5.0f,
      juce::String(), juce::AudioProcessorParameter::genericParameter,
      [](float value, int) { return juce::String(value, 1) + " dB"; }));

  layout.add(std::make_unique<juce::AudioParameterChoice>("detectionMode", "Detection Mode",
                                                          juce::StringArray("Peak", "RMS"), 0));

  layout.add(std::make_unique<juce::AudioParameterFloat>(
      "attackTime", "Attack Time", juce::NormalisableRange<float>(1.0f, 500.0f, 1.0f), 50.0f,
      juce::String(), juce::AudioProcessorParameter::genericParameter,
      [](float value, int) { return juce::String(static_cast<int>(value)) + " ms"; }));

  layout.add(std::make_unique<juce::AudioParameterFloat>(
      "releaseTime", "Release Time", juce::NormalisableRange<float>(1.0f, 1000.0f, 1.0f), 200.0f,
      juce::String(), juce::AudioProcessorParameter::genericParameter,
      [](float value, int) { return juce::String(static_cast<int>(value)) + " ms"; }));

  layout.add(std::make_unique<juce::AudioParameterFloat>(
      "outputGain", "Output Gain", juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f,
      juce::String(), juce::AudioProcessorParameter::genericParameter,
      [](float value, int) { return juce::String(value, 1) + " dB"; }));

  layout.add(std::make_unique<juce::AudioParameterFloat>(
      "irATrimGain", "IR A Trim", juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f,
      juce::String(), juce::AudioProcessorParameter::genericParameter,
      [](float value, int) { return juce::String(value, 1) + " dB"; }));

  layout.add(std::make_unique<juce::AudioParameterFloat>(
      "irBTrimGain", "IR B Trim", juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f,
      juce::String(), juce::AudioProcessorParameter::genericParameter,
      [](float value, int) { return juce::String(value, 1) + " dB"; }));

  return layout;
}

const juce::String OctobIRProcessor::getName() const
{
  return JucePlugin_Name;
}

bool OctobIRProcessor::acceptsMidi() const
{
  return false;
}

bool OctobIRProcessor::producesMidi() const
{
  return false;
}

bool OctobIRProcessor::isMidiEffect() const
{
  return false;
}

double OctobIRProcessor::getTailLengthSeconds() const
{
  const double sr = getSampleRate();
  if (sr <= 0.0)
    return 0.0;
  const size_t maxSamples =
      std::max(irProcessor_.getIR1NumSamples(), irProcessor_.getIR2NumSamples());
  return static_cast<double>(maxSamples) / sr;
}

int OctobIRProcessor::getNumPrograms()
{
  return 1;
}

int OctobIRProcessor::getCurrentProgram()
{
  return 0;
}

void OctobIRProcessor::setCurrentProgram(int index)
{
  juce::ignoreUnused(index);
}

const juce::String OctobIRProcessor::getProgramName(int index)
{
  juce::ignoreUnused(index);
  return {};
}

void OctobIRProcessor::changeProgramName(int index, const juce::String& newName)
{
  juce::ignoreUnused(index, newName);
}

void OctobIRProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
  irProcessor_.setSampleRate(sampleRate);
  irProcessor_.setMaxBlockSize(static_cast<octob::FrameCount>(samplesPerBlock));
}

void OctobIRProcessor::releaseResources()
{
  irProcessor_.reset();
}

bool OctobIRProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
  auto inputSet = layouts.getMainInputChannelSet();
  auto outputSet = layouts.getMainOutputChannelSet();

  if (outputSet != juce::AudioChannelSet::mono() && outputSet != juce::AudioChannelSet::stereo())
    return false;

  if (inputSet != outputSet &&
      !(inputSet == juce::AudioChannelSet::mono() && outputSet == juce::AudioChannelSet::stereo()))
    return false;

  auto sidechain = layouts.getChannelSet(true, 1);
  if (!sidechain.isDisabled() && sidechain != juce::AudioChannelSet::mono() &&
      sidechain != juce::AudioChannelSet::stereo())
    return false;

  return true;
}

void OctobIRProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                    juce::MidiBuffer& midiMessages)
{
  juce::ignoreUnused(midiMessages);
  juce::ScopedNoDenormals noDenormals;

  auto totalNumInputChannels = getTotalNumInputChannels();
  auto totalNumOutputChannels = getTotalNumOutputChannels();

  for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
    buffer.clear(i, 0, buffer.getNumSamples());

  bool irAEnable = apvts_.getRawParameterValue("irAEnable")->load() > 0.5f;
  bool irBEnable = apvts_.getRawParameterValue("irBEnable")->load() > 0.5f;
  bool dynamicMode = apvts_.getRawParameterValue("dynamicMode")->load() > 0.5f;
  bool sidechainEnabled = apvts_.getRawParameterValue("sidechainEnable")->load() > 0.5f;
  float blend = apvts_.getRawParameterValue("blend")->load();
  float threshold = apvts_.getRawParameterValue("threshold")->load();
  float rangeDb = apvts_.getRawParameterValue("rangeDb")->load();
  float kneeWidthDb = apvts_.getRawParameterValue("kneeWidthDb")->load();
  int detectionMode = static_cast<int>(apvts_.getRawParameterValue("detectionMode")->load());
  float attackTime = apvts_.getRawParameterValue("attackTime")->load();
  float releaseTime = apvts_.getRawParameterValue("releaseTime")->load();
  float outputGain = apvts_.getRawParameterValue("outputGain")->load();
  float irATrimGain = apvts_.getRawParameterValue("irATrimGain")->load();
  float irBTrimGain = apvts_.getRawParameterValue("irBTrimGain")->load();

  irProcessor_.setIRAEnabled(irAEnable);
  irProcessor_.setIRBEnabled(irBEnable);
  irProcessor_.setDynamicModeEnabled(dynamicMode);
  irProcessor_.setSidechainEnabled(sidechainEnabled);
  irProcessor_.setBlend(blend);
  irProcessor_.setThreshold(threshold);
  irProcessor_.setRangeDb(rangeDb);
  irProcessor_.setKneeWidthDb(kneeWidthDb);
  irProcessor_.setDetectionMode(detectionMode);
  irProcessor_.setAttackTime(attackTime);
  irProcessor_.setReleaseTime(releaseTime);
  irProcessor_.setOutputGain(outputGain);
  irProcessor_.setIRATrimGain(irATrimGain);
  irProcessor_.setIRBTrimGain(irBTrimGain);

  auto mainInputChannels = getBusBuffer(buffer, true, 0);
  auto sidechainBuffer = getBusBuffer(buffer, true, 1);
  bool hasSidechain = sidechainBuffer.getNumChannels() != 0;

  int numInputChannels = mainInputChannels.getNumChannels();
  bool monoToStereo = numInputChannels == 1 && totalNumOutputChannels >= 2;

  if (dynamicMode && sidechainEnabled && hasSidechain)
  {
    if (numInputChannels >= 2 && totalNumOutputChannels >= 2)
    {
      float* mainL = mainInputChannels.getWritePointer(0);
      float* mainR = mainInputChannels.getWritePointer(1);
      const float* scL =
          sidechainBuffer.getNumChannels() >= 1 ? sidechainBuffer.getReadPointer(0) : mainL;
      const float* scR =
          sidechainBuffer.getNumChannels() >= 2 ? sidechainBuffer.getReadPointer(1) : scL;

      float* outL = buffer.getWritePointer(0);
      float* outR = buffer.getWritePointer(1);

      irProcessor_.processStereoWithSidechain(mainL, mainR, scL, scR, outL, outR,
                                              static_cast<size_t>(buffer.getNumSamples()));
    }
    else if (monoToStereo)
    {
      const float* main = mainInputChannels.getReadPointer(0);
      const float* sc =
          sidechainBuffer.getNumChannels() >= 1 ? sidechainBuffer.getReadPointer(0) : main;
      float* outL = buffer.getWritePointer(0);
      float* outR = buffer.getWritePointer(1);

      irProcessor_.processMonoToStereoWithSidechain(main, sc, outL, outR,
                                                    static_cast<size_t>(buffer.getNumSamples()));
    }
    else if (numInputChannels >= 1 && totalNumOutputChannels >= 1)
    {
      float* main = mainInputChannels.getWritePointer(0);
      const float* sc =
          sidechainBuffer.getNumChannels() >= 1 ? sidechainBuffer.getReadPointer(0) : main;
      float* out = buffer.getWritePointer(0);

      irProcessor_.processMonoWithSidechain(main, sc, out,
                                            static_cast<size_t>(buffer.getNumSamples()));
    }
  }
  else
  {
    if (numInputChannels >= 2 && totalNumOutputChannels >= 2)
    {
      float* channelDataL = buffer.getWritePointer(0);
      float* channelDataR = buffer.getWritePointer(1);
      irProcessor_.processStereo(channelDataL, channelDataR, channelDataL, channelDataR,
                                 static_cast<size_t>(buffer.getNumSamples()));
    }
    else if (monoToStereo)
    {
      const float* inputData = mainInputChannels.getReadPointer(0);
      float* outL = buffer.getWritePointer(0);
      float* outR = buffer.getWritePointer(1);
      irProcessor_.processMonoToStereo(inputData, outL, outR,
                                       static_cast<size_t>(buffer.getNumSamples()));
    }
    else if (numInputChannels >= 1 && totalNumOutputChannels >= 1)
    {
      float* channelData = buffer.getWritePointer(0);
      irProcessor_.processMono(channelData, channelData,
                               static_cast<size_t>(buffer.getNumSamples()));
    }
  }

  if (irProcessor_.getLatencySamples() != getLatencySamples())
    triggerAsyncUpdate();
}

bool OctobIRProcessor::hasEditor() const
{
  return true;
}

juce::AudioProcessorEditor* OctobIRProcessor::createEditor()
{
  return new OctobIREditor(*this);
}

void OctobIRProcessor::getStateInformation(juce::MemoryBlock& destData)
{
  auto state = apvts_.copyState();
  state.setProperty("ir1Path", currentIR1Path_, nullptr);
  state.setProperty("ir2Path", currentIR2Path_, nullptr);
  state.setProperty("editorWidth", lastEditorWidth_.load(), nullptr);
  state.setProperty("editorHeight", lastEditorHeight_.load(), nullptr);

  juce::MemoryOutputStream stream(destData, false);
  state.writeToStream(stream);
}

void OctobIRProcessor::setStateInformation(const void* data, int sizeInBytes)
{
  juce::ValueTree state = juce::ValueTree::readFromData(data, static_cast<size_t>(sizeInBytes));

  if (state.isValid())
  {
    apvts_.replaceState(state);

    lastEditorWidth_.store(static_cast<int>(state.getProperty("editorWidth", 580)));
    lastEditorHeight_.store(static_cast<int>(state.getProperty("editorHeight", 694)));

    if (auto* editor = getActiveEditor())
      editor->setSize(lastEditorWidth_.load(), lastEditorHeight_.load());

    {
      const juce::SpinLock::ScopedLockType lock(pendingStateLock_);
      pendingState_ = state;
    }
    triggerAsyncUpdate();
  }
}

void OctobIRProcessor::handleAsyncUpdate()
{
  setLatencySamples(irProcessor_.getLatencySamples());

  juce::ValueTree state;
  {
    const juce::SpinLock::ScopedLockType lock(pendingStateLock_);
    state = pendingState_;
    pendingState_ = juce::ValueTree();
  }

  if (!state.isValid())
    return;

  if (!state.hasProperty("threshold") && state.hasProperty("lowThreshold"))
  {
    float oldLow = state.getProperty("lowThreshold", -40.0f);
    float oldHigh = state.getProperty("highThreshold", -10.0f);

    float newThreshold = oldLow;
    float newRange = std::max(1.0f, oldHigh - oldLow);

    if (auto* param = apvts_.getParameter("threshold"))
    {
      param->setValueNotifyingHost(param->convertTo0to1(newThreshold));
    }
    if (auto* param = apvts_.getParameter("rangeDb"))
    {
      param->setValueNotifyingHost(param->convertTo0to1(newRange));
    }
  }

  juce::String path = state.getProperty("ir1Path").toString();
  if (path.isNotEmpty())
  {
    juce::String error;
    loadImpulseResponse1(path, error);
  }

  juce::String path2 = state.getProperty("ir2Path").toString();
  if (path2.isNotEmpty())
  {
    juce::String error;
    loadImpulseResponse2(path2, error);
  }
}

bool OctobIRProcessor::loadImpulseResponse1(const juce::String& filepath,
                                            juce::String& errorMessage)
{
  std::string error;
  if (irProcessor_.loadImpulseResponse1(filepath.toStdString(), error))
  {
    currentIR1Path_ = filepath;
    DBG("Loaded IR1: " + filepath + " (Latency: " + juce::String(irProcessor_.getLatencySamples()) +
        " samples)");

    if (auto* param = apvts_.getParameter("irAEnable"))
    {
      param->setValueNotifyingHost(1.0f);
    }

    errorMessage.clear();
    return true;
  }
  else
  {
    DBG("Failed to load IR1: " + juce::String(error));
    errorMessage = juce::String(error);
    return false;
  }
}

bool OctobIRProcessor::loadImpulseResponse2(const juce::String& filepath,
                                            juce::String& errorMessage)
{
  std::string error;
  if (irProcessor_.loadImpulseResponse2(filepath.toStdString(), error))
  {
    currentIR2Path_ = filepath;
    DBG("Loaded IR2: " + filepath + " (Latency: " + juce::String(irProcessor_.getLatencySamples()) +
        " samples)");

    if (auto* param = apvts_.getParameter("irBEnable"))
    {
      param->setValueNotifyingHost(1.0f);
    }

    errorMessage.clear();
    return true;
  }
  else
  {
    DBG("Failed to load IR2: " + juce::String(error));
    errorMessage = juce::String(error);
    return false;
  }
}

void OctobIRProcessor::clearImpulseResponse1()
{
  irProcessor_.clearImpulseResponse1();
  currentIR1Path_.clear();

  if (auto* param = apvts_.getParameter("irAEnable"))
  {
    param->setValueNotifyingHost(0.0f);
  }

  DBG("Cleared IR1");
}

void OctobIRProcessor::clearImpulseResponse2()
{
  irProcessor_.clearImpulseResponse2();
  currentIR2Path_.clear();

  if (auto* param = apvts_.getParameter("irBEnable"))
  {
    param->setValueNotifyingHost(0.0f);
  }

  DBG("Cleared IR2");
}

void OctobIRProcessor::swapImpulseResponses()
{
  const float irAEnabled = apvts_.getRawParameterValue("irAEnable")->load();
  const float irBEnabled = apvts_.getRawParameterValue("irBEnable")->load();
  const float trimA = apvts_.getRawParameterValue("irATrimGain")->load();
  const float trimB = apvts_.getRawParameterValue("irBTrimGain")->load();

  const juce::String path1 = currentIR1Path_;
  const juce::String path2 = currentIR2Path_;

  DBG("Swapping IRs: slot1=" + path1 + " slot2=" + path2);

  if (path2.isNotEmpty())
  {
    juce::String err;
    loadImpulseResponse1(path2, err);
  }
  else
    clearImpulseResponse1();

  if (path1.isNotEmpty())
  {
    juce::String err;
    loadImpulseResponse2(path1, err);
  }
  else
    clearImpulseResponse2();

  if (auto* param = apvts_.getParameter("irAEnable"))
    param->setValueNotifyingHost(irAEnabled);
  if (auto* param = apvts_.getParameter("irBEnable"))
    param->setValueNotifyingHost(irBEnabled);

  // Swap trim gains so each IR retains its original trim after changing slots.
  if (auto* param = apvts_.getParameter("irATrimGain"))
    param->setValueNotifyingHost(param->convertTo0to1(trimB));
  if (auto* param = apvts_.getParameter("irBTrimGain"))
    param->setValueNotifyingHost(param->convertTo0to1(trimA));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
  return new OctobIRProcessor();
}
