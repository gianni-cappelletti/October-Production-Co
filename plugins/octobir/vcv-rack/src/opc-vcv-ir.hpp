#pragma once

#ifdef OCTOBIR_VCV_HEADLESS_TEST
#include "rack_stub.hpp"
#else
#include <rack.hpp>

#include "plugin.hpp"
#endif

#include <atomic>
#include <mutex>
#include <octobir-core/IRProcessor.hpp>
#include <string>

struct OpcVcvIr final : Module
{
  enum class ParamId : uint8_t
  {
    BlendParam,
    OutputGainParam,
    IrAEnableParam,
    IrBEnableParam,
    IrATrimGainParam,
    IrBTrimGainParam,
    DynamicModeParam,
    SidechainEnableParam,
    ThresholdParam,
    RangeDbParam,
    KneeWidthDbParam,
    DetectionModeParam,
    AttackTimeParam,
    ReleaseTimeParam,
    ParamsLen
  };

  enum class InputId : uint8_t
  {
    AudioInL,
    AudioInR,
    SidechainIn,
    ThresholdCvIn,
    BlendCvIn,
    DynamicsEnableCvIn,
    InputsLen
  };

  enum class OutputId : uint8_t
  {
    OutputL,
    OutputR,
    OutputsLen
  };

  enum class LightId : uint8_t
  {
    DynamicModeLight,
    SidechainLight,
    LightsLen
  };

  float getCurrentInputLevelDb() const
  {
    return currentInputLevelDb_.load(std::memory_order_relaxed);
  }
  float getCurrentBlend() const { return currentBlend_.load(std::memory_order_relaxed); }
  const octob::IRProcessor& getIRProcessor() const { return irProcessor_; }
  uint32_t getLastSystemSampleRate() const { return lastSystemSampleRate_; }

 private:
  octob::IRProcessor irProcessor_;
  std::atomic<float> currentInputLevelDb_{-96.f};
  std::atomic<float> currentBlend_{0.f};
  uint32_t lastSystemSampleRate_ = 44100;
  std::string loaded_file_path1_;
  std::string loaded_file_path2_;
  mutable std::mutex path_mutex_;

 public:
  OpcVcvIr()
  {
    config(static_cast<int>(ParamId::ParamsLen), static_cast<int>(InputId::InputsLen),
           static_cast<int>(OutputId::OutputsLen), static_cast<int>(LightId::LightsLen));

    configParam(static_cast<int>(ParamId::BlendParam), -1.f, 1.f, 0.f, "Blend");
    configParam(static_cast<int>(ParamId::OutputGainParam), -24.f, 24.f, 0.f, "Output Gain", " dB");
    configSwitch(static_cast<int>(ParamId::IrAEnableParam), 0.f, 1.f, 1.f, "IR A Enable",
                 {"Disabled", "Enabled"});
    configSwitch(static_cast<int>(ParamId::IrBEnableParam), 0.f, 1.f, 1.f, "IR B Enable",
                 {"Disabled", "Enabled"});
    configParam(static_cast<int>(ParamId::IrATrimGainParam), -12.f, 12.f, 0.f, "IR A Trim", " dB");
    configParam(static_cast<int>(ParamId::IrBTrimGainParam), -12.f, 12.f, 0.f, "IR B Trim", " dB");
    configSwitch(static_cast<int>(ParamId::DynamicModeParam), 0.f, 1.f, 0.f, "Dynamic Mode",
                 {"Off", "On"});
    configSwitch(static_cast<int>(ParamId::SidechainEnableParam), 0.f, 1.f, 0.f, "Sidechain Enable",
                 {"Off", "On"});
    configParam(static_cast<int>(ParamId::ThresholdParam), -60.f, 0.f, -30.f, "Threshold", " dB");
    configParam(static_cast<int>(ParamId::RangeDbParam), 1.f, 60.f, 20.f, "Range", " dB");
    configParam(static_cast<int>(ParamId::KneeWidthDbParam), 0.f, 20.f, 5.f, "Knee", " dB");
    configSwitch(static_cast<int>(ParamId::DetectionModeParam), 0.f, 1.f, 0.f, "Detection Mode",
                 {"Peak", "RMS"});
    configParam(static_cast<int>(ParamId::AttackTimeParam), 1.f, 500.f, 50.f, "Attack Time", " ms");
    configParam(static_cast<int>(ParamId::ReleaseTimeParam), 1.f, 1000.f, 200.f, "Release Time",
                " ms");

    configInput(static_cast<int>(InputId::AudioInL), "Audio Input L");
    configInput(static_cast<int>(InputId::AudioInR), "Audio Input R");
    configInput(static_cast<int>(InputId::SidechainIn), "Sidechain Input");
    configInput(static_cast<int>(InputId::ThresholdCvIn), "Threshold CV");
    configInput(static_cast<int>(InputId::BlendCvIn), "Blend CV");
    configInput(static_cast<int>(InputId::DynamicsEnableCvIn), "Dynamics Enable Gate");
    configOutput(static_cast<int>(OutputId::OutputL), "Audio Output L");
    configOutput(static_cast<int>(OutputId::OutputR), "Audio Output R");

    configLight(static_cast<int>(LightId::DynamicModeLight), "Dynamic Mode Active");
    configLight(static_cast<int>(LightId::SidechainLight), "Sidechain Active");

    irProcessor_.setSampleRate(44100.0);
    irProcessor_.setMaxBlockSize(1);
  }

