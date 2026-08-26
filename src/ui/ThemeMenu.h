#pragma once
#include <QMenu>
#include <QStringList>

/// Dropdown menu of available Markdown preview themes. Selecting an action
/// emits `themeSelected(internalName)`; the internal name is the basename of
/// the matching CSS file under `:/themes/` (e.g. "github", "github-dark").
/// All actions are members of an exclusive QActionGroup so only one shows
/// the check indicator at a time.
class ThemeMenu : public QMenu {
    Q_OBJECT
public:
    explicit ThemeMenu(QWidget* parent = nullptr);
    void setCurrent(const QString& theme);
signals:
    void themeSelected(const QString& theme);
private:
    QStringList themes_;
};