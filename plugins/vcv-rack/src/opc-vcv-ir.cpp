#include "opc-vcv-ir.hpp"

#include <dirent.h>
#include <osdialog.h>
#include <sys/stat.h>

#include <algorithm>
#include <cctype>
#include <vector>

static void drawRetroButton(NVGcontext* vg, float w, float h, const char* label, bool lit,
                            bool pressed, const std::string& fontPath)
{
  const NVGcolor bg = nvgRGB(0x1c, 0x1c, 0x30);
  const NVGcolor fg = nvgRGB(0xF0, 0x88, 0x30);
  const NVGcolor litBg = nvgRGBA(0xF0, 0x88, 0x30, 0x80);

  nvgBeginPath(vg);
  nvgRect(vg, 0.f, 0.f, w, h);
  nvgFillColor(vg, pressed ? fg : bg);
  nvgFill(vg);

  if (lit && !pressed)
  {
    nvgBeginPath(vg);
    nvgRect(vg, 0.f, 0.f, w, h);
    nvgFillColor(vg, litBg);
    nvgFill(vg);
  }

  nvgBeginPath(vg);
  nvgRect(vg, 0.5f, 0.5f, w - 1.f, h - 1.f);
  nvgStrokeColor(vg, fg);
  nvgStrokeWidth(vg, 1.5f);
  nvgStroke(vg);

  int fontId = nvgFindFont(vg, "Inconsolata");
  if (fontId < 0)
    fontId = nvgCreateFont(vg, "Inconsolata", fontPath.c_str());
  if (fontId >= 0)
    nvgFontFaceId(vg, fontId);

  nvgFontSize(vg, 9.f);
  nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
  nvgFillColor(vg, pressed ? bg : fg);
  nvgText(vg, w * 0.5f, h * 0.5f, label, nullptr);
}

static std::string getParentDir(const std::string& path)
{
  const size_t pos = path.find_last_of("/\\");
  return (pos != std::string::npos) ? path.substr(0, pos) : ".";
}

static std::string getFilename(const std::string& path)
{
  const size_t pos = path.find_last_of("/\\");
  return (pos != std::string::npos) ? path.substr(pos + 1) : path;
}

static bool isAudioExtension(const std::string& name)
{
  const size_t dot = name.rfind('.');
  if (dot == std::string::npos)
    return false;
  std::string ext = name.substr(dot);
  for (char& c : ext)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return ext == ".wav" || ext == ".aiff" || ext == ".aif" || ext == ".flac";
}

struct OpcTrimKnob : Trimpot
{
};

struct OpcIrEnableButton : app::Switch
{
  std::string label;
  std::string fontPath;

  void draw(const DrawArgs& args) override
  {
    engine::ParamQuantity* pq = getParamQuantity();
    const bool lit = pq ? pq->getValue() > 0.5f : false;
    drawRetroButton(args.vg, box.size.x, box.size.y, label.c_str(), lit, false, fontPath);
  }
};

struct OpcDetectModeButton : app::Switch
{
  std::string fontPath;

  void draw(const DrawArgs& args) override
  {
    engine::ParamQuantity* pq = getParamQuantity();
    const bool lit = pq ? pq->getValue() < 0.5f : true;
    drawRetroButton(args.vg, box.size.x, box.size.y, "PEAK", lit, false, fontPath);
  }
};

struct OpcPanelLabel : TransparentWidget
{
  std::string text;
  std::string fontPath;
  float fontSize = 9.f;
  NVGcolor color = nvgRGB(0x1a, 0x1a, 0x1a);
  int align = NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE;

