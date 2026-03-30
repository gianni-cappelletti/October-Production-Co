#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <octobir-core/IRProcessor.hpp>

class OctobIRProcessor : public juce::AudioProcessor, private juce::AsyncUpdater
{
 public:
  OctobIRProcessor();
  ~OctobIRProcessor() override;

  void prepareToPlay(double sampleRate, int samplesPerBlock) override;
  void releaseResources() override;

  bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

  void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

  juce::AudioProcessorEditor* createEditor() override;
  bool hasEditor() const override;

  const juce::String getName() const override;

  bool acceptsMidi() const override;
  bool producesMidi() const override;
  bool isMidiEffect() const override;
  double getTailLengthSeconds() const override;

  int getNumPrograms() override;
  int getCurrentProgram() override;
  void setCurrentProgram(int index) override;
  const juce::String getProgramName(int index) override;
  void changeProgramName(int index, const juce::String& newName) override;

  void getStateInformation(juce::MemoryBlock& destData) override;
  void setStateInformation(const void* data, int sizeInBytes) override;

  bool loadImpulseResponse1(const juce::String& filepath, juce::String& errorMessage);
  bool loadImpulseResponse2(const juce::String& filepath, juce::String& errorMessage);
  void clearImpulseResponse1();
  void clearImpulseResponse2();
  void swapImpulseResponses();
  juce::String getCurrentIR1Path() const { return currentIR1Path_; }
  juce::String getCurrentIR2Path() const { return currentIR2Path_; }

  juce::AudioProcessorValueTreeState& getAPVTS() { return apvts_; }

  float getCurrentInputLevel() const { return irProcessor_.getCurrentInputLevel(); }
  float getCurrentBlend() const { return irProcessor_.getCurrentBlend(); }
  octob::IRProcessor& getIRProcessor() { return irProcessor_; }

  std::atomic<int> lastEditorWidth_{580};
  std::atomic<int> lastEditorHeight_{694};

 private:
  octob::IRProcessor irProcessor_;
  juce::String currentIR1Path_;
  juce::String currentIR2Path_;

  juce::AudioProcessorValueTreeState apvts_;
  juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

  juce::SpinLock pendingStateLock_;
  juce::ValueTree pendingState_;
  void handleAsyncUpdate() override;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OctobIRProcessor)
};
