#pragma once
#include <QMenu>
#include <QStringList>

/// Dropdown menu of available Markdown preview themes. Selecting an action
/// emits `themeSelected(internalName)`; the internal name is the basename of
/// the matching CSS file under `:/themes/` (e.g. "github", "github-dark"),
/// or the sentinel `kAuto` ("auto") meaning "follow the OS color scheme".
/// All actions are members of an exclusive QActionGroup so only one shows
/// the check indicator at a time.
class ThemeMenu : public QMenu {
    Q_OBJECT
public:
    /// Sentinel emitted by the "Follow System" entry. The host resolves it
    /// against QStyleHints::colorScheme() to pick a concrete CSS file.
    static constexpr auto kAuto = "auto";

    explicit ThemeMenu(QWidget* parent = nullptr);
    void setCurrent(const QString& theme);

    /// Concrete-theme basenames the menu can emit (excludes the "auto"
    /// sentinel). Order matches the order actions appear in the menu.
    QStringList available() const { return themes_; }
signals:
    void themeSelected(const QString& theme);
private:
    QStringList themes_;
};