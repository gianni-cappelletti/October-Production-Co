#include <lilv/lilv.h>
#include <lv2/atom/forge.h>
#include <lv2/atom/util.h>
#include <lv2/buf-size/buf-size.h>
#include <lv2/log/log.h>
#include <lv2/options/options.h>
#include <lv2/patch/patch.h>
#include <lv2/urid/urid.h>
#include <lv2/worker/worker.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <map>
#include <string>
#include <vector>

namespace
{

constexpr double SampleRate = 48000.0;
constexpr size_t AtomBufferBytes = 64 * 1024;

struct Options
{
  std::string pluginUri;
  std::string fileParameterUri;
  std::vector<std::string> modelPaths;
  int blockSize = 128;
  double seconds = 20.0;
  std::map<std::string, float> controls;
  std::vector<std::pair<std::string, std::string>> files;
};

class Host
{
 public:
  LV2_URID map(const char* uri)
  {
    const auto found = ids_.find(uri);
    if (found != ids_.end())
      return found->second;
    uris_.emplace_back(uri);
    ids_[uri] = static_cast<LV2_URID>(uris_.size());
    return static_cast<LV2_URID>(uris_.size());
  }

  const char* unmap(LV2_URID id) const
  {
    return id == 0 || id > uris_.size() ? nullptr : uris_[id - 1].c_str();
  }

  static LV2_URID mapCallback(LV2_URID_Map_Handle handle, const char* uri)
  {
    return static_cast<Host*>(handle)->map(uri);
  }

  static const char* unmapCallback(LV2_URID_Unmap_Handle handle, LV2_URID id)
  {
    return static_cast<Host*>(handle)->unmap(id);
  }

  static LV2_Worker_Status schedule(LV2_Worker_Schedule_Handle handle, uint32_t size,
                                    const void* data)
  {
    const auto* bytes = static_cast<const uint8_t*>(data);
    static_cast<Host*>(handle)->work_.emplace_back(bytes, bytes + size);
    return LV2_WORKER_SUCCESS;
  }

  static LV2_Worker_Status respond(LV2_Worker_Respond_Handle handle, uint32_t size,
                                   const void* data)
  {
    const auto* bytes = static_cast<const uint8_t*>(data);
    static_cast<Host*>(handle)->responses_.emplace_back(bytes, bytes + size);
    return LV2_WORKER_SUCCESS;
  }

  static int logVPrintf(LV2_Log_Handle, LV2_URID, const char* fmt, va_list args)
  {
    return std::vfprintf(stderr, fmt, args);
  }

  static int logPrintf(LV2_Log_Handle handle, LV2_URID type, const char* fmt, ...)
  {
    va_list args;
    va_start(args, fmt);
    const int written = logVPrintf(handle, type, fmt, args);
    va_end(args);
    return written;
  }

  void serviceWorker(LV2_Handle instance, const LV2_Worker_Interface* worker)
  {
    if (worker == nullptr)
      return;
    while (!work_.empty())
    {
      const auto job = std::move(work_.front());
      work_.pop_front();
      worker->work(instance, &Host::respond, this, static_cast<uint32_t>(job.size()), job.data());
    }
    while (!responses_.empty())
    {
      const auto response = std::move(responses_.front());
      responses_.pop_front();
      worker->work_response(instance, static_cast<uint32_t>(response.size()), response.data());
    }
    if (worker->end_run != nullptr)
      worker->end_run(instance);
  }

