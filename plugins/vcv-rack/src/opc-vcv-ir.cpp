#include <osdialog.h>

#include <octobir-core/IRProcessor.hpp>
#include <string>

#include "plugin.hpp"

struct OpcVcvIr final : Module
{
  enum class ParamId : uint8_t
  {
    GainParam,
    BlendParam,
    IrAEnableParam,
    IrBEnableParam,
    ThresholdParam,
    RangeDbParam,
    KneeWidthDbParam,
    DetectionModeParam,
    AttackTimeParam,
    ReleaseTimeParam,
    OutputGainParam,
    ParamsLen
  };
  enum class InputId : uint8_t
  {
    InputL,
    InputR,
    SidechainL,
    SidechainR,
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

  bool dynamicModeEnabled_ = false;
  bool sidechainEnabled_ = false;

  int sample_rate_check_counter_ = 0;
  static constexpr int SampleRateCheckInterval = 4096;
  uint32_t last_system_sample_rate_;

  OpcVcvIr() : last_system_sample_rate_(static_cast<uint32_t>(APP->engine->getSampleRate()))
  {
    config(static_cast<int>(ParamId::ParamsLen), static_cast<int>(InputId::InputsLen),
           static_cast<int>(OutputId::OutputsLen), static_cast<int>(LightId::LightsLen));
    configParam(static_cast<int>(ParamId::GainParam), -200.f, 200.f, 0.f, "Gain", " dB", 0.f, 0.1f);
    paramQuantities[static_cast<int>(ParamId::GainParam)]->snapEnabled = true;

    configParam(static_cast<int>(ParamId::BlendParam), -1.f, 1.f, 0.f, "Static Blend");
    configSwitch(static_cast<int>(ParamId::IrAEnableParam), 0.f, 1.f, 1.f, "IR A Enable",
                 {"Disabled", "Enabled"});
    configSwitch(static_cast<int>(ParamId::IrBEnableParam), 0.f, 1.f, 1.f, "IR B Enable",
                 {"Disabled", "Enabled"});
    configParam(static_cast<int>(ParamId::ThresholdParam), -60.f, 0.f, -30.f, "Threshold", " dB");
    configParam(static_cast<int>(ParamId::RangeDbParam), 1.f, 60.f, 20.f, "Range", " dB");
    configParam(static_cast<int>(ParamId::KneeWidthDbParam), 0.f, 20.f, 5.f, "Knee", " dB");
    configSwitch(static_cast<int>(ParamId::DetectionModeParam), 0.f, 1.f, 0.f, "Detection Mode",
                 {"Peak", "RMS"});
    configParam(static_cast<int>(ParamId::AttackTimeParam), 1.f, 500.f, 50.f, "Attack Time", " ms");
    configParam(static_cast<int>(ParamId::ReleaseTimeParam), 1.f, 1000.f, 200.f, "Release Time",
                " ms");
    configParam(static_cast<int>(ParamId::OutputGainParam), -24.f, 24.f, 0.f, "Output Gain", " dB");

    configInput(static_cast<int>(InputId::InputL), "Audio Input L");
    configInput(static_cast<int>(InputId::InputR), "Audio Input R");
    configInput(static_cast<int>(InputId::SidechainL), "Sidechain Input L");
    configInput(static_cast<int>(InputId::SidechainR), "Sidechain Input R");
    configOutput(static_cast<int>(OutputId::OutputL), "Audio Output L");
    configOutput(static_cast<int>(OutputId::OutputR), "Audio Output R");

    configLight(static_cast<int>(LightId::DynamicModeLight), "Dynamic Mode Active");
    configLight(static_cast<int>(LightId::SidechainLight), "Sidechain Active");

    irProcessor_.setSampleRate(APP->engine->getSampleRate());

    const char* homeEnv = getenv("HOME");
    std::string homeDir = (homeEnv != nullptr) ? homeEnv : "";
    std::string irPath = homeDir + "/ir_sample.wav";
    loadIR(irPath);
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

  void process(const ProcessArgs& args) override
  {
    if (++sample_rate_check_counter_ >= SampleRateCheckInterval)
    {
      sample_rate_check_counter_ = 0;

      if (irProcessor_.isIR1Loaded())
      {
        auto currentSystemSampleRate = static_cast<uint32_t>(args.sampleRate);
        if (currentSystemSampleRate != last_system_sample_rate_)
        {
          INFO("System sample rate changed from %u to %u Hz, updating IR processor...",
               last_system_sample_rate_, currentSystemSampleRate);
          last_system_sample_rate_ = currentSystemSampleRate;
          irProcessor_.setSampleRate(args.sampleRate);
        }
      }
    }

    float gainDb = params[static_cast<int>(ParamId::GainParam)].getValue() * 0.1f;
    float gain = dsp::dbToAmplitude(gainDb);

    irProcessor_.setDynamicModeEnabled(dynamicModeEnabled_);
    irProcessor_.setSidechainEnabled(sidechainEnabled_);
    irProcessor_.setBlend(params[static_cast<int>(ParamId::BlendParam)].getValue());
    irProcessor_.setIRAEnabled(params[static_cast<int>(ParamId::IrAEnableParam)].getValue() > 0.5f);
    irProcessor_.setIRBEnabled(params[static_cast<int>(ParamId::IrBEnableParam)].getValue() > 0.5f);
    irProcessor_.setThreshold(params[static_cast<int>(ParamId::ThresholdParam)].getValue());
    irProcessor_.setRangeDb(params[static_cast<int>(ParamId::RangeDbParam)].getValue());
    irProcessor_.setKneeWidthDb(params[static_cast<int>(ParamId::KneeWidthDbParam)].getValue());
    irProcessor_.setDetectionMode(
        static_cast<int>(params[static_cast<int>(ParamId::DetectionModeParam)].getValue()));
    irProcessor_.setAttackTime(params[static_cast<int>(ParamId::AttackTimeParam)].getValue());
    irProcessor_.setReleaseTime(params[static_cast<int>(ParamId::ReleaseTimeParam)].getValue());
    irProcessor_.setOutputGain(params[static_cast<int>(ParamId::OutputGainParam)].getValue());

    lights[static_cast<int>(LightId::DynamicModeLight)].setBrightness(dynamicModeEnabled_ ? 1.0f
                                                                                          : 0.0f);
    lights[static_cast<int>(LightId::SidechainLight)].setBrightness(sidechainEnabled_ ? 1.0f
                                                                                      : 0.0f);

    bool leftInputConnected = inputs[static_cast<int>(InputId::InputL)].isConnected();
    bool rightInputConnected = inputs[static_cast<int>(InputId::InputR)].isConnected();
    bool sidechainLConnected = inputs[static_cast<int>(InputId::SidechainL)].isConnected();
    bool sidechainRConnected = inputs[static_cast<int>(InputId::SidechainR)].isConnected();
    bool hasSidechain = sidechainLConnected || sidechainRConnected;

    if (dynamicModeEnabled_ && sidechainEnabled_ && hasSidechain)
    {
      if (leftInputConnected && rightInputConnected)
      {
        float inputL = inputs[static_cast<int>(InputId::InputL)].getVoltage() * gain;
        float inputR = inputs[static_cast<int>(InputId::InputR)].getVoltage() * gain;
        float sidechainL = sidechainLConnected
                               ? inputs[static_cast<int>(InputId::SidechainL)].getVoltage() * gain
                               : inputL;
        float sidechainR = inputR;
        if (sidechainRConnected)
        {
          sidechainR = inputs[static_cast<int>(InputId::SidechainR)].getVoltage() * gain;
        }
        else if (sidechainLConnected)
        {
          sidechainR = sidechainL;
        }
        float outputL = 0.0f;
        float outputR = 0.0f;

        irProcessor_.processStereoWithSidechain(&inputL, &inputR, &sidechainL, &sidechainR,
                                                &outputL, &outputR, 1);

        outputs[static_cast<int>(OutputId::OutputL)].setVoltage(outputL);
        outputs[static_cast<int>(OutputId::OutputR)].setVoltage(outputR);
      }
      else if (leftInputConnected)
      {
        float input = inputs[static_cast<int>(InputId::InputL)].getVoltage() * gain;
        float sidechain = sidechainLConnected
                              ? inputs[static_cast<int>(InputId::SidechainL)].getVoltage() * gain
                              : input;
        float output = 0.0f;

        irProcessor_.processMonoWithSidechain(&input, &sidechain, &output, 1);

        outputs[static_cast<int>(OutputId::OutputL)].setVoltage(output);
        outputs[static_cast<int>(OutputId::OutputR)].setVoltage(output);
      }
      else if (rightInputConnected)
      {
        float input = inputs[static_cast<int>(InputId::InputR)].getVoltage() * gain;
        float sidechain = sidechainRConnected
                              ? inputs[static_cast<int>(InputId::SidechainR)].getVoltage() * gain
                              : input;
        float output = 0.0f;

        irProcessor_.processMonoWithSidechain(&input, &sidechain, &output, 1);

        outputs[static_cast<int>(OutputId::OutputL)].setVoltage(output);
        outputs[static_cast<int>(OutputId::OutputR)].setVoltage(output);
      }
      else
      {
        outputs[static_cast<int>(OutputId::OutputL)].setVoltage(0.0f);
        outputs[static_cast<int>(OutputId::OutputR)].setVoltage(0.0f);
      }
    }
    else
    {
      if (leftInputConnected && rightInputConnected)
      {
        float inputL = inputs[static_cast<int>(InputId::InputL)].getVoltage() * gain;
        float inputR = inputs[static_cast<int>(InputId::InputR)].getVoltage() * gain;
        float outputL = 0.0f;
        float outputR = 0.0f;

        irProcessor_.processStereo(&inputL, &inputR, &outputL, &outputR, 1);

        outputs[static_cast<int>(OutputId::OutputL)].setVoltage(outputL);
        outputs[static_cast<int>(OutputId::OutputR)].setVoltage(outputR);
      }
      else if (leftInputConnected)
      {
        float input = inputs[static_cast<int>(InputId::InputL)].getVoltage() * gain;
        float output = 0.0f;

        irProcessor_.processMono(&input, &output, 1);

        outputs[static_cast<int>(OutputId::OutputL)].setVoltage(output);
        outputs[static_cast<int>(OutputId::OutputR)].setVoltage(output);
      }
      else if (rightInputConnected)
      {
        float input = inputs[static_cast<int>(InputId::InputR)].getVoltage() * gain;
        float output = 0.0f;

        irProcessor_.processMono(&input, &output, 1);

        outputs[static_cast<int>(OutputId::OutputL)].setVoltage(output);
        outputs[static_cast<int>(OutputId::OutputR)].setVoltage(output);
      }
      else
      {
        outputs[static_cast<int>(OutputId::OutputL)].setVoltage(0.0f);
        outputs[static_cast<int>(OutputId::OutputR)].setVoltage(0.0f);
      }
    }
  }

  json_t* dataToJson() override
  {
    json_t* rootJ = json_object();
    json_object_set_new(rootJ, "filePath", json_string(loaded_file_path_.c_str()));
    json_object_set_new(rootJ, "filePath2", json_string(loaded_file_path2_.c_str()));
    json_object_set_new(rootJ, "dynamicModeEnabled", json_boolean(dynamicModeEnabled_));
    json_object_set_new(rootJ, "sidechainEnabled", json_boolean(sidechainEnabled_));
    return rootJ;
  }

  void dataFromJson(json_t* rootJ) override
  {
    json_t* filePathJ = json_object_get(rootJ, "filePath");
    if (filePathJ != nullptr)
    {
      std::string path = json_string_value(filePathJ);
      if (!path.empty())
      {
        loadIR(path);
      }
    }

    json_t* filePath2J = json_object_get(rootJ, "filePath2");
    if (filePath2J != nullptr)
    {
      std::string path = json_string_value(filePath2J);
      if (!path.empty())
      {
        loadIR2(path);
      }
    }

    json_t* dynamicModeJ = json_object_get(rootJ, "dynamicModeEnabled");
    if (dynamicModeJ != nullptr)
    {
      dynamicModeEnabled_ = json_boolean_value(dynamicModeJ);
    }

    json_t* sidechainJ = json_object_get(rootJ, "sidechainEnabled");
    if (sidechainJ != nullptr)
    {
      sidechainEnabled_ = json_boolean_value(sidechainJ);
    }
  }
};

struct IrFileDisplay : app::LedDisplayChoice
{
  OpcVcvIr* module = nullptr;
  bool isIR2;

