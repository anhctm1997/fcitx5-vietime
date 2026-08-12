#include "vietime_config.h"
#include <algorithm>
#include <cassert>

int main() {
    assert(vietimeProgramMatches("org.*.Terminal", "ORG.GNOME.TERMINAL"));
    assert(vietimeProgramMatches("?term", "xterm"));
    assert(!vietimeProgramMatches("xterm", "xterminal"));
    const auto normalized =
        vietimeNormalizePrograms({" Kitty ", "kitty", "", "XTERM"});
    assert((normalized == std::vector<std::string>{"kitty", "xterm"}));
    const auto goland = vietimeExpandApplicationRules(
        {"goland_goland.desktop|jetbrains-goland|/snap/bin/goland"});
    assert(std::find(goland.begin(), goland.end(), "goland_goland") !=
           goland.end());
    assert(std::find(goland.begin(), goland.end(), "goland") != goland.end());
    assert(vietimeMatchedProgram({"goland"}, goland) == "goland");

    VietimeConfig config;
    assert(vietimeUsePreedit(config, "kitty", fcitx::CapabilityFlags()));
    assert(vietimeUsePreedit(config, "unknown",
                             fcitx::CapabilityFlag::Terminal));
    assert(!vietimeUsePreedit(config, "unknown", fcitx::CapabilityFlags()));

    config.terminal.setValue(false);
    config.multiline.setValue(true);
    assert(vietimeUsePreedit(config, "unknown",
                             fcitx::CapabilityFlag::Multiline));
    assert(!vietimeUsePreedit(config, "unknown",
                              fcitx::CapabilityFlag::Terminal));

    config.programs.setValue(std::vector<std::string>{" custom-* "});
    fcitx::RawConfig raw;
    config.save(raw);
    VietimeConfig restored;
    restored.load(raw);
    assert(*restored.programs == std::vector<std::string>{" custom-* "});
    assert(!*restored.terminal);
    assert(*restored.multiline);

    restored.multiline.setValue(false);
    restored.programs.setValue(std::vector<std::string>{"com.jetbrains.goland"});
    assert(vietimeUsePreedit(
        restored, std::vector<std::string>{"gnome-shell", "com.jetbrains.goland"},
        fcitx::CapabilityFlags()));
    assert(!vietimeUsePreedit(
        restored, std::vector<std::string>{"gnome-shell", "code"},
        fcitx::CapabilityFlags()));
    assert(vietimeRunningGuiPrograms("/path/that/does/not/exist").empty());
    return 0;
}
