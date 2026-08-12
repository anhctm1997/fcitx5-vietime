#include "main.h"
#include "applicationseditor.h"

fcitx::FcitxQtConfigUIWidget *
VietimeApplicationsPlugin::create(const QString &key) {
    Q_UNUSED(key);
    return new VietimeApplicationsEditor;
}