  IrFileDisplay(bool ir2 = false) : isIR2(ir2)
  {
    fontPath = asset::plugin(pluginInstance, "res/font/Inconsolata_Condensed-Bold.ttf");
    color = nvgRGB(0x12, 0x12, 0x12);
    bgColor = nvgRGB(0xd9, 0x81, 0x29);
    text = "<No IR selected>";
  }

  void onButton(const widget::Widget::ButtonEvent& e) override
  {
    if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT && module != nullptr)
    {
      openFileDialog();
    }
  }

  void openFileDialog()
  {
    osdialog_filters* filters = osdialog_filters_parse("Audio files:wav,aiff,flac;All files:*");
    char* path = osdialog_file(OSDIALOG_OPEN, nullptr, nullptr, filters);

    if (path != nullptr)
    {
      if (isIR2)
      {
        module->loadIR2(std::string(path));
      }
      else
      {
        module->loadIR(std::string(path));
      }
      updateDisplayText();
      free(path);
    }

    osdialog_filters_free(filters);
  }

  void updateDisplayText()
  {
    if (module == nullptr)
    {
      text = "<No IR selected>";
      return;
    }

    std::string& path = isIR2 ? module->loaded_file_path2_ : module->loaded_file_path_;
    if (path.empty())
    {
      text = "<No IR selected>";
    }
    else
    {
      const size_t pos = path.find_last_of("/\\");
      std::string filename = (pos != std::string::npos) ? path.substr(pos + 1) : path;

      if (filename.length() > 20)
      {
        filename = filename.substr(0, 17) + "...";
      }
      text = filename;
    }
  }