  void draw(const DrawArgs& args) override
  {
    int fontId = nvgFindFont(args.vg, "Inconsolata");
    if (fontId < 0)
      fontId = nvgCreateFont(args.vg, "Inconsolata", fontPath.c_str());
    if (fontId >= 0)
      nvgFontFaceId(args.vg, fontId);

    nvgFontSize(args.vg, fontSize);
    nvgTextAlign(args.vg, align);
    nvgFillColor(args.vg, color);
    nvgText(args.vg, box.size.x * 0.5f, box.size.y * 0.5f, text.c_str(), nullptr);
  }
};

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
    else if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_RIGHT && module != nullptr)
    {
      e.consume(this);
      ui::Menu* menu = createMenu();
      menu->addChild(new MenuSeparator);
      menu->addChild(createMenuItem("Clear", "",
                                    [this]()
                                    {
                                      if (isIR2)
                                        module->clearIR2();
                                      else
                                        module->clearIR1();
                                    }));
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

      if (filename.length() > 30)
        filename = filename.substr(0, 27) + "...";
      text = filename;
    }
  }

  void step() override
  {
    updateDisplayText();
    app::LedDisplayChoice::step();
  }
};

struct OpcIrLoadButton : OpaqueWidget
{
  OpcVcvIr* module = nullptr;
  bool isIR2 = false;
  std::string fontPath;

  void draw(const DrawArgs& args) override
  {
    drawRetroButton(args.vg, box.size.x, box.size.y, "LOAD", false, false, fontPath);
  }

  void onButton(const ButtonEvent& e) override
  {
    if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT && module != nullptr)
    {
      e.consume(this);
      openFileDialog();
    }
  }

 private:
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
      free(path);
    }
    osdialog_filters_free(filters);
  }
};

struct OpcIrClearButton : OpaqueWidget
{
  OpcVcvIr* module = nullptr;
  bool isIR2 = false;
  std::string fontPath;

  void draw(const DrawArgs& args) override
  {
    drawRetroButton(args.vg, box.size.x, box.size.y, "CLR", false, false, fontPath);
  }

  void onButton(const ButtonEvent& e) override
  {
    if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT && module != nullptr)
    {
      e.consume(this);
      if (isIR2)
        module->clearIR2();
      else
        module->clearIR1();
    }
  }
};

template <int Direction>
struct OpcIrNavButton : OpaqueWidget
{
  OpcVcvIr* module = nullptr;
  bool isIR2 = false;
  std::string fontPath;

  void draw(const DrawArgs& args) override
  {
    const bool hasIr = module != nullptr &&
                       !(isIR2 ? module->loaded_file_path2_ : module->loaded_file_path_).empty();
    nvgGlobalAlpha(args.vg, hasIr ? 1.f : 0.4f);
    drawRetroButton(args.vg, box.size.x, box.size.y, Direction < 0 ? "<" : ">", false, false,
                    fontPath);
    nvgGlobalAlpha(args.vg, 1.f);
  }

  void onButton(const ButtonEvent& e) override
  {
    if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT && module != nullptr)
    {
      e.consume(this);
      loadAdjacentFile();
    }
  }

 private:
  void loadAdjacentFile()
  {
    const std::string& currentPath = isIR2 ? module->loaded_file_path2_ : module->loaded_file_path_;
    if (currentPath.empty())
      return;

    const std::string dir = getParentDir(currentPath);
    const std::string currentName = getFilename(currentPath);

    DIR* dp = opendir(dir.c_str());
    if (dp == nullptr)
    {
      WARN("OpcIrNavButton: could not open directory %s", dir.c_str());
      return;
    }

    std::vector<std::string> entries;
    struct dirent* ep = nullptr;
    while ((ep = readdir(dp)) != nullptr)
    {
      std::string name(ep->d_name);
      if (isAudioExtension(name))
        entries.push_back(name);
    }
    closedir(dp);

    if (entries.empty())
    {
      WARN("OpcIrNavButton: no audio files found in %s", dir.c_str());
      return;
    }

    std::sort(entries.begin(), entries.end());

    const auto it = std::find(entries.begin(), entries.end(), currentName);
    if (it == entries.end())
    {
      WARN("OpcIrNavButton: current file %s not found in directory listing", currentName.c_str());
      return;
    }

    const int idx = static_cast<int>(std::distance(entries.begin(), it));
    const int count = static_cast<int>(entries.size());
    const int newIdx = ((idx + Direction) % count + count) % count;

    const std::string newPath = dir + "/" + entries[static_cast<size_t>(newIdx)];
    INFO("OpcIrNavButton: loading adjacent file %s", newPath.c_str());
    if (isIR2)
      module->loadIR2(newPath);
    else
      module->loadIR(newPath);
  }
};

