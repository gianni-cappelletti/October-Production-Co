#include "opc-vcv-ir.hpp"

#include <osdialog.h>

struct IrFileDisplay : app::LedDisplayChoice
{
  OpcVcvIr* module = nullptr;
  bool isIR2;

  IrFileDisplay(bool ir2 = false) : isIR2(ir2)
  {
    fontPath = asset::plugin(pluginInstance, "res/font/Inconsolata_Condensed-Bold.ttf");
    color = nvgRGB(0x1c, 0x1c, 0x30);
    bgColor = nvgRGB(0xF0, 0x88, 0x30);
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
        module->loadIR2(std::string(path));
      else
        module->loadIR(std::string(path));
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
        filename = filename.substr(0, 17) + "...";
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
                                                 static_cast<int>(OpcVcvIr::ParamId::BlendParam)));
    addParam(createParamCentered<RoundBlackKnob>(
        mm2px(Vec(30.48, 50.0)), module, static_cast<int>(OpcVcvIr::ParamId::OutputGainParam)));

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

    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 115.0)), module,
                                             static_cast<int>(OpcVcvIr::InputId::AudioInL)));
    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(15.24, 115.0)), module,
                                             static_cast<int>(OpcVcvIr::InputId::AudioInR)));
    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(25.4, 115.0)), module,
                                             static_cast<int>(OpcVcvIr::InputId::SidechainIn)));

    addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(11.43, 125.0)), module,
                                               static_cast<int>(OpcVcvIr::OutputId::OutputL)));
    addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(29.21, 125.0)), module,
                                               static_cast<int>(OpcVcvIr::OutputId::OutputR)));
  }

  void appendContextMenu(Menu* menu) override
  {
    auto* module = dynamic_cast<OpcVcvIr*>(this->module);
    if (module == nullptr)
      return;

    menu->addChild(new MenuSeparator);

    const int dynIdx = static_cast<int>(OpcVcvIr::ParamId::DynamicModeParam);
    const int scIdx = static_cast<int>(OpcVcvIr::ParamId::SidechainEnableParam);

    menu->addChild(createCheckMenuItem(
        "Dynamic Mode", "", [module, dynIdx]() { return module->params[dynIdx].getValue() > 0.5f; },
        [module, dynIdx]()
        {
          const bool on = module->params[dynIdx].getValue() > 0.5f;
          module->params[dynIdx].setValue(on ? 0.f : 1.f);
        }));
    menu->addChild(createCheckMenuItem(
        "Sidechain Enable", "",
        [module, scIdx]() { return module->params[scIdx].getValue() > 0.5f; },
        [module, scIdx]()
        {
          const bool on = module->params[scIdx].getValue() > 0.5f;
          module->params[scIdx].setValue(on ? 0.f : 1.f);
        }));

    menu->addChild(new MenuSeparator);

    menu->addChild(
        createMenuItem("Swap IR Order", "", [module]() { module->swapImpulseResponses(); }));
  }
};

Model* modelOpcVcvIr = createModel<OpcVcvIr, OpcVcvIrWidget>("opc-vcv-ir");
