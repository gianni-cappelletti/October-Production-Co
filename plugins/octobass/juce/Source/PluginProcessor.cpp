#include "PluginProcessor.h"

#include "PluginEditor.h"

OctoBassProcessor::OctoBassProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts_(*this, nullptr, "OctoBassParams", createParameterLayout())
{
}

OctoBassProcessor::~OctoBassProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout OctoBassProcessor::createParameterLayout()
{
  std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
  return {params.begin(), params.end()};
}

void OctoBassProcessor::prepareToPlay(double /*sampleRate*/, int /*samplesPerBlock*/) {}

void OctoBassProcessor::releaseResources() {}

bool OctoBassProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
  if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() &&
      layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
    return false;

  if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
    return false;

  return true;
}

void OctoBassProcessor::processBlock(juce::AudioBuffer<float>& /*buffer*/,
                                     juce::MidiBuffer& /*midiMessages*/)
{
  // Pass-through: input audio is unchanged on the output
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
  return 0.0;
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

void OctoBassProcessor::getStateInformation(juce::MemoryBlock& destData)
{
  auto state = apvts_.copyState();
  std::unique_ptr<juce::XmlElement> xml(state.createXml());
  copyXmlToBinary(*xml, destData);
}

void OctoBassProcessor::setStateInformation(const void* data, int sizeInBytes)
{
  std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
  if (xmlState != nullptr && xmlState->hasTagName(apvts_.state.getType()))
    apvts_.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
  return new OctoBassProcessor();
}