using OpcIrPrevButton = OpcIrNavButton<-1>;
using OpcIrNextButton = OpcIrNavButton<1>;

struct OpcMeterDisplay : OpaqueWidget
{
  OpcVcvIr* module = nullptr;
  std::string fontPath;

  void draw(const DrawArgs& args) override
  {
    const float levelDb = module ? module->currentInputLevelDb_.load() : -60.f;
    const float blend = module ? module->currentBlend_.load() : 0.f;

    const float w = box.size.x;
    const float h = box.size.y;
    const float barH = h * 0.5f - 1.f;

    const NVGcolor orange = nvgRGB(0xF0, 0x88, 0x30);
    const NVGcolor dark = nvgRGB(0x1c, 0x1c, 0x30);

    nvgBeginPath(args.vg);
    nvgRect(args.vg, 0.f, 0.f, w, h);
    nvgFillColor(args.vg, dark);
    nvgFill(args.vg);

    int fontId = nvgFindFont(args.vg, "Inconsolata");
    if (fontId < 0 && !fontPath.empty())
      fontId = nvgCreateFont(args.vg, "Inconsolata", fontPath.c_str());
    if (fontId >= 0)
      nvgFontFaceId(args.vg, fontId);
    nvgFontSize(args.vg, 8.f);
    nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgFillColor(args.vg, orange);
    nvgText(args.vg, 6.f, barH * 0.5f, "INPUT", nullptr);
    nvgText(args.vg, 6.f, h * 0.5f + 1.f + barH * 0.5f, "BLEND", nullptr);

    drawBar(args.vg, 0.f, barH, w, levelDb, false, orange, dark);
    drawBar(args.vg, h * 0.5f + 1.f, barH, w, blend, true, orange, dark);
  }

 private:
  static const int kSegments = 19;
  static const float kDbMin;
  static const float kDbMax;

  void drawBar(NVGcontext* vg, float y, float h, float w, float value, bool isCenterBalanced,
               NVGcolor fill, NVGcolor bg) const
  {
    nvgBeginPath(vg);
    nvgRect(vg, 0.f, y, w, h);
    nvgFillColor(vg, bg);
    nvgFill(vg);

    float fillFrac;
    float startX;
    float fillW;

    if (isCenterBalanced)
    {
      fillFrac = std::abs(value);
      fillFrac = fillFrac > 1.f ? 1.f : fillFrac;
      startX = (value >= 0.f) ? w * 0.5f : w * 0.5f * (1.f - fillFrac);
      fillW = w * 0.5f * fillFrac;
    }
    else
    {
      fillFrac = (value - kDbMin) / (kDbMax - kDbMin);
      fillFrac = fillFrac < 0.f ? 0.f : (fillFrac > 1.f ? 1.f : fillFrac);
      startX = 0.f;
      fillW = w * fillFrac;
    }

    if (fillW > 0.f)
    {
      nvgBeginPath(vg);
      nvgRect(vg, startX, y, fillW, h);
      nvgFillColor(vg, fill);
      nvgFill(vg);
    }

    const float segW = w / static_cast<float>(kSegments + 1);
    nvgStrokeColor(vg, bg);
    nvgStrokeWidth(vg, 1.5f);
    for (int i = 1; i <= kSegments; ++i)
    {
      const float sx = segW * static_cast<float>(i);
      nvgBeginPath(vg);
      nvgMoveTo(vg, sx, y);
      nvgLineTo(vg, sx, y + h);
      nvgStroke(vg);
    }
  }
};

