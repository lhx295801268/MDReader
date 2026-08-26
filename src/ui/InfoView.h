#pragma once
#include <QLabel>
#include <QFormLayout>

class InfoView : public QWidget {
    Q_OBJECT
public:
    explicit InfoView(QWidget* parent = nullptr);
    void setStats(int words, int chars, int paragraphs, int headings);
private:
    QLabel* words_; QLabel* chars_;
    QLabel* paras_; QLabel* heads_;
};
