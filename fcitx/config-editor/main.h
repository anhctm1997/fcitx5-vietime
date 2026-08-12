#pragma once
#include <fcitxqtconfiguiplugin.h>

class VietimeApplicationsPlugin final : public fcitx::FcitxQtConfigUIPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID FcitxQtConfigUIFactoryInterface_iid FILE
                      "applications-editor.json")
public:
    explicit VietimeApplicationsPlugin(QObject *parent = nullptr)
        : FcitxQtConfigUIPlugin(parent) {}
    fcitx::FcitxQtConfigUIWidget *create(const QString &key) override;
};