  void onSampleRateChange(const SampleRateChangeEvent& e) override
  {
    lastSystemSampleRate_ = static_cast<uint32_t>(e.sampleRate);
    irProcessor_.setSampleRate(e.sampleRate);
  }

  std::string getLoadedFilePath(bool ir2) const
  {
    std::lock_guard<std::mutex> lock(path_mutex_);
    return ir2 ? loaded_file_path2_ : loaded_file_path1_;
  }

  void loadIR(const std::string& file_path)
  {
    std::string error;
    if (irProcessor_.loadImpulseResponse1(file_path, error))
    {
      std::lock_guard<std::mutex> lock(path_mutex_);
      loaded_file_path1_ = file_path;
      INFO("Loaded IR A: %s (%zu samples, %.0f Hz)", file_path.c_str(),
           irProcessor_.getIR1NumSamples(), irProcessor_.getIR1SampleRate());
    }
    else
    {
      WARN("Failed to load IR A file %s: %s", file_path.c_str(), error.c_str());
      std::lock_guard<std::mutex> lock(path_mutex_);
      loaded_file_path1_.clear();
    }
  }

  void loadIR2(const std::string& file_path)
  {
    std::string error;
    if (irProcessor_.loadImpulseResponse2(file_path, error))
    {
      std::lock_guard<std::mutex> lock(path_mutex_);
      loaded_file_path2_ = file_path;
      INFO("Loaded IR B: %s (%zu samples, %.0f Hz)", file_path.c_str(),
           irProcessor_.getIR2NumSamples(), irProcessor_.getIR2SampleRate());
    }
    else
    {
      WARN("Failed to load IR B file %s: %s", file_path.c_str(), error.c_str());
      std::lock_guard<std::mutex> lock(path_mutex_);
      loaded_file_path2_.clear();
    }
  }

  void clearIR1()
  {
    irProcessor_.clearImpulseResponse1();
    {
      std::lock_guard<std::mutex> lock(path_mutex_);
      loaded_file_path1_.clear();
    }
    INFO("IR A cleared");
  }

  void clearIR2()
  {
    irProcessor_.clearImpulseResponse2();
    {
      std::lock_guard<std::mutex> lock(path_mutex_);
      loaded_file_path2_.clear();
    }
    INFO("IR B cleared");
  }

  void swapImpulseResponses()
  {
    irProcessor_.swapIRSlots();

    {
      std::lock_guard<std::mutex> lock(path_mutex_);
      std::swap(loaded_file_path1_, loaded_file_path2_);
    }

    const float trimA = params[static_cast<int>(ParamId::IrATrimGainParam)].getValue();
    const float trimB = params[static_cast<int>(ParamId::IrBTrimGainParam)].getValue();
    params[static_cast<int>(ParamId::IrATrimGainParam)].setValue(trimB);
    params[static_cast<int>(ParamId::IrBTrimGainParam)].setValue(trimA);
  }

