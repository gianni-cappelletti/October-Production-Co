#include <dr_wav.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "octobass-core/BassProcessor.hpp"

namespace
{

constexpr double kPi = 3.14159265358979323846;

struct BenchConfig
{
  double sampleRate = 48000.0;
  size_t blockSize = 128;
  double seconds = 20.0;
  float namQuality = 1.0f;
  float squash = 0.5f;
  int compressionMode = 0;
  int eqNodes = 4;
  std::string inputPath;
  std::string nam1Path;
  std::string nam2Path;
  std::string irPath;
};

void printUsage(const char* program)
{
  std::fprintf(stderr,
               "Usage: %s [options]\n"
               "  --nam1 <file.nam>     Model for NAM slot 1\n"
               "  --nam2 <file.nam>     Model for NAM slot 2\n"
               "  --ir <file.wav>       Cabinet impulse response\n"
               "  --quality <0..1>      NAM quality for both slots (default 1)\n"
               "  --input <file.wav>    Mono input, looped (default: synthesized bass)\n"
               "  --rate <hz>           Sample rate (default 48000)\n"
               "  --block <frames>      Block size (default 128)\n"
               "  --seconds <s>         Audio duration to process (default 20)\n"
               "  --squash <0..1>       Compressor amount (default 0.5)\n"
               "  --mode <0..3>         Compression mode (default 0)\n"
               "  --eq-nodes <0..16>    Active graphic EQ nodes (default 4)\n",
               program);
}

std::optional<BenchConfig> parseArgs(int argc, char** argv)
{
  BenchConfig config;
  for (int i = 1; i < argc; ++i)
  {
    const std::string arg = argv[i];
    if (arg == "--help" || arg == "-h")
      return std::nullopt;

    if (i + 1 >= argc)
    {
      std::fprintf(stderr, "Missing value for %s\n", arg.c_str());
      return std::nullopt;
    }
    const char* value = argv[++i];

    if (arg == "--nam1")
      config.nam1Path = value;
    else if (arg == "--nam2")
      config.nam2Path = value;
    else if (arg == "--ir")
      config.irPath = value;
    else if (arg == "--input")
      config.inputPath = value;
    else if (arg == "--quality")
      config.namQuality = std::strtof(value, nullptr);
    else if (arg == "--rate")
      config.sampleRate = std::strtod(value, nullptr);
    else if (arg == "--block")
      config.blockSize = std::strtoul(value, nullptr, 10);
    else if (arg == "--seconds")
      config.seconds = std::strtod(value, nullptr);
    else if (arg == "--squash")
      config.squash = std::strtof(value, nullptr);
    else if (arg == "--mode")
      config.compressionMode = std::atoi(value);
    else if (arg == "--eq-nodes")
      config.eqNodes = std::clamp(std::atoi(value), 0, octob::kGraphicEQNumNodes);
    else
    {
      std::fprintf(stderr, "Unknown option: %s\n", arg.c_str());
      return std::nullopt;
    }
  }

  if (config.sampleRate <= 0.0 || config.blockSize == 0 || config.seconds <= 0.0)
  {
    std::fprintf(stderr, "Sample rate, block size and duration must be positive\n");
    return std::nullopt;
  }
  return config;
}

std::vector<float> synthesizeBass(double sampleRate)
{
  constexpr double kFundamentalHz = 41.2;
  constexpr double kNoteSeconds = 0.5;
  const auto length = static_cast<size_t>(sampleRate * kNoteSeconds * 8.0);
  const auto noteLength = static_cast<size_t>(sampleRate * kNoteSeconds);

  std::vector<float> signal(length);
  for (size_t i = 0; i < length; ++i)
  {
    const size_t noteIndex = i / noteLength;
    const double t = static_cast<double>(i % noteLength) / sampleRate;
    const double freq = kFundamentalHz * ((noteIndex % 2 == 0) ? 1.0 : 1.5);
    const double phase = std::fmod(freq * t, 1.0);
    const double envelope = std::exp(-t * 4.0);
    signal[i] = static_cast<float>(0.5 * envelope * (2.0 * phase - 1.0) +
                                   0.1 * envelope * std::sin(2.0 * kPi * freq * 4.0 * t));
  }
  return signal;
}

std::optional<std::vector<float>> loadInput(const std::string& path)
{
  unsigned int channels = 0;
  unsigned int fileRate = 0;
  drwav_uint64 frameCount = 0;
  float* raw = drwav_open_file_and_read_pcm_frames_f32(path.c_str(), &channels, &fileRate,
                                                       &frameCount, nullptr);
  if (raw == nullptr || frameCount == 0)
  {
    std::fprintf(stderr, "Failed to read input '%s'\n", path.c_str());
    drwav_free(raw, nullptr);
    return std::nullopt;
  }

  std::vector<float> mono(static_cast<size_t>(frameCount));
  for (size_t i = 0; i < mono.size(); ++i)
    mono[i] = raw[i * channels];
  drwav_free(raw, nullptr);

  if (channels > 1)
    std::fprintf(stderr, "Input has %u channels, using the first\n", channels);
  std::fprintf(stderr, "Input: %s (%llu frames @ %u Hz)\n", path.c_str(),
               static_cast<unsigned long long>(frameCount), fileRate);
  return mono;
}

bool configureProcessor(octob::BassProcessor& processor, const BenchConfig& config)
{
  processor.setSampleRate(config.sampleRate);
  processor.setMaxBlockSize(config.blockSize);

  std::string error;
  const std::pair<octob::NamSlot, const std::string*> namPaths[] = {
      {octob::NamSlot::First, &config.nam1Path}, {octob::NamSlot::Second, &config.nam2Path}};
  for (const auto& [slot, path] : namPaths)
  {
    const int slotNumber = static_cast<int>(slot) + 1;
    if (path->empty())
    {
      std::fprintf(stderr, "NAM %d: empty\n", slotNumber);
      continue;
    }
    if (!processor.loadNamModel(slot, *path, error))
    {
      std::fprintf(stderr, "NAM %d: failed to load '%s': %s\n", slotNumber, path->c_str(),
                   error.c_str());
      return false;
    }
    processor.setNamQuality(slot, config.namQuality);
    std::fprintf(stderr, "NAM %d: %s (quality %.2f, %d levels)\n", slotNumber, path->c_str(),
                 config.namQuality, processor.getNamQualityLevels(slot));
  }

  if (config.irPath.empty())
    std::fprintf(stderr, "IR: empty\n");
  else if (processor.loadImpulseResponse(config.irPath, error))
    std::fprintf(stderr, "IR: %s (latency %d samples)\n", config.irPath.c_str(),
                 processor.getLatencySamples());
  else
  {
    std::fprintf(stderr, "IR: failed to load '%s': %s\n", config.irPath.c_str(), error.c_str());
    return false;
  }

  processor.setSquash(config.squash);
  processor.setCompressionMode(config.compressionMode);
  processor.setGateThreshold(-60.0f);

  constexpr float kNodeFrequenciesHz[] = {40.0f,   80.0f,   160.0f,  250.0f,  400.0f,  630.0f,
                                          800.0f,  1000.0f, 1600.0f, 2000.0f, 2500.0f, 3200.0f,
                                          4000.0f, 5000.0f, 6300.0f, 8000.0f};
  for (int node = 0; node < config.eqNodes; ++node)
    processor.setGraphicEQNode(node, true, kNodeFrequenciesHz[node],
                               (node % 2 == 0) ? 3.0f : -3.0f);
  return true;
}

double percentile(std::vector<double> values, double fraction)
{
  if (values.empty())
    return 0.0;
  const auto index = static_cast<size_t>(fraction * static_cast<double>(values.size() - 1));
  std::nth_element(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(index),
                   values.end());
  return values[index];
}

}  // namespace

