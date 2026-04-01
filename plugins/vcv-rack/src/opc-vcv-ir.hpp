#pragma once

#ifdef OCTOBIR_VCV_HEADLESS_TEST
#include "rack_stub.hpp"
#else
#include <rack.hpp>

#include "plugin.hpp"
#endif

#include <atomic>
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

  octob::IRProcessor irProcessor_;
  std::string loaded_file_path_;
  std::string loaded_file_path2_;
  std::atomic<float> currentInputLevelDb_{-96.f};
  std::atomic<float> currentBlend_{0.f};
  uint32_t last_system_sample_rate_ = 44100;

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
    last_system_sample_rate_ = static_cast<uint32_t>(e.sampleRate);
    irProcessor_.setSampleRate(e.sampleRate);
  }

  void loadIR(const std::string& file_path)
  {
    std::string error;
    if (irProcessor_.loadImpulseResponse1(file_path, error))
    {
      loaded_file_path_ = file_path;
      INFO("Loaded IR A: %s (%zu samples, %.0f Hz)", file_path.c_str(),
           irProcessor_.getIR1NumSamples(), irProcessor_.getIR1SampleRate());
    }
    else
    {
      WARN("Failed to load IR A file %s: %s", file_path.c_str(), error.c_str());
      loaded_file_path_.clear();
    }
  }

  void loadIR2(const std::string& file_path)
  {
    std::string error;
    if (irProcessor_.loadImpulseResponse2(file_path, error))
    {
      loaded_file_path2_ = file_path;
      INFO("Loaded IR B: %s (%zu samples, %.0f Hz)", file_path.c_str(),
           irProcessor_.getIR2NumSamples(), irProcessor_.getIR2SampleRate());
    }
    else
    {
      WARN("Failed to load IR B file %s: %s", file_path.c_str(), error.c_str());
      loaded_file_path2_.clear();
    }
  }

  void swapImpulseResponses()
  {
    const std::string path1 = loaded_file_path_;
    const std::string path2 = loaded_file_path2_;
    const float trimA = params[static_cast<int>(ParamId::IrATrimGainParam)].getValue();
    const float trimB = params[static_cast<int>(ParamId::IrBTrimGainParam)].getValue();

    if (!path2.empty())
    {
      loadIR(path2);
    }
    else
    {
      irProcessor_.clearImpulseResponse1();
      loaded_file_path_.clear();
    }

    if (!path1.empty())
    {
      loadIR2(path1);
    }
    else
    {
      irProcessor_.clearImpulseResponse2();
      loaded_file_path2_.clear();
    }

    params[static_cast<int>(ParamId::IrATrimGainParam)].setValue(trimB);
    params[static_cast<int>(ParamId::IrBTrimGainParam)].setValue(trimA);
  }

  void process(const ProcessArgs& args) override
  {
    (void)args;

    bool dynamicMode = params[static_cast<int>(ParamId::DynamicModeParam)].getValue() > 0.5f;
    if (inputs[static_cast<int>(InputId::DynamicsEnableCvIn)].isConnected())
    {
      dynamicMode =
          dynamicMode || inputs[static_cast<int>(InputId::DynamicsEnableCvIn)].getVoltage() > 1.0f;
    }

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

    irProcessor_.setDynamicModeEnabled(dynamicMode);
    irProcessor_.setSidechainEnabled(sidechainEnabled);
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

    const bool leftConnected = inputs[static_cast<int>(InputId::AudioInL)].isConnected();
    const bool rightConnected = inputs[static_cast<int>(InputId::AudioInR)].isConnected();
    const bool scConnected = inputs[static_cast<int>(InputId::SidechainIn)].isConnected();

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

    currentInputLevelDb_.store(irProcessor_.getCurrentInputLevel());
    currentBlend_.store(irProcessor_.getCurrentBlend());
  }

  json_t* dataToJson() override
  {
    json_t* rootJ = json_object();
    json_object_set_new(rootJ, "ir1Path", json_string(loaded_file_path_.c_str()));
    json_object_set_new(rootJ, "ir2Path", json_string(loaded_file_path2_.c_str()));
    return rootJ;
  }

  void dataFromJson(json_t* rootJ) override
  {
    json_t* ir1PathJ = json_object_get(rootJ, "ir1Path");
    if (ir1PathJ != nullptr)
    {
      std::string path = json_string_value(ir1PathJ);
      if (!path.empty())
        loadIR(path);
    }

    json_t* ir2PathJ = json_object_get(rootJ, "ir2Path");
    if (ir2PathJ != nullptr)
    {
      std::string path = json_string_value(ir2PathJ);
      if (!path.empty())
        loadIR2(path);
    }

    // Backward compat: pre-port patch keys
    json_t* filePathJ = json_object_get(rootJ, "filePath");
    if (filePathJ != nullptr && ir1PathJ == nullptr)
    {
      std::string path = json_string_value(filePathJ);
      if (!path.empty())
        loadIR(path);
    }

    json_t* filePath2J = json_object_get(rootJ, "filePath2");
    if (filePath2J != nullptr && ir2PathJ == nullptr)
    {
      std::string path = json_string_value(filePath2J);
      if (!path.empty())
        loadIR2(path);
    }

    // Backward compat: migrate old bool fields to param values
    json_t* dynamicModeJ = json_object_get(rootJ, "dynamicModeEnabled");
    if (dynamicModeJ != nullptr)
    {
      params[static_cast<int>(ParamId::DynamicModeParam)].setValue(
          json_boolean_value(dynamicModeJ) ? 1.f : 0.f);
    }

    json_t* sidechainJ = json_object_get(rootJ, "sidechainEnabled");
    if (sidechainJ != nullptr)
    {
      params[static_cast<int>(ParamId::SidechainEnableParam)].setValue(
          json_boolean_value(sidechainJ) ? 1.f : 0.f);
    }
  }
};
