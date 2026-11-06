#include "ui/ThemeMenu.h"
#include <QAction>
#include <QActionGroup>

ThemeMenu::ThemeMenu(QWidget* parent) : QMenu(parent) {
    setTitle(tr("Theme"));
    auto* grp = new QActionGroup(this);
    grp->setExclusive(true);

    // "Follow System" entry — sentinel value "auto" is emitted to the host,
    // which resolves it via QStyleHints::colorScheme() into a concrete theme.
    auto* autoAct = addAction(tr("Follow System"));
    autoAct->setCheckable(true);
    autoAct->setData(QStringLiteral("auto"));  // Phase 10: source marker
    grp->addAction(autoAct);
    connect(autoAct, &QAction::triggered, this, [this] { emit themeSelected(kAuto); });

    addSeparator();

    themes_ = entries();
    themes_.removeFirst();  // drop the "auto" sentinel — it is above
    for (const auto& t : themes_) {
        auto* a = addAction(tr(t.toUtf8().constData()));
        a->setCheckable(true);
        a->setData(t);  // Phase 10: stash internal basename for retranslate
        grp->addAction(a);   // also makes it auto-exclusive
        connect(a, &QAction::triggered, this, [this, t] { emit themeSelected(t); });
    }
}

QStringList ThemeMenu::entries() {
    // Phase 14 (macOS): one source of truth for the selectable entries.
    // The first entry is the "auto" sentinel ("Follow System"); the rest
    // are concrete theme basenames.
    return {"auto", "github", "github-dark", "dracula",
            "solarized-light", "solarized-dark", "one-dark"};
}

void ThemeMenu::retranslate() {
    // Phase 14 (macOS): QAction::setText() is not refreshed automatically
    // when the translator is swapped, so re-run tr() for the title and
    // every entry. The internal id stays in data() — that is what
    // setCurrent() and the host compare against.
    setTitle(tr("Theme"));
    for (auto* a : actions()) {
        if (a->isSeparator()) continue;
        const QString id = a->data().toString();
        if (id == QLatin1String(kAuto)) {
            a->setText(tr("Follow System"));
        } else if (!id.isEmpty()) {
            a->setText(tr(id.toUtf8().constData()));
        }
    }
}

void ThemeMenu::setCurrent(const QString& theme) {
    for (auto* a : actions()) {
        // Phase 10: match by data() (the source English basename or the
        // "auto" sentinel) instead of the visible text. After a language
        // flip the action's text() will be Chinese, so a text() compare
        // would silently miss — but data() stays in the original
        // English/identifier space and is therefore stable.
        const bool match = (a->data().toString() == theme);
        a->setChecked(match);
    }
}