const float OpcMeterDisplay::kDbMin = -60.f;
const float OpcMeterDisplay::kDbMax = 0.f;

struct OpcRetroButton : app::Switch
{
  std::string label;
  std::string fontPath;

  void draw(const DrawArgs& args) override
  {
    engine::ParamQuantity* pq = getParamQuantity();
    const bool lit = pq ? pq->getValue() > 0.5f : false;
    drawRetroButton(args.vg, box.size.x, box.size.y, label.c_str(), lit, false, fontPath);
  }
};

struct OpcSwapButton : OpaqueWidget
{
  OpcVcvIr* module = nullptr;
  std::string fontPath;
  bool pressed_ = false;

  void draw(const DrawArgs& args) override
  {
    drawRetroButton(args.vg, box.size.x, box.size.y, "SWAP", false, pressed_, fontPath);
  }

  void onDragStart(const DragStartEvent& e) override
  {
    pressed_ = true;
    OpaqueWidget::onDragStart(e);
  }

  void onDragEnd(const DragEndEvent& e) override
  {
    pressed_ = false;
    OpaqueWidget::onDragEnd(e);
  }

  void onButton(const ButtonEvent& e) override
  {
    if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT && module != nullptr)
    {
      e.consume(this);
      module->swapImpulseResponses();
    }
  }
};

