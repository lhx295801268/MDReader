#include "ui/ThemeMenu.h"

ThemeMenu::ThemeMenu(QWidget* parent) : QMenu(parent) {
    setTitle("Theme");
    themes_ = {"github", "github-dark", "dracula",
               "solarized-light", "solarized-dark", "one-dark"};
    for (const auto& t : themes_) {
        auto* a = addAction(t);
        a->setCheckable(true);
        connect(a, &QAction::triggered, this, [this, t] { emit themeSelected(t); });
    }
}

void ThemeMenu::setCurrent(const QString& theme) {
    for (auto* a : actions()) a->setChecked(a->text() == theme);
}