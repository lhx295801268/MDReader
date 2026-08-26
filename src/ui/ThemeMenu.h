#pragma once
#include <QMenu>
#include <QStringList>

class ThemeMenu : public QMenu {
    Q_OBJECT
public:
    explicit ThemeMenu(QWidget* parent = nullptr);
    void setCurrent(const QString& theme);
    QStringList available() const { return themes_; }
signals:
    void themeSelected(const QString& theme);
private:
    QStringList themes_;
};