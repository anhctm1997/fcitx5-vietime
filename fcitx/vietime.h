#pragma once
#include "vietime_ffi.h"
#include "vietime_config.h"
#include <fcitx/addonfactory.h>
#include <fcitx/addonmanager.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx-utils/dbus/bus.h>
#include <memory>
#include <unordered_map>

class FcitxVietimeEngine final : public fcitx::InputMethodEngineV2 {
public:
    explicit FcitxVietimeEngine(fcitx::Instance *instance);
    void keyEvent(const fcitx::InputMethodEntry &, fcitx::KeyEvent &event) override;
    void reset(const fcitx::InputMethodEntry &, fcitx::InputContextEvent &event) override;
    void deactivate(const fcitx::InputMethodEntry &, fcitx::InputContextEvent &event) override;
    void reloadConfig() override;
    const fcitx::Configuration *getConfig() const override;
    void setConfig(const fcitx::RawConfig &config) override;
private:
    using Core = std::unique_ptr<VietimeEngineHandle, decltype(&vietime_engine_free)>;
    struct ContextState {
        Core engine{nullptr, &vietime_engine_free};
        bool deferred = false;
    };
    Core &core(fcitx::InputContext *context);
    bool usePreedit(fcitx::InputContext *context) const;
    void flushAndRecreateContexts();
    void updateFocusedApplication(fcitx::dbus::Message &message);
    void queryFocusedApplication();
    void queryFocusedApplicationSync();
    std::vector<std::string> identities(fcitx::InputContext *context) const;
    void writeStatus(fcitx::InputContext *context, bool deferred) const;
    std::unordered_map<fcitx::InputContext *, ContextState> cores_;
    VietimeConfig config_;
    fcitx::Instance *instance_;
    fcitx::dbus::Bus focusBus_{fcitx::dbus::BusType::Session};
    std::unique_ptr<fcitx::dbus::Slot> focusSignal_;
    std::unique_ptr<fcitx::dbus::Slot> focusQuery_;
    std::vector<std::string> focusedIdentities_;
    std::vector<std::string> applicationPrograms_;
};

class VietimeEngineFactory final : public fcitx::AddonFactory {
    fcitx::AddonInstance *create(fcitx::AddonManager *manager) override {
        return new FcitxVietimeEngine(manager->instance());
    }
};
