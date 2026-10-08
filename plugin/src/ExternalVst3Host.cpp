#include "ExternalVst3Host.h"

#include <algorithm>
#include <memory>

namespace {
std::shared_ptr<juce::AudioPluginFormatManager> getFormatManager() {
  // JUCE plugin instances may retain references to their creating format.
  // Keep the manager and its format objects alive for the process lifetime.
  static auto manager = [] {
    auto value = std::make_shared<juce::AudioPluginFormatManager>();
    value->addDefaultFormats();
    return value;
  }();
  return manager;
}
}  // namespace

ExternalVst3Host::ScanResult ExternalVst3Host::scanInstalledVst3() {
  ScanResult result;
#if JUCE_PLUGINHOST_VST3
  auto manager = getFormatManager();
  juce::AudioPluginFormat* vst3 = nullptr;
  for (int i = 0; i < manager->getNumFormats(); ++i) {
    auto* format = manager->getFormat(i);
    if (format != nullptr && format->getName().containsIgnoreCase("VST3")) {
      vst3 = format;
      break;
    }
  }

  if (vst3 == nullptr) {
    result.error = "JUCE VST3 hosting support is not available in this build.";
    return result;
  }

  const auto locations = vst3->searchPathsForPlugins(
      vst3->getDefaultLocationsToSearch(), true, true);
  for (int i = 0; i < locations.getNumPaths(); ++i) {
    juce::OwnedArray<juce::PluginDescription> descriptions;
    vst3->findAllTypesForFile(descriptions, locations[i]);
    for (const auto* description : descriptions) {
      if (description != nullptr && description->pluginFormatName == vst3->getName())
        result.plugins.push_back(*description);
    }
  }

  std::sort(result.plugins.begin(), result.plugins.end(),
            [](const auto& a, const auto& b) {
              const int byName = a.name.compareIgnoreCase(b.name);
              return byName == 0 ? a.manufacturerName.compareIgnoreCase(b.manufacturerName) < 0
                                 : byName < 0;
            });
  result.ok = true;
#else
  result.error = "VST3 hosting is disabled for this platform/build.";
#endif
  return result;
}

std::unique_ptr<ExternalVst3Host> ExternalVst3Host::create(
    const juce::PluginDescription& description,
    double sampleRate,
    int maximumBlockSize,
    juce::String& error) {
#if JUCE_PLUGINHOST_VST3
  if (sampleRate <= 0.0 || maximumBlockSize <= 0) {
    error = "Invalid sample rate or maximum block size.";
    return {};
  }

  auto manager = getFormatManager();
  auto* format = manager->getFormatForDescription(description);
  if (format == nullptr || !format->getName().containsIgnoreCase("VST3")) {
    error = "The selected plug-in is not a supported VST3 plug-in.";
    return {};
  }

  auto instance = manager->createPluginInstance(description, sampleRate,
                                                maximumBlockSize, error);
  if (instance == nullptr)
    return {};

  instance->setNonRealtime(false);
  instance->prepareToPlay(sampleRate, maximumBlockSize);
  return std::unique_ptr<ExternalVst3Host>(
      new ExternalVst3Host(description, std::move(instance)));
#else
  juce::ignoreUnused(description, sampleRate, maximumBlockSize);
  error = "VST3 hosting is disabled for this platform/build.";
  return {};
#endif
}

ExternalVst3Host::ExternalVst3Host(
    juce::PluginDescription description,
    std::unique_ptr<juce::AudioPluginInstance> plugin)
    : pluginDescription(std::move(description)), instance(std::move(plugin)) {}

void ExternalVst3Host::prepare(double sampleRate, int maximumBlockSize) {
  if (instance == nullptr || sampleRate <= 0.0 || maximumBlockSize <= 0)
    return;
  instance->releaseResources();
  instance->prepareToPlay(sampleRate, maximumBlockSize);
}

ExternalVst3Host::~ExternalVst3Host() {
  // Instance teardown may execute arbitrary third-party code. The owner must
  // destroy this object away from the real-time audio callback.
  if (instance != nullptr)
    instance->releaseResources();
}

void ExternalVst3Host::processBlock(juce::AudioBuffer<float>& audio,
                                    juce::MidiBuffer& midi) noexcept {
  if (instance == nullptr)
    return;
  try {
    instance->processBlock(audio, midi);
  } catch (...) {
    // Never let a third-party exception unwind through the host audio callback.
    audio.clear();
    midi.clear();
  }
}

juce::MemoryBlock ExternalVst3Host::saveState() const {
  juce::MemoryBlock state;
  if (instance != nullptr)
    instance->getStateInformation(state);
  return state;
}

bool ExternalVst3Host::restoreState(const void* data, int size, juce::String& error) {
  if (instance == nullptr || size < 0 || (size > 0 && data == nullptr)) {
    error = "Cannot restore state for an invalid plug-in instance or state buffer.";
    return false;
  }
  instance->setStateInformation(data, size);
  return true;
}

double ExternalVst3Host::getLatencySeconds() const noexcept {
  if (instance == nullptr || instance->getSampleRate() <= 0.0)
    return 0.0;
  return static_cast<double>(instance->getLatencySamples()) / instance->getSampleRate();
}

int ExternalVst3Host::getLatencySamples() const noexcept {
  return instance != nullptr ? instance->getLatencySamples() : 0;
}
