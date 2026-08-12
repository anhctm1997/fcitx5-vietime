#pragma once

#include <fcitx-config/configuration.h>
#include <fcitx-utils/capabilityflags.h>
#include <string>
#include <string_view>
#include <vector>
#include <filesystem>
#include <optional>

std::vector<std::string> vietimeDefaultPrograms();
std::vector<std::string> vietimeNormalizePrograms(
    const std::vector<std::string> &programs);
bool vietimeProgramMatches(std::string_view pattern, std::string_view program);
std::vector<std::string> vietimeExpandIdentities(
    const std::vector<std::string> &identities);
std::vector<std::string> vietimeExpandApplicationRules(
    const std::vector<std::string> &rules);
std::vector<std::string> vietimeLoadApplicationPrograms();
std::optional<std::string> vietimeMatchedProgram(
    const std::vector<std::string> &patterns,
    const std::vector<std::string> &identities);
std::vector<std::string> vietimeRunningGuiPrograms(
    const std::filesystem::path &procRoot = "/proc");

FCITX_CONFIGURATION(
    VietimeConfig,
    fcitx::HiddenOption<std::vector<std::string>> programs{
        this, "Programs",
        "Applications using anti-flicker preedit (supports * and ?)",
        vietimeDefaultPrograms()};
    fcitx::ExternalOption applicationEditor{
        this, "ApplicationEditor", "Applications using anti-flicker preedit",
        "fcitx://config/addon/vietime/applications"};
    fcitx::Option<bool> terminal{this, "MatchTerminal", "Terminal capability",
                                 true};
    fcitx::Option<bool> preedit{this, "MatchPreedit", "Preedit capability (may match many applications)",
                                false};
    fcitx::Option<bool> surroundingText{
        this, "MatchSurroundingText", "SurroundingText capability (broad match)", false};
    fcitx::Option<bool> multiline{this, "MatchMultiline", "Multiline capability (broad match)",
                                  false};
    fcitx::Option<bool> clientSideInputPanel{
        this, "MatchClientSideInputPanel", "ClientSideInputPanel capability (broad match)",
        false};);

bool vietimeUsePreedit(const VietimeConfig &config, std::string_view program,
                       fcitx::CapabilityFlags capabilities);
bool vietimeUsePreedit(const VietimeConfig &config,
                       const std::vector<std::string> &identities,
                       fcitx::CapabilityFlags capabilities);
