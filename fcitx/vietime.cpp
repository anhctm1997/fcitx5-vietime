#include "vietime.h"
#include <fcitx/inputcontext.h>
#include <fcitx/inputcontextmanager.h>
#include <fcitx/inputpanel.h>
#include <fcitx/instance.h>
#include <fcitx/text.h>
#include <fcitx/userinterface.h>
#include <fcitx-config/iniparser.h>
#include <fcitx-utils/capabilityflags.h>
#include <fcitx-utils/key.h>
#include <fcitx-utils/dbus/matchrule.h>
#include <fcitx-utils/standardpath.h>
#include <algorithm>
#include <string>

FCITX_ADDON_FACTORY(VietimeEngineFactory)

namespace {

void updateTerminalPreedit(fcitx::InputContext *context,
                           const std::string &text) {
    auto &panel = context->inputPanel();
    panel.reset();
    fcitx::Text preedit(text);
    preedit.setCursor(static_cast<int>(text.size()));
    if (context->capabilityFlags().test(fcitx::CapabilityFlag::Preedit))
        panel.setClientPreedit(preedit);
    else
        panel.setPreedit(preedit);
    context->updatePreedit();
    context->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
}

} // namespace

FcitxVietimeEngine::FcitxVietimeEngine(fcitx::Instance *instance)
    : instance_(instance) {
    focusBus_.attachEventLoop(&instance_->eventLoop());
    focusSignal_ = focusBus_.addMatch(
        fcitx::dbus::MatchRule("org.vietime.FocusTracker",
                               "/org/vietime/FocusTracker",
                               "org.vietime.FocusTracker", "FocusChanged"),
        [this](fcitx::dbus::Message &message) {
            updateFocusedApplication(message);
            return true;
        });
    queryFocusedApplication();
    reloadConfig();
}

bool FcitxVietimeEngine::usePreedit(fcitx::InputContext *context) const {
    const auto currentIdentities = identities(context);
    return vietimeUsePreedit(config_, currentIdentities,
                             context->capabilityFlags()) ||
           vietimeMatchedProgram(applicationPrograms_, currentIdentities)
               .has_value();
}

std::vector<std::string>
FcitxVietimeEngine::identities(fcitx::InputContext *context) const {
    auto result = focusedIdentities_;
    if (!context->program().empty())
        result.push_back(context->program());
    return vietimeExpandIdentities(result);
}

void FcitxVietimeEngine::updateFocusedApplication(
    fcitx::dbus::Message &message) {
    using Entry = fcitx::dbus::DictEntry<std::string, std::string>;
    std::vector<Entry> identity;
    message >> identity;
    if (!message)
        return;
    std::vector<std::string> next;
    for (const auto &entry : identity) {
        if (entry.key() == "appId" || entry.key() == "desktopId" ||
            entry.key() == "wmClass" || entry.key() == "executable")
            next.push_back(entry.value());
    }
    next = vietimeNormalizePrograms(next);
    if (next != focusedIdentities_) {
        flushAndRecreateContexts();
        focusedIdentities_ = std::move(next);
    }
}

void FcitxVietimeEngine::queryFocusedApplication() {
    if (!focusBus_.isOpen())
        return;
    auto message = focusBus_.createMethodCall(
        "org.vietime.FocusTracker", "/org/vietime/FocusTracker",
        "org.vietime.FocusTracker", "GetFocusedApplication");
    focusQuery_ = message.callAsync(500000, [this](fcitx::dbus::Message &reply) {
        if (!reply.isError())
            updateFocusedApplication(reply);
        return true;
    });
}

void FcitxVietimeEngine::queryFocusedApplicationSync() {
    if (!focusBus_.isOpen())
        return;
    auto message = focusBus_.createMethodCall(
        "org.vietime.FocusTracker", "/org/vietime/FocusTracker",
        "org.vietime.FocusTracker", "GetFocusedApplication");
    auto reply = message.call(100000);
    if (!reply.isError())
        updateFocusedApplication(reply);
}

