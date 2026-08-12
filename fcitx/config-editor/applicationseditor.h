#pragma once
#include <fcitxqtconfiguiwidget.h>

class QListWidget;
class QPushButton;
class QLabel;

class VietimeApplicationsEditor final : public fcitx::FcitxQtConfigUIWidget {
    Q_OBJECT
public:
    explicit VietimeApplicationsEditor(QWidget *parent = nullptr);
    void load() override;
    void save() override;
    QString title() override;
    QString icon() override;

private:
    void addApplication();
    bool saveImmediately();
    void refreshStatus();
    QString configPath() const;
    QString statusPath() const;
    QListWidget *rules_;
    QPushButton *remove_;
    QLabel *status_;
};