int main(int argc, char** argv)
{
  const auto config = parseArgs(argc, argv);
  if (!config)
  {
    printUsage(argv[0]);
    return EXIT_FAILURE;
  }

  std::vector<float> input;
  if (config->inputPath.empty())
  {
    std::fprintf(stderr, "Input: synthesized bass\n");
    input = synthesizeBass(config->sampleRate);
  }
  else if (auto loaded = loadInput(config->inputPath))
    input = std::move(*loaded);
  else
    return EXIT_FAILURE;

  auto processor = std::make_unique<octob::BassProcessor>();
  if (!configureProcessor(*processor, *config))
    return EXIT_FAILURE;

  const size_t blockSize = config->blockSize;
  const auto totalBlocks =
      static_cast<size_t>(config->seconds * config->sampleRate / static_cast<double>(blockSize));
  const double blockBudgetSeconds = static_cast<double>(blockSize) / config->sampleRate;

  std::vector<float> inBlock(blockSize);
  std::vector<float> outBlock(blockSize);
  std::vector<double> blockLoads;
  blockLoads.reserve(totalBlocks);

  constexpr size_t kWarmupBlocks = 64;
  size_t readPos = 0;
  float peak = 0.0f;
  for (size_t block = 0; block < totalBlocks + kWarmupBlocks; ++block)
  {
    for (size_t i = 0; i < blockSize; ++i)
    {
      inBlock[i] = input[readPos];
      readPos = (readPos + 1) % input.size();
    }

    const auto start = std::chrono::steady_clock::now();
    processor->processMono(inBlock.data(), outBlock.data(), blockSize);
    const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start);

    if (block >= kWarmupBlocks)
      blockLoads.push_back(elapsed.count() / blockBudgetSeconds);
    for (const float sample : outBlock)
      peak = std::max(peak, std::fabs(sample));
  }

  if (!std::isfinite(peak))
  {
    std::fprintf(stderr, "Output contains non-finite samples\n");
    return EXIT_FAILURE;
  }

  double sum = 0.0;
  for (const double load : blockLoads)
    sum += load;
  const double mean = sum / static_cast<double>(blockLoads.size());
  const double p99 = percentile(blockLoads, 0.99);
  const double worst = *std::max_element(blockLoads.begin(), blockLoads.end());
  const auto overruns =
      std::count_if(blockLoads.begin(), blockLoads.end(), [](double load) { return load > 1.0; });

  std::printf(
      "rate=%.0f block=%zu blocks=%zu mean=%.1f%% p99=%.1f%% max=%.1f%% overruns=%td "
      "peak=%.3f\n",
      config->sampleRate, blockSize, blockLoads.size(), mean * 100.0, p99 * 100.0, worst * 100.0,
      overruns, peak);
  return EXIT_SUCCESS;
}
