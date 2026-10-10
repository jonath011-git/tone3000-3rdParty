#include "PluginHeader.h"

#include "core/Brand.h"
#include "core/CustomIcons.h"
#include "core/Design.h"
#include "core/Help.h"
#include "core/Paint.h"
#include "core/Theme.h"
#include "widgets/Clickable.h"
#include "services/Vst3Library.h"

namespace t3k::ui {

namespace {
constexpr int kPadX = 24;
constexpr int kGroupGap = 40;  // between header items
constexpr int kPairGap = 16;   // tight pairs (undo/redo)
constexpr int kLogoWidth = 160;
constexpr int kLogoHeight = 24;  // 160 * 32 / 210, rounded like the browser
}  // namespace

// The wordmark links to tone3000.com.
class PluginHeader::LogoLink : public Clickable {
public:
  LogoLink() : Clickable({}) {
    setTitle("TONE3000");  // tone3000.com, to a screen reader
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    onClick = [] { juce::URL("https://www.tone3000.com").launchInDefaultBrowser(); };
  }
  void paintButton(juce::Graphics& g, bool, bool) override {
    Brand::drawLogo(g, getLocalBounds().toFloat());
  }
};

PluginHeader::PluginHeader(Services& services)
    : services_(services),
      logo_(std::make_unique<LogoLink>()),
      presetBar_(services),
      tuner_(custom_icons::kTuningFork, 28, 18) {
  addAndMakeVisible(*logo_);
  addAndMakeVisible(presetBar_);

  vst3LibraryButton_.setTooltip("Bibliothèque de plug-ins VST3");
  vst3LibraryButton_.onClick = [this] {
    juce::PopupMenu menu;
    menu.addItem(1, "Ajouter un dossier...");
    menu.addItem(2, "Analyser / actualiser la bibliothèque");
    menu.addSeparator();

    const auto folders = vst3library::folders(services_.prefs);
    if (folders.empty()) {
      menu.addItem(3, "Aucun dossier configuré", false);
    } else {
      menu.addSectionHeader("Dossiers de la bibliothèque");
      for (size_t i = 0; i < folders.size(); ++i)
        menu.addItem(static_cast<int>(100 + i),
                     "Retirer : " + folders[i].getFullPathName());
    }
    menu.addSeparator();
    const auto count = vst3library::plugins(services_.prefs).size();
    menu.addItem(4, juce::String(static_cast<int>(count)) + " plug-in(s) indexé(s)", false);

    juce::Component::SafePointer<PluginHeader> safeThis(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&vst3LibraryButton_),
                       [safeThis, folders](int result) {
      if (safeThis == nullptr) return;
      if (result == 1) {
        juce::File initial;
        if (!folders.empty()) initial = folders.back();
        safeThis->vst3FolderChooser_ = std::make_unique<juce::FileChooser>(
            "Choisir un dossier contenant des plug-ins VST3", initial, "", true);
        safeThis->vst3FolderChooser_->launchAsync(
            juce::FileBrowserComponent::openMode |
                juce::FileBrowserComponent::canSelectDirectories,
            [safeThis](const juce::FileChooser& chooser) {
          const auto selected = chooser.getResult();
          juce::MessageManager::callAsync([safeThis, selected] {
            if (safeThis == nullptr) return;
            if (selected.isDirectory()) {
              vst3library::addFolder(safeThis->services_.prefs, selected);
              const auto found = vst3library::rescan(safeThis->services_.prefs);
              safeThis->services_.toast.show("Bibliothèque VST3 actualisée : " +
                                               juce::String(static_cast<int>(found.size())) +
                                               " plug-in(s).");
            }
            safeThis->vst3FolderChooser_.reset();
          });
        });
      } else if (result == 2) {
        const auto found = vst3library::rescan(safeThis->services_.prefs);
        safeThis->services_.toast.show("Bibliothèque VST3 actualisée : " +
                                         juce::String(static_cast<int>(found.size())) +
                                         " plug-in(s).");
      } else if (result >= 100 && static_cast<size_t>(result - 100) < folders.size()) {
        vst3library::removeFolder(safeThis->services_.prefs,
                                  static_cast<size_t>(result - 100));
        const auto found = vst3library::rescan(safeThis->services_.prefs);
        safeThis->services_.toast.show("Dossier retiré. " +
                                         juce::String(static_cast<int>(found.size())) +
                                         " plug-in(s) dans la bibliothèque.");
      }
    });
  };
  addAndMakeVisible(vst3LibraryButton_);

