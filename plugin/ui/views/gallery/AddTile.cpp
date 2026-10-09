#include "AddTile.h"

#include <memory>

#include "GalleryGeometry.h"
#include "core/Help.h"
#include "core/Icons.h"
#include "core/Paint.h"
#include "core/Theme.h"

namespace t3k::ui {

AddTile::Routing AddTile::routingFor(int index, int count) {
  if (count <= 1) return Routing::none;
  if (index == 0) return Routing::right;
  if (index == count - 1) return Routing::left;
  return Routing::both;
}

AddTile::AddTile(Services& services, std::string blockId, int size)
    : GalleryTile(services, std::move(blockId), size) {
  setHelpText(help::text(help::Key::addTile));
  setTitle("Add Tone");
}

void AddTile::setRouting(Routing routing) {
  if (routing_ == routing) return;
  routing_ = routing;
  repaint();
}

void AddTile::setCanPaste(bool canPaste) { canPaste_ = canPaste; }

void AddTile::open() {
  if (onAdd) onAdd(blockId());
}

std::vector<ContextMenu::Item> AddTile::menuItems() {
  std::vector<ContextMenu::Item> items{
      {"Paste", Icon::ClipboardPaste, help::Key::pasteBlock,
       [this] { if (onPaste) onPaste(blockId()); }, /*disabled=*/!canPaste_},
      {"Load VST3", Icon::File, help::Key::loadFileTile,
       [this] {
         vst3Chooser_ = std::make_unique<juce::FileChooser>(
             "Choose a VST3 plug-in", juce::File(), "*.vst3");
         auto flags = juce::FileBrowserComponent::openMode |
                      juce::FileBrowserComponent::canSelectFiles |
                      juce::FileBrowserComponent::canSelectDirectories;
         // JUCE invokes the async callback from FileChooser::finished().
         // Do not destroy the chooser or trigger UI/model changes from inside
         // that callback: loading a plugin can rebuild the gallery and destroy
         // this tile, which would otherwise destroy the FileChooser while its
         // own callback is still on the stack.
         juce::Component::SafePointer<AddTile> safeThis(this);
         vst3Chooser_->launchAsync(flags, [safeThis](const juce::FileChooser& chooser) {
           const auto file = chooser.getResult();

           juce::MessageManager::callAsync([safeThis, file] {
             if (safeThis == nullptr) return;

             if (file.exists()) {
               const auto error = safeThis->services().chain.loadExternalVst3(
                   file, safeThis->blockId());
               if (safeThis != nullptr && error.isNotEmpty())
                 safeThis->services().toast.show(error);
             }

             // We're now outside FileChooser::finished(), so destroying the
             // chooser here cannot invalidate its active callback stack.
             if (safeThis != nullptr)
               safeThis->vst3Chooser_.reset();
           });
         });
       }},
  };
  for (auto& item : localLoadItems()) items.push_back(std::move(item));
  return items;
}

void AddTile::paint(juce::Graphics& g) {
  const auto box = getLocalBounds().toFloat();
  const bool armed = dropArmed();
  paint::fill(g, box, gallery::kTileCorner, theme::kSurfaceRaised);
  paint::dashedBorder(g, box, gallery::kTileCorner,
                      armed ? gallery::kFileDropBorder : gallery::kAddTileBorder,
                      gallery::kAddTileBorderWidth);

  if (armed) {
    const float s = gallery::kFileDropGlyphSize;
    Icons::draw(g, Icon::Upload, box.withSizeKeepingCentre(s, s), theme::kGray);
    return;
  }

  const int size = tileSize();
  const int icon = gallery::plusIconSize(size);
  // Routing lines are anchored inside the border (absolute children position
  // against the padding box), so the run to the plus ring is a border-width
  // shorter than measured from the tile's outer edge.
  if (!travelling() && routing_ != Routing::none) {
    const float run = size / 2.0f - icon / 2.0f + gallery::plusCircleInset(icon) -
                      gallery::kAddTileBorderWidth;
    const float y = box.getCentreY() - gallery::kLineWidth / 2;
    g.setColour(theme::kWhite);
    if (routing_ == Routing::left || routing_ == Routing::both)
      g.fillRect(juce::Rectangle<float>(gallery::kAddTileBorderWidth, y, run, gallery::kLineWidth));
    if (routing_ == Routing::right || routing_ == Routing::both)
      g.fillRect(juce::Rectangle<float>(box.getRight() - gallery::kAddTileBorderWidth - run, y, run,
                                        gallery::kLineWidth));
  }
  Icons::draw(g, Icon::PlusCircle, box.withSizeKeepingCentre(icon, icon), theme::kWhite,
              /*strokeWidth=*/1.0f);
}

}  // namespace t3k::ui