 private:
  std::vector<std::string> uris_;
  std::map<std::string, LV2_URID> ids_;
  std::deque<std::vector<uint8_t>> work_;
  std::deque<std::vector<uint8_t>> responses_;
};

bool parseArgs(int argc, char** argv, Options& options)
{
  for (int i = 1; i + 1 < argc; i += 2)
  {
    const std::string arg = argv[i];
    const std::string value = argv[i + 1];
    if (arg == "--uri")
      options.pluginUri = value;
    else if (arg == "--param")
      options.fileParameterUri = value;
    else if (arg == "--model")
      options.modelPaths.push_back(value);
    else if (arg == "--block")
      options.blockSize = std::atoi(value.c_str());
    else if (arg == "--seconds")
      options.seconds = std::atof(value.c_str());
    else if (arg == "--file")
    {
      const auto equals = value.find('=');
      if (equals == std::string::npos)
        return false;
      options.files.emplace_back(value.substr(0, equals), value.substr(equals + 1));
    }
    else if (arg == "--set")
    {
      const auto equals = value.find('=');
      if (equals == std::string::npos)
        return false;
      options.controls[value.substr(0, equals)] = std::strtof(value.c_str() + equals + 1, nullptr);
    }
    else
      return false;
  }
  return !options.pluginUri.empty();
}

std::vector<std::string> writableParameters(LilvWorld* world, const LilvPlugin* plugin)
{
  LilvNode* writable = lilv_new_uri(world, LV2_PATCH__writable);
  LilvNodes* nodes = lilv_world_find_nodes(world, lilv_plugin_get_uri(plugin), writable, nullptr);
  std::vector<std::string> result;
  LILV_FOREACH(nodes, it, nodes)
  result.emplace_back(lilv_node_as_uri(lilv_nodes_get(nodes, it)));
  lilv_nodes_free(nodes);
  lilv_node_free(writable);
  std::sort(result.begin(), result.end());
  return result;
}

void writePatchSets(Host& host, std::vector<uint8_t>& buffer,
                    const std::vector<std::pair<std::string, std::string>>& sets)
{
  LV2_URID_Map map{&host, &Host::mapCallback};
  LV2_Atom_Forge forge;
  lv2_atom_forge_init(&forge, &map);
  lv2_atom_forge_set_buffer(&forge, buffer.data(), buffer.size());
  LV2_Atom_Forge_Frame sequence;
  lv2_atom_forge_sequence_head(&forge, &sequence, 0);
  for (const auto& [parameter, path] : sets)
  {
    LV2_Atom_Forge_Frame object;
    lv2_atom_forge_frame_time(&forge, 0);
    lv2_atom_forge_object(&forge, &object, 0, host.map(LV2_PATCH__Set));
    lv2_atom_forge_key(&forge, host.map(LV2_PATCH__property));
    lv2_atom_forge_urid(&forge, host.map(parameter.c_str()));
    lv2_atom_forge_key(&forge, host.map(LV2_PATCH__value));
    lv2_atom_forge_path(&forge, path.c_str(), static_cast<uint32_t>(path.size()));
    lv2_atom_forge_pop(&forge, &object);
  }
  lv2_atom_forge_pop(&forge, &sequence);
}

void prepareNotify(Host& host, std::vector<uint8_t>& buffer)
{
  auto* atom = reinterpret_cast<LV2_Atom*>(buffer.data());
  atom->size = static_cast<uint32_t>(buffer.size() - sizeof(LV2_Atom));
  atom->type = host.map(LV2_ATOM__Chunk);
}

float bassSample(size_t frame)
{
  const double t = static_cast<double>(frame % 24000) / SampleRate;
  const double freq = (frame / 24000) % 2 == 0 ? 41.2 : 61.8;
  const double phase = std::fmod(freq * t, 1.0);
  return static_cast<float>(0.5 * std::exp(-t * 4.0) * (2.0 * phase - 1.0));
}

}  // namespace

