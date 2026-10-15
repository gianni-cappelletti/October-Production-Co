#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <octobass-core/BassProcessor.hpp>
#include <octobass-core/Types.hpp>

#include "BrowseFolderMemory.h"

// AsyncUpdater is public so tests can flush deferred work deterministically
// with handleUpdateNowIfNeeded() instead of pumping the message loop
class OctoBassProcessor : public juce::AudioProcessor,
                          public juce::AsyncUpdater,
                          private juce::AudioProcessorValueTreeState::Listener
{
 public:
  enum class FileSlot : uint8_t
  {
    Nam1,
    Nam2,
    ImpulseResponse
  };

  explicit OctoBassProcessor(const juce::File& browseFolderSettingsFile =
                                 BrowseFolderMemory::getDefaultSettingsFile(JucePlugin_Name));
  ~OctoBassProcessor() override;

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

  juce::AudioProcessorValueTreeState& getAPVTS() { return apvts_; }

  struct NamSlotIds
  {
    const char* quality;
    const char* calibrateInput;
    const char* inputCalibrationLevel;
    const char* outputMode;
    const char* on;
    const char* modelPathProperty;
  };
  static const NamSlotIds& getNamSlotIds(octob::NamSlot slot);

  static juce::String getNamSlotName(octob::NamSlot slot);

  // NAM model management
  bool loadNamModel(octob::NamSlot slot, const juce::String& filepath, juce::String& errorMessage);
  void clearNamModel(octob::NamSlot slot);
  bool isNamModelLoaded(octob::NamSlot slot) const;
  juce::String getCurrentNamModelPath(octob::NamSlot slot) const;

  // Discrete quality levels of the loaded NAM model: 0 = no model,
  // 1 = no quality options, >1 = selectable levels. Message thread only.
  int getNamQualityLevels(octob::NamSlot slot) const;

  // Level metadata of the loaded NAM model, all-false defaults when none is
  // loaded. Message thread only.
  octob::NamModelMetadata getNamModelMetadata(octob::NamSlot slot) const;

  // IR management
  bool loadImpulseResponse(const juce::String& filepath, juce::String& errorMessage);
  void clearImpulseResponse();
  bool isIRLoaded() const;
  juce::String getCurrentIRPath() const;

  static FileSlot fileSlotFor(octob::NamSlot slot);
  juce::File getBrowseStartFolder(FileSlot slot) const;
  bool loadPickedFile(FileSlot slot, const juce::File& file, juce::String& errorMessage);

  int getLatencySamples() const;

  // Spectrum analyzer FIFO (single-writer audio thread, single-reader GUI timer)
  static constexpr int kSpectrumFifoSize = 8192;
  juce::AbstractFifo& getSpectrumFifo() { return spectrumFifo_; }
  const std::array<float, kSpectrumFifoSize>& getSpectrumFifoBuffer() const
  {
    return spectrumFifoBuffer_;
  }

 private:
  juce::AudioProcessorValueTreeState apvts_;
  juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

  octob::BassProcessor bassProcessor_;
  BrowseFolderMemory browseFolderMemory_;

  bool prevLowSolo_ = false;
  bool prevHighSolo_ = false;

  // Which solo button the message thread must switch off to keep the pair
  // mutually exclusive (setValueNotifyingHost is not real-time safe)
  enum class SoloCorrection : int
  {
    None,
    ClearLow,
    ClearHigh
  };
  std::atomic<SoloCorrection> pendingSoloCorrection_{SoloCorrection::None};

  juce::SpinLock pendingStateLock_;
  juce::ValueTree pendingState_;
  void handleAsyncUpdate() override;

  // NAM quality is applied on the message thread via the async updater because
  // SetSlimmableSize is not real-time safe
  void parameterChanged(const juce::String& parameterID, float newValue) override;

  struct NamSlotParams
  {
    std::atomic<float>* quality = nullptr;
    std::atomic<float>* calibrateInput = nullptr;
    std::atomic<float>* inputCalibrationLevel = nullptr;
    std::atomic<float>* outputMode = nullptr;
    std::atomic<float>* on = nullptr;
    std::atomic<bool> qualityDirty{false};
  };
  std::array<NamSlotParams, octob::kNumNamSlots> namSlotParams_;
  NamSlotParams& namSlotParams(octob::NamSlot slot)
  {
    return namSlotParams_[static_cast<size_t>(slot)];
  }

  struct EQNodeParams
  {
    std::atomic<float>* active = nullptr;
    std::atomic<float>* freq = nullptr;
    std::atomic<float>* gain = nullptr;
  };
  std::array<EQNodeParams, octob::kGraphicEQNumNodes> eqNodeParams_{};
  std::atomic<float>* eqLowCutActiveParam_ = nullptr;
  std::atomic<float>* eqLowCutFreqParam_ = nullptr;
  std::atomic<float>* eqHighCutActiveParam_ = nullptr;
  std::atomic<float>* eqHighCutFreqParam_ = nullptr;

  std::atomic<float>* crossoverParam_ = nullptr;
  std::atomic<float>* squashParam_ = nullptr;
  std::atomic<float>* compressionModeParam_ = nullptr;
  std::atomic<float>* lowBandLevelParam_ = nullptr;
  std::atomic<float>* highInputGainParam_ = nullptr;
  std::atomic<float>* highOutputGainParam_ = nullptr;
  std::atomic<float>* outputGainParam_ = nullptr;
  std::atomic<float>* dryWetMixParam_ = nullptr;
  std::atomic<float>* gateThresholdParam_ = nullptr;
  std::atomic<float>* highBandMixParam_ = nullptr;
  std::atomic<float>* lowBandSoloParam_ = nullptr;
  std::atomic<float>* highBandSoloParam_ = nullptr;

  juce::AbstractFifo spectrumFifo_{kSpectrumFifoSize};
  std::array<float, kSpectrumFifoSize> spectrumFifoBuffer_{};

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OctoBassProcessor)
};
