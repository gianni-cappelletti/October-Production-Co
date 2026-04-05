#pragma once

// Minimal Rack API stub for headless unit testing of OpcVcvIr.
// Provides just enough surface area to compile and run the module struct
// without a running Rack engine or GUI.

#include <cstdint>
#include <map>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Logging macros (no-op in tests)
// ---------------------------------------------------------------------------
// NOLINTBEGIN(cppcoreguidelines-macro-usage)
#define INFO(...) (void)0
#define WARN(...) (void)0
// NOLINTEND(cppcoreguidelines-macro-usage)

// ---------------------------------------------------------------------------
// Minimal jansson-compatible JSON types
// ---------------------------------------------------------------------------
// These names must exactly match the jansson C API used in opc-vcv-ir.cpp.
// NOLINTBEGIN(readability-identifier-naming)
enum json_type_t : uint8_t
{
  JSON_OBJECT_T,
  JSON_STRING_T
};

struct json_t
{
  json_type_t type;
  std::string strValue;
  std::map<std::string, json_t*> children;

  ~json_t()
  {
    for (auto& kv : children)
      delete kv.second;
  }

  json_t(const json_t&) = delete;
  json_t& operator=(const json_t&) = delete;
  json_t(json_t&&) = delete;
  json_t& operator=(json_t&&) = delete;
};

inline json_t* json_object()
{
  return new json_t{JSON_OBJECT_T};
}

inline json_t* json_string(const char* s)
{
  auto* j = new json_t{JSON_STRING_T};
  j->strValue = (s != nullptr) ? s : "";
  return j;
}

inline void json_object_set_new(json_t* obj, const char* key, json_t* val)
{
  if (obj != nullptr && key != nullptr && val != nullptr)
    obj->children[key] = val;
}

inline json_t* json_object_get(const json_t* obj, const char* key)
{
  if (obj == nullptr || key == nullptr)
    return nullptr;
  auto it = obj->children.find(key);
  return (it != obj->children.end()) ? it->second : nullptr;
}

inline bool json_is_string(const json_t* j)
{
  return j != nullptr && j->type == JSON_STRING_T;
}

inline const char* json_string_value(const json_t* j)
{
  return (j != nullptr && j->type == JSON_STRING_T) ? j->strValue.c_str() : "";
}

inline void json_decref(json_t* j)
{
  delete j;
}
// NOLINTEND(readability-identifier-naming)

// ---------------------------------------------------------------------------
// Rack namespace stubs
// ---------------------------------------------------------------------------
namespace rack
{

struct ParamQuantity
{
  bool snapEnabled = false;
};

struct Param
{
  float value = 0.f;

  [[nodiscard]] float getValue() const { return value; }
  void setValue(float v) { value = v; }
};

struct Input
{
  float voltage = 0.f;
  bool connected = false;

  [[nodiscard]] bool isConnected() const { return connected; }
  [[nodiscard]] float getVoltage() const { return voltage; }
};

struct Output
{
  float voltage = 0.f;

  void setVoltage(float v) { voltage = v; }
  [[nodiscard]] float getVoltage() const { return voltage; }
};

struct Light
{
  float brightness = 0.f;

  void setBrightness(float b) { brightness = b; }
};

struct ProcessArgs
{
  float sampleRate = 44100.f;
  float sampleTime = 1.f / 44100.f;
};

struct SampleRateChangeEvent
{
  float sampleRate = 44100.f;
};

template <typename T>
T clamp(T x, T lo, T hi)
{
  if (x < lo)
    return lo;
  if (x > hi)
    return hi;
  return x;
}

struct Module
{
  std::vector<Param> params;
  std::vector<Input> inputs;
  std::vector<Output> outputs;
  std::vector<Light> lights;
  std::vector<ParamQuantity*> paramQuantities;

  virtual ~Module()
  {
    for (auto* pq : paramQuantities)
      delete pq;
  }

  Module() = default;
  Module(const Module&) = delete;
  Module& operator=(const Module&) = delete;
  Module(Module&&) = delete;
  Module& operator=(Module&&) = delete;

  void config(int numParams, int numInputs, int numOutputs, int numLights)
  {
    params.resize(static_cast<size_t>(numParams));
    inputs.resize(static_cast<size_t>(numInputs));
    outputs.resize(static_cast<size_t>(numOutputs));
    lights.resize(static_cast<size_t>(numLights));
    paramQuantities.resize(static_cast<size_t>(numParams), nullptr);
    for (int i = 0; i < numParams; ++i)
      paramQuantities[static_cast<size_t>(i)] = new ParamQuantity();
  }

  void configParam(int id, float /*minVal*/, float /*maxVal*/, float defaultVal,
                   const std::string& /*label*/ = "", const std::string& /*unit*/ = "",
                   float /*displayBase*/ = 0.f, float /*displayMultiplier*/ = 1.f)
  {
    if (id >= 0 && id < static_cast<int>(params.size()))
      params[static_cast<size_t>(id)].value = defaultVal;
  }

  void configSwitch(int id, float /*minVal*/, float /*maxVal*/, float defaultVal,
                    const std::string& /*label*/ = "",
                    const std::vector<std::string>& /*labels*/ = {})
  {
    if (id >= 0 && id < static_cast<int>(params.size()))
      params[static_cast<size_t>(id)].value = defaultVal;
  }

  void configInput(int /*id*/, const std::string& /*label*/ = "") {}
  void configOutput(int /*id*/, const std::string& /*label*/ = "") {}
  void configLight(int /*id*/, const std::string& /*label*/ = "") {}

  virtual void process(const ProcessArgs& /*args*/) {}
  virtual void onSampleRateChange(const SampleRateChangeEvent& /*e*/) {}
  virtual json_t* dataToJson() { return json_object(); }
  virtual void dataFromJson(json_t* /*rootJ*/) {}
};

}  // namespace rack

using namespace rack;