void FcitxVietimeEngine::reloadConfig() {
    config_ = VietimeConfig();
    fcitx::readAsIni(config_, fcitx::StandardPath::Type::PkgConfig,
                     "conf/vietime.conf");
    config_.programs.setValue(vietimeNormalizePrograms(*config_.programs));
    applicationPrograms_ = vietimeLoadApplicationPrograms();
    flushAndRecreateContexts();
}

const fcitx::Configuration *FcitxVietimeEngine::getConfig() const {
    return &config_;
}

void FcitxVietimeEngine::setConfig(const fcitx::RawConfig &rawConfig) {
    VietimeConfig next = config_;
    next.load(rawConfig, true);
    next.programs.setValue(vietimeNormalizePrograms(*next.programs));
    flushAndRecreateContexts();
    config_ = std::move(next);
    fcitx::safeSaveAsIni(config_, fcitx::StandardPath::Type::PkgConfig,
                         "conf/vietime.conf");
}

void FcitxVietimeEngine::flushAndRecreateContexts() {
    for (auto &[context, state] : cores_) {
        auto action = vietime_process_key(state.engine.get(), 0, 12, 0);
        if (state.deferred) {
            updateTerminalPreedit(context, "");
        }
        if ((action.action_type == VIETIME_COMMIT ||
             action.action_type == VIETIME_COMMIT_AND_PASS) &&
            action.text_len != 0) {
            context->commitString(std::string(
                reinterpret_cast<char *>(action.text_ptr), action.text_len));
        }
        vietime_action_free(action);
    }
    cores_.clear();
}

FcitxVietimeEngine::Core &FcitxVietimeEngine::core(fcitx::InputContext *context) {
    auto [it, inserted] = cores_.try_emplace(context);
    if (inserted) {
        // Terminal frontends may advertise SurroundingText even when deletion
        // is not wired to the shell/readline buffer. Some GTK/Wayland terminal
        // contexts also omit the Terminal capability, so use the application
        // id as a fallback. Always defer their words to avoid replacement
        // flicker.
        it->second.deferred = usePreedit(context);
        it->second.engine.reset(it->second.deferred
                                    ? vietime_engine_new_deferred()
                                    : vietime_engine_new());
        writeStatus(context, it->second.deferred);
    }
    return it->second.engine;
}

void FcitxVietimeEngine::writeStatus(fcitx::InputContext *context,
                                     bool deferred) const {
    fcitx::RawConfig status;
    const auto currentIdentities = identities(context);
    for (size_t i = 0; i < currentIdentities.size(); ++i)
        status.setValueByPath("Identities/" + std::to_string(i),
                              currentIdentities[i]);
    auto matched = vietimeMatchedProgram(applicationPrograms_, currentIdentities);
    if (!matched)
        matched = vietimeMatchedProgram(*config_.programs, currentIdentities);
    status.setValueByPath("MatchedRule", matched.value_or(""));
    status.setValueByPath("Mode", deferred ? "preedit" : "direct");
    status.setValueByPath("Program", context->program());
    fcitx::safeSaveAsIni(status, fcitx::StandardPath::Type::Cache,
                         "vietime/status.conf");
}

void FcitxVietimeEngine::reset(const fcitx::InputMethodEntry &, fcitx::InputContextEvent &event) {
    auto it = cores_.find(event.inputContext());
    if (it != cores_.end()) {
        vietime_engine_reset(it->second.engine.get());
        if (it->second.deferred) {
            updateTerminalPreedit(event.inputContext(), "");
        }
    }
}

void FcitxVietimeEngine::deactivate(const fcitx::InputMethodEntry &,
                                    fcitx::InputContextEvent &event) {
    auto it = cores_.find(event.inputContext());
    if (it != cores_.end()) {
        auto action = vietime_process_key(it->second.engine.get(), 0, 12, 0);
        if (it->second.deferred) {
            updateTerminalPreedit(event.inputContext(), "");
        }
        if (action.action_type == VIETIME_COMMIT_AND_PASS &&
            action.text_len != 0) {
            std::string text(reinterpret_cast<char *>(action.text_ptr),
                             action.text_len);
            event.inputContext()->commitString(text);
        }
        vietime_action_free(action);
        vietime_engine_reset(it->second.engine.get());
    }
}

