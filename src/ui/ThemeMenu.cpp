#include "ui/ThemeMenu.h"
#include <QAction>
#include <QActionGroup>

ThemeMenu::ThemeMenu(QWidget* parent) : QMenu(parent) {
    setTitle("Theme");
    auto* grp = new QActionGroup(this);
    grp->setExclusive(true);

    // "Follow System" entry — sentinel value "auto" is emitted to the host,
    // which resolves it via QStyleHints::colorScheme() into a concrete theme.
    auto* autoAct = addAction(tr("Follow System"));
    autoAct->setCheckable(true);
    grp->addAction(autoAct);
    connect(autoAct, &QAction::triggered, this, [this] { emit themeSelected(kAuto); });

    addSeparator();

    themes_ = {"github", "github-dark", "dracula",
               "solarized-light", "solarized-dark", "one-dark"};
    for (const auto& t : themes_) {
        auto* a = addAction(t);
        a->setCheckable(true);
        grp->addAction(a);   // also makes it auto-exclusive
        connect(a, &QAction::triggered, this, [this, t] { emit themeSelected(t); });
    }
}

void ThemeMenu::setCurrent(const QString& theme) {
    for (auto* a : actions()) {
        // The "Follow System" entry has visible text "Follow System" but the
        // host passes the sentinel "auto" — match on that explicitly.
        const bool match = (theme == kAuto)
            ? (a->text() == tr("Follow System"))
            : (a->text() == theme);
        a->setChecked(match);
    }
}