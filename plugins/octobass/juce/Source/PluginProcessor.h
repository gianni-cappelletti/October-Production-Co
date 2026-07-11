#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>
#include <octobass-core/BassProcessor.hpp>
#include <octobass-core/Types.hpp>

// AsyncUpdater is public so tests can flush deferred work deterministically
// with handleUpdateNowIfNeeded() instead of pumping the message loop
class OctoBassProcessor : public juce::AudioProcessor,
                          public juce::AsyncUpdater,
                          private juce::AudioProcessorValueTreeState::Listener
{
 public:
  OctoBassProcessor();
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

  // NAM model management
  bool loadNamModel(const juce::String& filepath, juce::String& errorMessage);
  void clearNamModel();
  bool isNamModelLoaded() const;
  juce::String getCurrentNamModelPath() const;

  // Discrete quality levels of the loaded NAM model: 0 = no model,
  // 1 = no quality options, >1 = selectable levels. Message thread only.
  int getNamQualityLevels() const;

  // Level metadata of the loaded NAM model, all-false defaults when none is
  // loaded. Message thread only.
  octob::NamModelMetadata getNamModelMetadata() const;

  // IR management
  bool loadImpulseResponse(const juce::String& filepath, juce::String& errorMessage);
  void clearImpulseResponse();
  bool isIRLoaded() const;
  juce::String getCurrentIRPath() const;

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
  std::atomic<bool> namQualityDirty_{false};
  std::atomic<float>* namQualityParam_ = nullptr;

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
  std::atomic<float>* namCalibrateInputParam_ = nullptr;
  std::atomic<float>* namInputCalibrationLevelParam_ = nullptr;
  std::atomic<float>* namOutputModeParam_ = nullptr;

  juce::AbstractFifo spectrumFifo_{kSpectrumFifoSize};
  std::array<float, kSpectrumFifoSize> spectrumFifoBuffer_{};

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OctoBassProcessor)
};
