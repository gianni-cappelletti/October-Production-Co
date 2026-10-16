#include <lv2/core/lv2.h>
#include <lv2/state/state.h>
#include <lv2/worker/worker.h>

#include <cstring>

#include "OctoBassLv2.hpp"
#include "Uris.hpp"

namespace
{

using octob::lv2::OctoBassPlugin;

OctoBassPlugin& plugin(LV2_Handle instance)
{
  return *static_cast<OctoBassPlugin*>(instance);
}

LV2_Handle instantiate(const LV2_Descriptor* /*descriptor*/, double sampleRate,
                       const char* /*bundlePath*/, const LV2_Feature* const* features)
{
  return OctoBassPlugin::create(sampleRate, features).release();
}

void connectPort(LV2_Handle instance, uint32_t port, void* data)
{
  plugin(instance).connectPort(port, data);
}

void activate(LV2_Handle instance)
{
  plugin(instance).activate();
}

void run(LV2_Handle instance, uint32_t frames)
{
  plugin(instance).run(frames);
}

void cleanup(LV2_Handle instance)
{
  delete static_cast<OctoBassPlugin*>(instance);
}

LV2_Worker_Status work(LV2_Handle instance, LV2_Worker_Respond_Function /*respond*/,
                       LV2_Worker_Respond_Handle /*handle*/, uint32_t size, const void* data)
{
  return plugin(instance).work(data, size);
}

LV2_Worker_Status workResponse(LV2_Handle /*instance*/, uint32_t /*size*/, const void* /*data*/)
{
  return LV2_WORKER_SUCCESS;
}

LV2_State_Status save(LV2_Handle instance, LV2_State_Store_Function store, LV2_State_Handle handle,
                      uint32_t /*flags*/, const LV2_Feature* const* features)
{
  return plugin(instance).save(store, handle, features);
}

LV2_State_Status restore(LV2_Handle instance, LV2_State_Retrieve_Function retrieve,
                         LV2_State_Handle handle, uint32_t /*flags*/,
                         const LV2_Feature* const* features)
{
  return plugin(instance).restore(retrieve, handle, features);
}

const LV2_Worker_Interface workerInterface = {work, workResponse, nullptr};
const LV2_State_Interface stateInterface = {save, restore};

const void* extensionData(const char* uri)
{
  if (std::strcmp(uri, LV2_WORKER__interface) == 0)
    return &workerInterface;
  if (std::strcmp(uri, LV2_STATE__interface) == 0)
    return &stateInterface;
  return nullptr;
}

const LV2_Descriptor descriptor = {
    octob::lv2::PluginUri, instantiate, connectPort, activate, run, nullptr, cleanup, extensionData,
};

}  // namespace

LV2_SYMBOL_EXPORT const LV2_Descriptor* lv2_descriptor(uint32_t index)
{
  return index == 0 ? &descriptor : nullptr;
}