int main(int argc, char** argv)
{
  Options options;
  if (!parseArgs(argc, argv, options))
  {
    std::fprintf(
        stderr,
        "Usage: %s --uri <plugin> [--model <file>] [--file <param-uri>=<path> ...] [--block 128] "
        "[--seconds 20] [--set symbol=value ...]\n",
        argv[0]);
    return 1;
  }

  LilvWorld* world = lilv_world_new();
  lilv_world_load_all(world);
  LilvNode* uri = lilv_new_uri(world, options.pluginUri.c_str());
  const LilvPlugin* plugin = lilv_plugins_get_by_uri(lilv_world_get_all_plugins(world), uri);
  if (plugin == nullptr)
  {
    std::fprintf(stderr, "Plugin not found: %s\n", options.pluginUri.c_str());
    return 1;
  }

  Host host;
  LV2_URID_Map map{&host, &Host::mapCallback};
  LV2_URID_Unmap unmap{&host, &Host::unmapCallback};
  LV2_Worker_Schedule schedule{&host, &Host::schedule};
  LV2_Log_Log log{&host, &Host::logPrintf, &Host::logVPrintf};
  int32_t blockLength = options.blockSize;
  float sampleRate = static_cast<float>(SampleRate);
  const LV2_URID atomInt = host.map(LV2_ATOM__Int);
  LV2_Options_Option hostOptions[] = {
      {LV2_OPTIONS_INSTANCE, 0, host.map(LV2_BUF_SIZE__maxBlockLength), sizeof(int32_t), atomInt,
       &blockLength},
      {LV2_OPTIONS_INSTANCE, 0, host.map(LV2_BUF_SIZE__nominalBlockLength), sizeof(int32_t),
       atomInt, &blockLength},
      {LV2_OPTIONS_INSTANCE, 0, host.map("http://lv2plug.in/ns/ext/parameters#sampleRate"),
       sizeof(float), host.map(LV2_ATOM__Float), &sampleRate},
      {LV2_OPTIONS_INSTANCE, 0, 0, 0, 0, nullptr},
  };
  const LV2_Feature mapFeature{LV2_URID__map, &map};
  const LV2_Feature unmapFeature{LV2_URID__unmap, &unmap};
  const LV2_Feature scheduleFeature{LV2_WORKER__schedule, &schedule};
  const LV2_Feature logFeature{LV2_LOG__log, &log};
  const LV2_Feature optionsFeature{LV2_OPTIONS__options, hostOptions};
  const LV2_Feature boundedFeature{LV2_BUF_SIZE__boundedBlockLength, nullptr};
  const LV2_Feature* features[] = {&mapFeature,     &unmapFeature,   &scheduleFeature, &logFeature,
                                   &optionsFeature, &boundedFeature, nullptr};

  LilvInstance* instance = lilv_plugin_instantiate(plugin, SampleRate, features);
  if (instance == nullptr)
  {
    std::fprintf(stderr, "Failed to instantiate %s\n", options.pluginUri.c_str());
    return 1;
  }
  const auto* worker = static_cast<const LV2_Worker_Interface*>(
      lilv_instance_get_extension_data(instance, LV2_WORKER__interface));

  const auto block = static_cast<size_t>(options.blockSize);
  std::vector<float> input(block);
  std::vector<float> output(block);
  std::vector<float> scratch(block);
  std::vector<uint8_t> control(AtomBufferBytes);
  std::vector<uint8_t> notify(AtomBufferBytes);
  const uint32_t numPorts = lilv_plugin_get_num_ports(plugin);
  std::vector<float> controls(numPorts, 0.0f);
  std::vector<float> minimums(numPorts), maximums(numPorts), defaults(numPorts);
  lilv_plugin_get_port_ranges_float(plugin, minimums.data(), maximums.data(), defaults.data());

  LilvNode* audioClass = lilv_new_uri(world, LV2_CORE__AudioPort);
  LilvNode* controlClass = lilv_new_uri(world, LV2_CORE__ControlPort);
  LilvNode* atomClass = lilv_new_uri(world, LV2_ATOM__AtomPort);
  LilvNode* inputClass = lilv_new_uri(world, LV2_CORE__InputPort);
  bool firstAudioInput = true;
  for (uint32_t index = 0; index < numPorts; ++index)
  {
    const LilvPort* port = lilv_plugin_get_port_by_index(plugin, index);
    const bool isInput = lilv_port_is_a(plugin, port, inputClass);
    if (lilv_port_is_a(plugin, port, audioClass))
    {
      float* buffer = isInput ? (firstAudioInput ? input.data() : scratch.data()) : (output.data());
      if (isInput)
        firstAudioInput = false;
      lilv_instance_connect_port(instance, index, buffer);
    }
    else if (lilv_port_is_a(plugin, port, controlClass))
    {
      const std::string symbol = lilv_node_as_string(lilv_port_get_symbol(plugin, port));
      controls[index] = std::isnan(defaults[index]) ? 0.0f : defaults[index];
      if (const auto found = options.controls.find(symbol); found != options.controls.end())
        controls[index] = found->second;
      lilv_instance_connect_port(instance, index, &controls[index]);
    }
    else if (lilv_port_is_a(plugin, port, atomClass))
    {
      lilv_instance_connect_port(instance, index, isInput ? control.data() : notify.data());
    }
  }

  const auto parameters = writableParameters(world, plugin);
  std::vector<std::pair<std::string, std::string>> sets = options.files;
  for (size_t i = 0; i < options.modelPaths.size(); ++i)
  {
    const std::string parameter = !options.fileParameterUri.empty() && i == 0
                                      ? options.fileParameterUri
                                      : (i < parameters.size() ? parameters[i] : std::string());
    if (parameter.empty())
    {
      std::fprintf(stderr, "No writable parameter for %s\n", options.modelPaths[i].c_str());
      return 1;
    }
    std::fprintf(stderr, "Setting %s = %s\n", parameter.c_str(), options.modelPaths[i].c_str());
    sets.emplace_back(parameter, options.modelPaths[i]);
  }

  lilv_instance_activate(instance);
  size_t frame = 0;
  const auto runBlock = [&](bool timed, std::vector<double>& loads)
  {
    for (size_t i = 0; i < block; ++i)
      input[i] = bassSample(frame + i);
    frame += block;
    prepareNotify(host, notify);
    const auto start = std::chrono::steady_clock::now();
    lilv_instance_run(instance, static_cast<uint32_t>(block));
    const auto elapsed = std::chrono::steady_clock::now() - start;
    if (timed)
      loads.push_back(std::chrono::duration<double>(elapsed).count() * SampleRate /
                      static_cast<double>(block));
    writePatchSets(host, control, {});
    host.serviceWorker(lilv_instance_get_handle(instance), worker);
  };

  std::vector<double> loads;
  writePatchSets(host, control, sets);
  for (int i = 0; i < 400; ++i)
    runBlock(false, loads);

  const auto blocks =
      static_cast<size_t>(options.seconds * SampleRate / static_cast<double>(block));
  loads.reserve(blocks);
  for (size_t i = 0; i < blocks; ++i)
    runBlock(true, loads);

  float peak = 0.0f;
  for (const float sample : output)
    peak = std::max(peak, std::fabs(sample));

  double sum = 0.0;
  for (const double load : loads)
    sum += load;
  std::vector<double> sorted = loads;
  std::sort(sorted.begin(), sorted.end());
  std::printf("mean=%.2f%% p99=%.2f%% max=%.2f%% lastBlockPeak=%.3f\n",
              100.0 * sum / static_cast<double>(loads.size()),
              100.0 * sorted[sorted.size() * 99 / 100], 100.0 * sorted.back(), peak);

  lilv_instance_deactivate(instance);
  lilv_instance_free(instance);
  lilv_node_free(audioClass);
  lilv_node_free(controlClass);
  lilv_node_free(atomClass);
  lilv_node_free(inputClass);
  lilv_node_free(uri);
  lilv_world_free(world);
  return 0;
}