void FcitxVietimeEngine::keyEvent(const fcitx::InputMethodEntry &, fcitx::KeyEvent &event) {
    if (event.isRelease()) return;
    // GNOME's IBus frontend reuses program=gnome-shell across applications.
    // Resolve the active window before the first key is allowed to create a
    // direct-mode core; the asynchronous FocusChanged signal can arrive one
    // event too late during a fast application switch.
    if (event.inputContext()->program() == "gnome-shell")
        queryFocusedApplicationSync();
    // UniKey processes the raw ASCII keys so Telex behavior does not depend on
    // transformations applied to KeyEvent::key() by another addon.
    const auto key = event.rawKey();
    uint32_t mods = 0;
    if (key.states() & fcitx::KeyState::Ctrl) mods |= 1;
    if (key.states() & fcitx::KeyState::Alt) mods |= 2;
    if (key.states() & fcitx::KeyState::Super) mods |= 4;
    if (key.states() & fcitx::KeyState::Shift) mods |= 8;
    uint32_t special = 0;
    switch (key.sym()) {
    case FcitxKey_BackSpace: special=1; break; case FcitxKey_Delete: special=2; break;
    case FcitxKey_Return: special=3; break; case FcitxKey_Tab: special=4; break;
    case FcitxKey_Escape: special=5; break; case FcitxKey_Left: special=6; break;
    case FcitxKey_Right: special=7; break; case FcitxKey_Up: special=8; break;
    case FcitxKey_Down: special=9; break; case FcitxKey_Home: special=10; break;
    case FcitxKey_End: special=11; break; default: break;
    }
    const auto utf8 = key.toString();
    uint32_t unicode = special == 0 && utf8.size() == 1 ? static_cast<unsigned char>(utf8[0]) : 0;
    auto action = vietime_process_key(core(event.inputContext()).get(), unicode, special, mods);
    std::string text;
    if (action.text_len != 0)
        text.assign(reinterpret_cast<char *>(action.text_ptr), action.text_len);

    if (action.action_type == VIETIME_COMMIT ||
        action.action_type == VIETIME_COMMIT_AND_PASS) {
        if (usePreedit(event.inputContext())) {
            updateTerminalPreedit(event.inputContext(), "");
        }
        if (!text.empty()) event.inputContext()->commitString(text);
        if (action.action_type == VIETIME_COMMIT) {
            event.filterAndAccept();
        }
    } else if (action.action_type == VIETIME_REPLACE ||
               action.action_type == VIETIME_REPLACE_AND_PASS) {
        const bool canReplace = event.inputContext()
                                    ->capabilityFlags()
                                    .test(fcitx::CapabilityFlag::SurroundingText) &&
                                event.inputContext()->surroundingText().isValid();
        if (canReplace) {
            event.inputContext()->deleteSurroundingText(
                -static_cast<int>(action.delete_before),
                action.delete_before);
            if (!text.empty()) event.inputContext()->commitString(text);
            if (action.action_type == VIETIME_REPLACE) {
                event.filterAndAccept();
            }
        } else {
            // Terminals commonly lack usable surrounding text. Keep direct
            // commit/no-preedit behavior by deleting the committed suffix with
            // real Backspace events before committing its Telex replacement.
            for (uint32_t i = 0; i < action.delete_before; ++i) {
                event.inputContext()->forwardKey(
                    fcitx::Key(FcitxKey_BackSpace));
            }
            if (!text.empty()) event.inputContext()->commitString(text);
            if (action.action_type == VIETIME_REPLACE) {
                event.filterAndAccept();
            }
        }
    } else if (action.action_type == VIETIME_CONSUME) {
        if (usePreedit(event.inputContext())) {
            updateTerminalPreedit(event.inputContext(), text);
        }
        event.filterAndAccept();
    }
    vietime_action_free(action);
}