  void step() override
  {
    updateDisplayText();
    app::LedDisplayChoice::step();
  }
};

struct OpcVcvIrWidget final : ModuleWidget
{
  explicit OpcVcvIrWidget(OpcVcvIr* module)
  {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance, "res/opc-vcv-ir-panel.svg")));

    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
    addChild(createWidget<ScrewSilver>(
        Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

    auto* fileDisplay = createWidget<IrFileDisplay>(mm2px(Vec(5.08, 12.0)));
    fileDisplay->box.size = mm2px(Vec(30.48, 10.0));
    fileDisplay->module = module;
    fileDisplay->isIR2 = false;
    addChild(fileDisplay);

    auto* fileDisplay2 = createWidget<IrFileDisplay>(mm2px(Vec(5.08, 23.0)));
    fileDisplay2->box.size = mm2px(Vec(30.48, 10.0));
    fileDisplay2->module = module;
    fileDisplay2->isIR2 = true;
    addChild(fileDisplay2);

    addChild(createLightCentered<MediumLight<RedLight>>(
        mm2px(Vec(5.08, 38.0)), module, static_cast<int>(OpcVcvIr::LightId::DynamicModeLight)));
    addChild(createLightCentered<MediumLight<BlueLight>>(
        mm2px(Vec(35.56, 38.0)), module, static_cast<int>(OpcVcvIr::LightId::SidechainLight)));

    addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(10.16, 50.0)), module,
                                                 static_cast<int>(OpcVcvIr::ParamId::GainParam)));
    addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(30.48, 50.0)), module,
                                                 static_cast<int>(OpcVcvIr::ParamId::BlendParam)));

    addParam(createParamCentered<RoundSmallBlackKnob>(
        mm2px(Vec(10.16, 65.0)), module, static_cast<int>(OpcVcvIr::ParamId::ThresholdParam)));
    addParam(createParamCentered<RoundSmallBlackKnob>(
        mm2px(Vec(30.48, 65.0)), module, static_cast<int>(OpcVcvIr::ParamId::RangeDbParam)));

    addParam(createParamCentered<RoundSmallBlackKnob>(
        mm2px(Vec(10.16, 78.0)), module, static_cast<int>(OpcVcvIr::ParamId::KneeWidthDbParam)));
    addParam(createParamCentered<RoundSmallBlackKnob>(
        mm2px(Vec(30.48, 78.0)), module, static_cast<int>(OpcVcvIr::ParamId::DetectionModeParam)));

    addParam(createParamCentered<RoundSmallBlackKnob>(
        mm2px(Vec(10.16, 91.0)), module, static_cast<int>(OpcVcvIr::ParamId::AttackTimeParam)));
    addParam(createParamCentered<RoundSmallBlackKnob>(
        mm2px(Vec(30.48, 91.0)), module, static_cast<int>(OpcVcvIr::ParamId::ReleaseTimeParam)));

    addParam(createParamCentered<RoundBlackKnob>(
        mm2px(Vec(20.32, 103.0)), module, static_cast<int>(OpcVcvIr::ParamId::OutputGainParam)));

    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 115.0)), module,
                                             static_cast<int>(OpcVcvIr::InputId::InputL)));
    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(15.24, 115.0)), module,
                                             static_cast<int>(OpcVcvIr::InputId::InputR)));
    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(25.4, 115.0)), module,
                                             static_cast<int>(OpcVcvIr::InputId::SidechainL)));
    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(33.02, 115.0)), module,
                                             static_cast<int>(OpcVcvIr::InputId::SidechainR)));

    addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(11.43, 125.0)), module,
                                               static_cast<int>(OpcVcvIr::OutputId::OutputL)));
    addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(29.21, 125.0)), module,
                                               static_cast<int>(OpcVcvIr::OutputId::OutputR)));
  }

  void appendContextMenu(Menu* menu) override
  {
    auto* module = dynamic_cast<OpcVcvIr*>(this->module);
    if (module == nullptr)
    {
      return;
    }

    menu->addChild(new MenuSeparator);

    menu->addChild(createBoolPtrMenuItem("Dynamic Mode", "", &module->dynamicModeEnabled_));
    menu->addChild(createBoolPtrMenuItem("Sidechain Enable", "", &module->sidechainEnabled_));

    menu->addChild(new MenuSeparator);

    menu->addChild(createMenuItem("Swap IR Order", "",
                                  [module]()
                                  {
                                    const std::string path1 = module->loaded_file_path_;
                                    const std::string path2 = module->loaded_file_path2_;

                                    if (!path2.empty())
                                      module->loadIR(path2);
                                    else
                                    {
                                      module->irProcessor_.clearImpulseResponse1();
                                      module->loaded_file_path_.clear();
                                    }

                                    if (!path1.empty())
                                      module->loadIR2(path1);
                                    else
                                    {
                                      module->irProcessor_.clearImpulseResponse2();
                                      module->loaded_file_path2_.clear();
                                    }
                                  }));
  }
};

Model* modelOpcVcvIr = createModel<OpcVcvIr, OpcVcvIrWidget>("opc-vcv-ir");