struct OpcVcvIrWidget final : ModuleWidget
{
  explicit OpcVcvIrWidget(OpcVcvIr* module)
  {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance, "res/opc-vcv-ir-panel.svg")));

    const std::string fp = asset::plugin(pluginInstance, "res/font/Inconsolata_Condensed-Bold.ttf");

    addChild(createWidget<ScrewSilver>(mm2px(Vec(7.62f, 7.62f))));
    addChild(createWidget<ScrewSilver>(mm2px(Vec(134.62f, 7.62f))));
    addChild(createWidget<ScrewSilver>(mm2px(Vec(7.62f, 120.88f))));
    addChild(createWidget<ScrewSilver>(mm2px(Vec(134.62f, 120.88f))));

    auto makeLabel = [&](math::Vec pos, math::Vec size, const char* txt, float fs = 9.f)
    {
      auto* lbl = createWidget<OpcPanelLabel>(pos);
      lbl->box.size = size;
      lbl->text = txt;
      lbl->fontPath = fp;
      lbl->fontSize = fs;
      addChild(lbl);
    };

    auto makeLoadBtn = [&](math::Vec center, bool ir2)
    {
      auto* btn = createWidget<OpcIrLoadButton>(center - mm2px(Vec(4.0f, 3.0f)));
      btn->box.size = mm2px(Vec(8.0f, 6.0f));
      btn->module = module;
      btn->isIR2 = ir2;
      btn->fontPath = fp;
      addChild(btn);
    };

    auto makeClearBtn = [&](math::Vec center, bool ir2)
    {
      auto* btn = createWidget<OpcIrClearButton>(center - mm2px(Vec(4.0f, 3.0f)));
      btn->box.size = mm2px(Vec(8.0f, 6.0f));
      btn->module = module;
      btn->isIR2 = ir2;
      btn->fontPath = fp;
      addChild(btn);
    };

    auto makePrevBtn = [&](math::Vec center, bool ir2)
    {
      auto* btn = createWidget<OpcIrPrevButton>(center - mm2px(Vec(4.0f, 3.0f)));
      btn->box.size = mm2px(Vec(8.0f, 6.0f));
      btn->module = module;
      btn->isIR2 = ir2;
      btn->fontPath = fp;
      addChild(btn);
    };

    auto makeNextBtn = [&](math::Vec center, bool ir2)
    {
      auto* btn = createWidget<OpcIrNextButton>(center - mm2px(Vec(4.0f, 3.0f)));
      btn->box.size = mm2px(Vec(8.0f, 6.0f));
      btn->module = module;
      btn->isIR2 = ir2;
      btn->fontPath = fp;
      addChild(btn);
    };

    auto makeEnableBtn = [&](math::Vec center, bool ir2)
    {
      const auto paramId = static_cast<int>(ir2 ? OpcVcvIr::ParamId::IrBEnableParam
                                                : OpcVcvIr::ParamId::IrAEnableParam);
      auto* btn = createParam<OpcIrEnableButton>(center - mm2px(Vec(6.0f, 3.0f)), module, paramId);
      btn->box.size = mm2px(Vec(12.0f, 6.0f));
      btn->label = "ENBL";
      btn->fontPath = fp;
      addParam(btn);
    };

    // IR A row (Y-center = 10.0)
    // Control order: LOAD | CLR | < | > | ENBL | TRIM | LCD
    makeLoadBtn(mm2px(Vec(10.0f, 10.0f)), false);
    makeClearBtn(mm2px(Vec(19.0f, 10.0f)), false);
    makePrevBtn(mm2px(Vec(28.0f, 10.0f)), false);
    makeNextBtn(mm2px(Vec(37.0f, 10.0f)), false);
    makeEnableBtn(mm2px(Vec(49.0f, 10.0f)), false);
    addParam(createParamCentered<OpcTrimKnob>(
        mm2px(Vec(59.0f, 10.0f)), module, static_cast<int>(OpcVcvIr::ParamId::IrATrimGainParam)));

    auto* fileDisplayA = createWidget<IrFileDisplay>(mm2px(Vec(63.0f, 5.5f)));
    fileDisplayA->box.size = mm2px(Vec(74.0f, 9.0f));
    fileDisplayA->module = module;
    fileDisplayA->isIR2 = false;
    addChild(fileDisplayA);

    makeLabel(mm2px(Vec(1.0f, 3.5f)), mm2px(Vec(8.0f, 4.0f)), "IR A", 8.f);

    // IR B row (Y-center = 20.0)
    makeLoadBtn(mm2px(Vec(10.0f, 20.0f)), true);
    makeClearBtn(mm2px(Vec(19.0f, 20.0f)), true);
    makePrevBtn(mm2px(Vec(28.0f, 20.0f)), true);
    makeNextBtn(mm2px(Vec(37.0f, 20.0f)), true);
    makeEnableBtn(mm2px(Vec(49.0f, 20.0f)), true);
    addParam(createParamCentered<OpcTrimKnob>(
        mm2px(Vec(59.0f, 20.0f)), module, static_cast<int>(OpcVcvIr::ParamId::IrBTrimGainParam)));

    auto* fileDisplayB = createWidget<IrFileDisplay>(mm2px(Vec(63.0f, 15.5f)));
    fileDisplayB->box.size = mm2px(Vec(74.0f, 9.0f));
    fileDisplayB->module = module;
    fileDisplayB->isIR2 = true;
    addChild(fileDisplayB);

    makeLabel(mm2px(Vec(1.0f, 13.5f)), mm2px(Vec(8.0f, 4.0f)), "IR B", 8.f);

    // Mode row (Y-center = 29.0)
    {
      auto* dynBtn =
          createParam<OpcRetroButton>(mm2px(Vec(25.40f - 14.0f, 29.0f - 4.0f)), module,
                                      static_cast<int>(OpcVcvIr::ParamId::DynamicModeParam));
      dynBtn->box.size = mm2px(Vec(28.0f, 8.0f));
      dynBtn->label = "DYNAMIC";
      dynBtn->fontPath = fp;
      addParam(dynBtn);
    }

    {
      auto* swapBtn = createWidget<OpcSwapButton>(mm2px(Vec(71.12f - 14.0f, 29.0f - 4.0f)));
      swapBtn->box.size = mm2px(Vec(28.0f, 8.0f));
      swapBtn->module = module;
      swapBtn->fontPath = fp;
      addChild(swapBtn);
    }

    {
      auto* scBtn =
          createParam<OpcRetroButton>(mm2px(Vec(116.84f - 14.0f, 29.0f - 4.0f)), module,
                                      static_cast<int>(OpcVcvIr::ParamId::SidechainEnableParam));
      scBtn->box.size = mm2px(Vec(28.0f, 8.0f));
      scBtn->label = "SIDECHAIN";
      scBtn->fontPath = fp;
      addParam(scBtn);
    }

    // Meter display
    {
      auto* meter = createWidget<OpcMeterDisplay>(mm2px(Vec(5.0f, 33.5f)));
      meter->box.size = mm2px(Vec(132.24f, 14.0f));
      meter->module = module;
      meter->fontPath = fp;
      addChild(meter);
    }

    // Large knobs (Y-center = 56.5)
    addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(35.56f, 56.5f)), module,
                                                 static_cast<int>(OpcVcvIr::ParamId::BlendParam)));
    addParam(createParamCentered<RoundBlackKnob>(
        mm2px(Vec(106.68f, 56.5f)), module, static_cast<int>(OpcVcvIr::ParamId::OutputGainParam)));

    makeLabel(mm2px(Vec(25.56f, 48.0f)), mm2px(Vec(20.0f, 4.0f)), "BLEND", 8.f);
    makeLabel(mm2px(Vec(96.68f, 48.0f)), mm2px(Vec(20.0f, 4.0f)), "OUT GAIN", 8.f);

    // Small knobs row 1 (Y-center = 73.5)
    addParam(createParamCentered<RoundSmallBlackKnob>(
        mm2px(Vec(25.40f, 73.5f)), module, static_cast<int>(OpcVcvIr::ParamId::ThresholdParam)));
    addParam(createParamCentered<RoundSmallBlackKnob>(
        mm2px(Vec(71.12f, 73.5f)), module, static_cast<int>(OpcVcvIr::ParamId::RangeDbParam)));
    addParam(createParamCentered<RoundSmallBlackKnob>(
        mm2px(Vec(116.84f, 73.5f)), module, static_cast<int>(OpcVcvIr::ParamId::KneeWidthDbParam)));

    makeLabel(mm2px(Vec(15.40f, 69.0f)), mm2px(Vec(20.0f, 4.0f)), "THRESH", 8.f);
    makeLabel(mm2px(Vec(61.12f, 69.0f)), mm2px(Vec(20.0f, 4.0f)), "RANGE", 8.f);
    makeLabel(mm2px(Vec(106.84f, 69.0f)), mm2px(Vec(20.0f, 4.0f)), "KNEE", 8.f);

    // Small knobs row 2 (Y-center = 89.5)
    addParam(createParamCentered<RoundSmallBlackKnob>(
        mm2px(Vec(25.40f, 89.5f)), module, static_cast<int>(OpcVcvIr::ParamId::AttackTimeParam)));
    addParam(createParamCentered<RoundSmallBlackKnob>(
        mm2px(Vec(71.12f, 89.5f)), module, static_cast<int>(OpcVcvIr::ParamId::ReleaseTimeParam)));
    {
      auto* detectBtn =
          createParam<OpcDetectModeButton>(mm2px(Vec(116.84f - 10.0f, 89.5f - 4.0f)), module,
                                           static_cast<int>(OpcVcvIr::ParamId::DetectionModeParam));
      detectBtn->box.size = mm2px(Vec(20.0f, 8.0f));
      detectBtn->fontPath = fp;
      addParam(detectBtn);
    }

    makeLabel(mm2px(Vec(15.40f, 85.0f)), mm2px(Vec(20.0f, 4.0f)), "ATTACK", 8.f);
    makeLabel(mm2px(Vec(61.12f, 85.0f)), mm2px(Vec(20.0f, 4.0f)), "RELEASE", 8.f);
    makeLabel(mm2px(Vec(106.84f, 85.0f)), mm2px(Vec(20.0f, 4.0f)), "DETECT", 8.f);

    // Port row 1 (Y-center = 102.5)
    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(23.71f, 102.5f)), module,
                                             static_cast<int>(OpcVcvIr::InputId::AudioInL)));
    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(47.41f, 102.5f)), module,
                                             static_cast<int>(OpcVcvIr::InputId::AudioInR)));
    addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(71.12f, 102.5f)), module,
                                               static_cast<int>(OpcVcvIr::OutputId::OutputL)));
    addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(94.82f, 102.5f)), module,
                                               static_cast<int>(OpcVcvIr::OutputId::OutputR)));
    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(118.53f, 102.5f)), module,
                                             static_cast<int>(OpcVcvIr::InputId::SidechainIn)));

    makeLabel(mm2px(Vec(17.71f, 108.0f)), mm2px(Vec(12.0f, 4.0f)), "IN L", 8.f);
    makeLabel(mm2px(Vec(41.41f, 108.0f)), mm2px(Vec(12.0f, 4.0f)), "IN R", 8.f);
    makeLabel(mm2px(Vec(65.12f, 108.0f)), mm2px(Vec(12.0f, 4.0f)), "OUT L", 8.f);
    makeLabel(mm2px(Vec(88.82f, 108.0f)), mm2px(Vec(12.0f, 4.0f)), "OUT R", 8.f);
    makeLabel(mm2px(Vec(112.53f, 108.0f)), mm2px(Vec(12.0f, 4.0f)), "SC IN", 8.f);

    // Port row 2 (Y-center = 114.0)
    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(35.56f, 114.0f)), module,
                                             static_cast<int>(OpcVcvIr::InputId::ThresholdCvIn)));
    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(71.12f, 114.0f)), module,
                                             static_cast<int>(OpcVcvIr::InputId::BlendCvIn)));
    addInput(
        createInputCentered<PJ301MPort>(mm2px(Vec(106.68f, 114.0f)), module,
                                        static_cast<int>(OpcVcvIr::InputId::DynamicsEnableCvIn)));

    makeLabel(mm2px(Vec(29.56f, 119.5f)), mm2px(Vec(12.0f, 4.0f)), "THR CV", 8.f);
    makeLabel(mm2px(Vec(65.12f, 119.5f)), mm2px(Vec(12.0f, 4.0f)), "BLD CV", 8.f);
    makeLabel(mm2px(Vec(100.68f, 119.5f)), mm2px(Vec(12.0f, 4.0f)), "DYN EN", 8.f);
  }

  void appendContextMenu(Menu* menu) override
  {
    auto* module = dynamic_cast<OpcVcvIr*>(this->module);
    if (module == nullptr)
      return;

    menu->addChild(new MenuSeparator);

    menu->addChild(createCheckMenuItem(
        "Dynamic Mode", "",
        [module]()
        {
          return module->params[static_cast<int>(OpcVcvIr::ParamId::DynamicModeParam)].getValue() >
                 0.5f;
        },
        [module]()
        {
          auto& p = module->params[static_cast<int>(OpcVcvIr::ParamId::DynamicModeParam)];
          p.setValue(p.getValue() > 0.5f ? 0.f : 1.f);
        }));
    menu->addChild(createCheckMenuItem(
        "Sidechain Enable", "",
        [module]()
        {
          return module->params[static_cast<int>(OpcVcvIr::ParamId::SidechainEnableParam)]
                     .getValue() > 0.5f;
        },
        [module]()
        {
          auto& p = module->params[static_cast<int>(OpcVcvIr::ParamId::SidechainEnableParam)];
          p.setValue(p.getValue() > 0.5f ? 0.f : 1.f);
        }));

    menu->addChild(new MenuSeparator);

    menu->addChild(
        createMenuItem("Swap IR Order", "", [module]() { module->swapImpulseResponses(); }));
  }
};

Model* modelOpcVcvIr = createModel<OpcVcvIr, OpcVcvIrWidget>("opc-vcv-ir");