  void process(const ProcessArgs& args) override
  {
    (void)args;

    bool dynamicMode =
        inputs[static_cast<int>(InputId::DynamicsEnableCvIn)].isConnected()
            ? inputs[static_cast<int>(InputId::DynamicsEnableCvIn)].getVoltage() > 1.0f
            : params[static_cast<int>(ParamId::DynamicModeParam)].getValue() > 0.5f;

    bool sidechainEnabled =
        params[static_cast<int>(ParamId::SidechainEnableParam)].getValue() > 0.5f;

    float threshold = params[static_cast<int>(ParamId::ThresholdParam)].getValue() +
                      (inputs[static_cast<int>(InputId::ThresholdCvIn)].isConnected()
                           ? inputs[static_cast<int>(InputId::ThresholdCvIn)].getVoltage() * 6.0f
                           : 0.0f);
    threshold = clamp(threshold, -60.f, 0.f);

    float blend = params[static_cast<int>(ParamId::BlendParam)].getValue() +
                  (inputs[static_cast<int>(InputId::BlendCvIn)].isConnected()
                       ? inputs[static_cast<int>(InputId::BlendCvIn)].getVoltage() * 0.2f
                       : 0.0f);
    blend = clamp(blend, -1.f, 1.f);

    const bool leftConnected = inputs[static_cast<int>(InputId::AudioInL)].isConnected();
    const bool rightConnected = inputs[static_cast<int>(InputId::AudioInR)].isConnected();
    const bool scConnected = inputs[static_cast<int>(InputId::SidechainIn)].isConnected();

    irProcessor_.setDynamicModeEnabled(dynamicMode);
    irProcessor_.setSidechainEnabled(dynamicMode && sidechainEnabled && scConnected);
    irProcessor_.setBlend(blend);
    irProcessor_.setIRAEnabled(params[static_cast<int>(ParamId::IrAEnableParam)].getValue() > 0.5f);
    irProcessor_.setIRBEnabled(params[static_cast<int>(ParamId::IrBEnableParam)].getValue() > 0.5f);
    irProcessor_.setThreshold(threshold);
    irProcessor_.setRangeDb(params[static_cast<int>(ParamId::RangeDbParam)].getValue());
    irProcessor_.setKneeWidthDb(params[static_cast<int>(ParamId::KneeWidthDbParam)].getValue());
    irProcessor_.setDetectionMode(
        static_cast<int>(params[static_cast<int>(ParamId::DetectionModeParam)].getValue()));
    irProcessor_.setAttackTime(params[static_cast<int>(ParamId::AttackTimeParam)].getValue());
    irProcessor_.setReleaseTime(params[static_cast<int>(ParamId::ReleaseTimeParam)].getValue());
    irProcessor_.setOutputGain(params[static_cast<int>(ParamId::OutputGainParam)].getValue());
    irProcessor_.setIRATrimGain(params[static_cast<int>(ParamId::IrATrimGainParam)].getValue());
    irProcessor_.setIRBTrimGain(params[static_cast<int>(ParamId::IrBTrimGainParam)].getValue());

    lights[static_cast<int>(LightId::DynamicModeLight)].setBrightness(dynamicMode ? 1.0f : 0.0f);
    lights[static_cast<int>(LightId::SidechainLight)].setBrightness(sidechainEnabled ? 1.0f : 0.0f);

    if (dynamicMode && sidechainEnabled && scConnected)
    {
      float sc = inputs[static_cast<int>(InputId::SidechainIn)].getVoltage();

      if (leftConnected && rightConnected)
      {
        float inputL = inputs[static_cast<int>(InputId::AudioInL)].getVoltage();
        float inputR = inputs[static_cast<int>(InputId::AudioInR)].getVoltage();
        float outputL = 0.0f;
        float outputR = 0.0f;
        irProcessor_.processStereoWithSidechain(&inputL, &inputR, &sc, &sc, &outputL, &outputR, 1);
        outputs[static_cast<int>(OutputId::OutputL)].setVoltage(outputL);
        outputs[static_cast<int>(OutputId::OutputR)].setVoltage(outputR);
      }
      else if (leftConnected)
      {
        float input = inputs[static_cast<int>(InputId::AudioInL)].getVoltage();
        float outputL = 0.0f;
        float outputR = 0.0f;
        irProcessor_.processMonoToStereoWithSidechain(&input, &sc, &outputL, &outputR, 1);
        outputs[static_cast<int>(OutputId::OutputL)].setVoltage(outputL);
        outputs[static_cast<int>(OutputId::OutputR)].setVoltage(outputR);
      }
      else if (rightConnected)
      {
        float input = inputs[static_cast<int>(InputId::AudioInR)].getVoltage();
        float outputL = 0.0f;
        float outputR = 0.0f;
        irProcessor_.processMonoToStereoWithSidechain(&input, &sc, &outputL, &outputR, 1);
        outputs[static_cast<int>(OutputId::OutputL)].setVoltage(outputL);
        outputs[static_cast<int>(OutputId::OutputR)].setVoltage(outputR);
      }
      else
      {
        outputs[static_cast<int>(OutputId::OutputL)].setVoltage(0.0f);
        outputs[static_cast<int>(OutputId::OutputR)].setVoltage(0.0f);
      }
    }
    else
    {
      if (leftConnected && rightConnected)
      {
        float inputL = inputs[static_cast<int>(InputId::AudioInL)].getVoltage();
        float inputR = inputs[static_cast<int>(InputId::AudioInR)].getVoltage();
        float outputL = 0.0f;
        float outputR = 0.0f;
        irProcessor_.processStereo(&inputL, &inputR, &outputL, &outputR, 1);
        outputs[static_cast<int>(OutputId::OutputL)].setVoltage(outputL);
        outputs[static_cast<int>(OutputId::OutputR)].setVoltage(outputR);
      }
      else if (leftConnected)
      {
        float input = inputs[static_cast<int>(InputId::AudioInL)].getVoltage();
        float outputL = 0.0f;
        float outputR = 0.0f;
        irProcessor_.processMonoToStereo(&input, &outputL, &outputR, 1);
        outputs[static_cast<int>(OutputId::OutputL)].setVoltage(outputL);
        outputs[static_cast<int>(OutputId::OutputR)].setVoltage(outputR);
      }
      else if (rightConnected)
      {
        float input = inputs[static_cast<int>(InputId::AudioInR)].getVoltage();
        float outputL = 0.0f;
        float outputR = 0.0f;
        irProcessor_.processMonoToStereo(&input, &outputL, &outputR, 1);
        outputs[static_cast<int>(OutputId::OutputL)].setVoltage(outputL);
        outputs[static_cast<int>(OutputId::OutputR)].setVoltage(outputR);
      }
      else
      {
        outputs[static_cast<int>(OutputId::OutputL)].setVoltage(0.0f);
        outputs[static_cast<int>(OutputId::OutputR)].setVoltage(0.0f);
      }
    }

    currentInputLevelDb_.store(irProcessor_.getCurrentInputLevel(), std::memory_order_relaxed);
    currentBlend_.store(irProcessor_.getCurrentBlend(), std::memory_order_relaxed);
  }

  json_t* dataToJson() override
  {
    json_t* rootJ = json_object();
    std::lock_guard<std::mutex> lock(path_mutex_);
    json_object_set_new(rootJ, "ir1Path", json_string(loaded_file_path1_.c_str()));
    json_object_set_new(rootJ, "ir2Path", json_string(loaded_file_path2_.c_str()));
    return rootJ;
  }

  void dataFromJson(json_t* rootJ) override
  {
    json_t* ir1PathJ = json_object_get(rootJ, "ir1Path");
    if (json_is_string(ir1PathJ))
    {
      std::string path = json_string_value(ir1PathJ);
      if (!path.empty())
        loadIR(path);
    }

    json_t* ir2PathJ = json_object_get(rootJ, "ir2Path");
    if (json_is_string(ir2PathJ))
    {
      std::string path = json_string_value(ir2PathJ);
      if (!path.empty())
        loadIR2(path);
    }
  }
};
