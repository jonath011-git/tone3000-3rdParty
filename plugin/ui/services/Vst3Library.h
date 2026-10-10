// Persistent, per-user VST3 folder library shared by the header and insert tile.
#pragma once

#include <algorithm>
#include <vector>

#include "UiPrefs.h"

namespace t3k::ui::vst3library {

inline constexpr const char* kFoldersKey = "t3k.vst3LibraryFolders";
inline constexpr const char* kPluginsKey = "t3k.vst3LibraryPlugins";

inline std::vector<juce::File> readPaths(UiPrefs& prefs, const char* key) {
  std::vector<juce::File> result;
  const auto value = prefs.getJson(key);
  if (const auto* array = value.getArray()) {
    for (const auto& item : *array) {
      if (!item.isString()) continue;
      const juce::File file(item.toString());
      if (file.getFullPathName().isNotEmpty()) result.push_back(file);
    }
  }
  return result;
}

inline void writePaths(UiPrefs& prefs, const char* key, const std::vector<juce::File>& paths) {
  juce::var array{juce::Array<juce::var>{}};
  for (const auto& path : paths) array.append(path.getFullPathName());
  prefs.setJson(key, array);
}

inline std::vector<juce::File> folders(UiPrefs& prefs) {
  auto result = readPaths(prefs, kFoldersKey);
  result.erase(std::remove_if(result.begin(), result.end(),
                              [](const juce::File& file) { return !file.isDirectory(); }),
               result.end());
  return result;
}

inline std::vector<juce::File> plugins(UiPrefs& prefs) {
  auto result = readPaths(prefs, kPluginsKey);
  result.erase(std::remove_if(result.begin(), result.end(),
                              [](const juce::File& file) { return !file.exists(); }),
               result.end());
  return result;
}

inline void addFolder(UiPrefs& prefs, const juce::File& folder) {
  if (!folder.isDirectory()) return;
  auto list = folders(prefs);
  const auto path = folder.getFullPathName();
  for (const auto& existing : list)
    if (existing.getFullPathName().equalsIgnoreCase(path)) return;
  list.push_back(folder);
  writePaths(prefs, kFoldersKey, list);
}

inline void removeFolder(UiPrefs& prefs, size_t index) {
  auto list = folders(prefs);
  if (index >= list.size()) return;
  list.erase(list.begin() + static_cast<std::ptrdiff_t>(index));
  writePaths(prefs, kFoldersKey, list);
}

// Scan manually rather than using a recursive glob: a .vst3 bundle is itself a
// directory on some platforms and must not be scanned a second time internally.
inline void scanDirectory(const juce::File& folder, std::vector<juce::File>& found) {
  juce::Array<juce::File> children;
  folder.findChildFiles(children, juce::File::findFilesAndDirectories, false);
  for (const auto& child : children) {
    if (child.getFileName().endsWithIgnoreCase(".vst3")) {
      found.push_back(child);
    } else if (child.isDirectory()) {
      scanDirectory(child, found);
    }
  }
}

inline std::vector<juce::File> rescan(UiPrefs& prefs) {
  std::vector<juce::File> found;
  for (const auto& folder : folders(prefs)) scanDirectory(folder, found);

  std::sort(found.begin(), found.end(), [](const juce::File& a, const juce::File& b) {
    return a.getFileName().compareIgnoreCase(b.getFileName()) < 0;
  });
  found.erase(std::unique(found.begin(), found.end(), [](const juce::File& a, const juce::File& b) {
                return a.getFullPathName().equalsIgnoreCase(b.getFullPathName());
              }), found.end());
  writePaths(prefs, kPluginsKey, found);
  return found;
}

inline juce::String displayName(const juce::File& plugin) {
  auto name = plugin.getFileName();
  if (name.endsWithIgnoreCase(".vst3")) name = name.dropLastCharacters(5);
  return name;
}

}  // namespace t3k::ui::vst3library