  stereo_.onToggle = [this](bool stereo) {
    if (onStereoToggle) onStereoToggle(stereo);
  };
  addAndMakeVisible(stereo_);

  tuner_.setHelpText(help::text(help::Key::tuner));
  tuner_.setFillWhenActive(true);
  tuner_.onClick = [this] {
    if (onToggleTuner) onToggleTuner(!tunerShown_);
  };
  addAndMakeVisible(tuner_);

  undo_.setHelpText(help::text(help::Key::undo));
  undo_.onClick = [this] {
    if (onUndo) onUndo();
  };
  redo_.setHelpText(help::text(help::Key::redo));
  redo_.onClick = [this] {
    if (onRedo) onRedo();
  };
  addAndMakeVisible(undo_);
  addAndMakeVisible(redo_);

  account_.onOpenSettings = [this] {
    if (onOpenSettings) onOpenSettings();
  };
  account_.onLogin = [this] {
    if (onLogin) onLogin();
  };
  account_.onLogout = [this] {
    if (onLogout) onLogout();
  };
  addAndMakeVisible(account_);

  services_.chain.addListener(this);
  services_.session.addListener(this);
  sessionChanged();
  chainChanged(services_.chain.state());
  setTunerShown(false);
  setSize(design::kWidth, kHeight);
}

PluginHeader::~PluginHeader() {
  services_.session.removeListener(this);
  services_.chain.removeListener(this);
}

void PluginHeader::sessionChanged() {
  const auto& session = services_.session;
  account_.setAuthenticated(session.authenticated());
  const auto user = session.user();
  const auto url = user ? user->avatarUrl : juce::String();
  if (url == avatarUrl_ && (url.isNotEmpty() || !session.authenticated())) return;
  avatarUrl_ = url;
  account_.setAvatar(services_.images, url);
}

void PluginHeader::setTunerShown(bool shown) {
  tunerShown_ = shown;
  // Lit + HIGHLIGHT fill while the tuner is up; the icon itself stays white.
  tuner_.setActive(true);
  tuner_.setFillWhenActive(shown);
}

void PluginHeader::chainChanged(const ChainState& state) {
  undo_.setEnabled(state.canUndo);
  redo_.setEnabled(state.canRedo);
  stereo_.setStereoEnabled(state.stereoEnabled);
}

void PluginHeader::paint(juce::Graphics& g) {
  g.fillAll(theme::kBlack);
  paint::hairlineH(g, 0, static_cast<float>(getWidth()), static_cast<float>(getHeight() - 1),
                   theme::kBorder);
}

void PluginHeader::resized() {
  // The 1px bottom border is inside the 64px box; items centre in the 63px
  // content row like the flex container did.
  auto area = getLocalBounds().withTrimmedBottom(1).reduced(kPadX, 0);
  // 31.5 in the browser; rounding up puts even-height boxes where the
  // browser's anti-aliased edges land.
  const int cy = (area.getHeight() + 1) / 2;
  auto place = [&](juce::Component& c, int right) {
    c.setBounds(right - c.getWidth(), cy - c.getHeight() / 2, c.getWidth(), c.getHeight());
    return c.getX();
  };

  logo_->setBounds(area.getX(), cy - kLogoHeight / 2, kLogoWidth, kLogoHeight);
  vst3LibraryButton_.setBounds(area.getX() + kLogoWidth + 20, cy - 15, 58, 30);

  // Right group, laid out from the right edge: account · undo/redo · tuner ·
  // stereo · presets, 40px apart (16px inside the undo/redo pair).
  int x = place(account_, area.getRight()) - kGroupGap;
  x = place(redo_, x) - kPairGap;
  x = place(undo_, x) - kGroupGap;
  x = place(tuner_, x) - kGroupGap;
  x = place(stereo_, x) - kGroupGap;
  place(presetBar_, x);
}

}  // namespace t3k::ui
