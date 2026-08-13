#include "vietime_config.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <unordered_set>
#include <sys/stat.h>
#include <unistd.h>
#include <fcitx-config/iniparser.h>
#include <fcitx-utils/standardpath.h>

namespace {

std::string normalize(std::string_view value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) {
        return {};
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    std::string result(value.substr(first, last - first + 1));
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char ch) { return std::tolower(ch); });
    return result;
}

bool wildcardMatch(std::string_view pattern, std::string_view value) {
    size_t p = 0, v = 0, star = std::string_view::npos, retry = 0;
    while (v < value.size()) {
        if (p < pattern.size() &&
            (pattern[p] == '?' || pattern[p] == value[v])) {
            ++p;
            ++v;
        } else if (p < pattern.size() && pattern[p] == '*') {
            star = p++;
            retry = v;
        } else if (star != std::string_view::npos) {
            p = star + 1;
            v = ++retry;
        } else {
            return false;
        }
    }
    while (p < pattern.size() && pattern[p] == '*') {
        ++p;
    }
    return p == pattern.size();
}

std::string readFile(const std::filesystem::path &path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>()};
}

bool isNumeric(std::string_view value) {
    return !value.empty() &&
           std::all_of(value.begin(), value.end(),
                       [](unsigned char ch) { return std::isdigit(ch); });
}

std::vector<std::string> splitAliases(std::string_view value) {
    std::vector<std::string> result;
    size_t start = 0;
    while (start <= value.size()) {
        const auto end = value.find('|', start);
        result.emplace_back(value.substr(start, end - start));
        if (end == std::string_view::npos)
            break;
        start = end + 1;
    }
    return result;
}

} // namespace

std::vector<std::string> vietimeDefaultPrograms() {
    return {"alacritty",          "com.mitchellh.ghostty", "foot",
            "gnome-terminal",     "gnome-terminal-server", "ghostty",
            "kitty",              "konsole",                "lxterminal",
            "mate-terminal",      "org.gnome.console",      "org.gnome.ptyxis",
            "ptyxis",             "qterminal",              "st",
            "terminator",         "tilix",                  "urxvt",
            "wezterm",            "xfce4-terminal",         "xterm",
            "x-terminal-emulator"};
}

std::vector<std::string> vietimeNormalizePrograms(
    const std::vector<std::string> &programs) {
    std::vector<std::string> result;
    std::unordered_set<std::string> seen;
    for (const auto &program : programs) {
        auto normalized = normalize(program);
        if (!normalized.empty() && seen.insert(normalized).second) {
            result.push_back(std::move(normalized));
        }
    }
    return result;
}

bool vietimeProgramMatches(std::string_view pattern, std::string_view program) {
    return wildcardMatch(normalize(pattern), normalize(program));
}

std::vector<std::string> vietimeExpandIdentities(
    const std::vector<std::string> &identities) {
    std::vector<std::string> expanded = identities;
    for (const auto &identity : identities) {
        const std::filesystem::path path(identity);
        if (path.has_filename())
            expanded.push_back(path.filename().string());
        auto normalized = normalize(identity);
        constexpr std::string_view suffix = ".desktop";
        if (normalized.size() > suffix.size() &&
            normalized.ends_with(suffix))
            expanded.push_back(normalized.substr(0, normalized.size() - suffix.size()));
    }
    return vietimeNormalizePrograms(expanded);
}

std::vector<std::string> vietimeLoadApplicationPrograms() {
    fcitx::RawConfig raw;
    fcitx::readAsIni(raw, fcitx::StandardPathsType::PkgConfig,
                     "conf/vietime-applications.conf");
    std::vector<std::string> rulesValues;
    const auto rules = raw.get("Rules");
    if (rules) {
        for (const auto &key : rules->subItems()) {
            const auto item = rules->get(key);
            if (!item)
                continue;
            rulesValues.push_back(item->value());
        }
    }
    return vietimeExpandApplicationRules(rulesValues);
}

std::vector<std::string> vietimeExpandApplicationRules(
    const std::vector<std::string> &rules) {
    std::vector<std::string> programs;
    for (const auto &rule : rules) {
        auto aliases = splitAliases(rule);
        programs.insert(programs.end(), aliases.begin(), aliases.end());
    }
    return vietimeExpandIdentities(programs);
}

std::optional<std::string> vietimeMatchedProgram(
    const std::vector<std::string> &patterns,
    const std::vector<std::string> &identities) {
    for (const auto &pattern : patterns) {
        for (const auto &identity : identities) {
            if (vietimeProgramMatches(pattern, identity))
                return normalize(pattern);
        }
    }
    return std::nullopt;
}

std::vector<std::string>
vietimeRunningGuiPrograms(const std::filesystem::path &procRoot) {
    std::vector<std::string> programs;
    std::error_code error;
    for (const auto &entry : std::filesystem::directory_iterator(procRoot, error)) {
        const auto pid = entry.path().filename().string();
        if (!isNumeric(pid)) {
            continue;
        }
        struct stat processStat {};
        if (::stat(entry.path().c_str(), &processStat) != 0 ||
            processStat.st_uid != ::geteuid()) {
            continue;
        }
        const auto environment = readFile(entry.path() / "environ");
        if (environment.find("DISPLAY=") == std::string::npos &&
            environment.find("WAYLAND_DISPLAY=") == std::string::npos) {
            continue;
        }
        const auto commandLine = readFile(entry.path() / "cmdline");
        if (commandLine.find("--type=") != std::string::npos) {
            continue;
        }
        auto program = normalize(readFile(entry.path() / "comm"));
        if (!program.empty()) {
            programs.push_back(std::move(program));
        }
    }
    auto normalized = vietimeNormalizePrograms(programs);
    std::sort(normalized.begin(), normalized.end());
    return normalized;
}

bool vietimeUsePreedit(const VietimeConfig &config, std::string_view program,
                       fcitx::CapabilityFlags capabilities) {
    return vietimeUsePreedit(config, std::vector<std::string>{std::string(program)},
                             capabilities);
}

bool vietimeUsePreedit(const VietimeConfig &config,
                       const std::vector<std::string> &identities,
                       fcitx::CapabilityFlags capabilities) {
    const auto has = [&](bool enabled, fcitx::CapabilityFlag flag) {
        return enabled && capabilities.test(flag);
    };
    if (has(*config.terminal, fcitx::CapabilityFlag::Terminal) ||
        has(*config.preedit, fcitx::CapabilityFlag::Preedit) ||
        has(*config.surroundingText, fcitx::CapabilityFlag::SurroundingText) ||
        has(*config.multiline, fcitx::CapabilityFlag::Multiline) ||
        has(*config.clientSideInputPanel,
            fcitx::CapabilityFlag::ClientSideInputPanel)) {
        return true;
    }
    return vietimeMatchedProgram(*config.programs,
                                 vietimeExpandIdentities(identities)).has_value();
}
