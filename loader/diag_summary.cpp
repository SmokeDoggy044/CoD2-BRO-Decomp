#include "diag_summary.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <string>
#include <vector>

namespace loader {

namespace fs = std::filesystem;

namespace {

// One log line: "[    12.345] warn  render   text" (platform/log.cpp).
struct LogLine {
    double seconds = 0;
    std::string level, category, text;
};

bool parse_line(const std::string& line, LogLine& out) {
    if (line.size() < 3 || line[0] != '[') return false;
    const size_t close = line.find(']');
    if (close == std::string::npos) return false;
    out.seconds = std::atof(line.c_str() + 1);
    size_t i = close + 1;
    auto word = [&](std::string& to) {
        while (i < line.size() && line[i] == ' ') ++i;
        const size_t begin = i;
        while (i < line.size() && line[i] != ' ') ++i;
        to = line.substr(begin, i - begin);
    };
    word(out.level);
    word(out.category);
    while (i < line.size() && line[i] == ' ') ++i;
    out.text = line.substr(i);
    return !out.level.empty();
}

struct Problem {
    double first = 0;
    uint32_t count = 0;
    std::string picture;  // an automatic mark's screenshot, if it was marked
};

bool starts_with(const std::string& text, const char* prefix) { return text.rfind(prefix, 0) == 0; }

}  // namespace

void write_diag_summary(const fs::path& dir) {
    std::ifstream log(dir / "log.txt");
    if (!log) return;

    // Read the session: everything after the last launch that was not a reboot.
    std::vector<LogLine> lines;
    bool rebooting = false;
    uint32_t reboots = 0;
    for (std::string text; std::getline(log, text);) {
        LogLine line;
        if (!parse_line(text, line)) continue;
        if (line.category == "dump") continue;
        if (line.category == "diag" && starts_with(line.text, "diagnostics in ")) {
            if (rebooting)
                ++reboots;
            else
                lines.clear(), reboots = 0;
        }
        rebooting = line.category == "loader" && starts_with(line.text, "reboot: starting");
        lines.push_back(std::move(line));
    }
    if (lines.empty()) return;

    std::map<std::string, Problem> errors, warnings;  // by "category: text"
    std::map<std::string, std::string> auto_pictures;  // event -> automark_<n>.bmp
    std::vector<std::string> fatal;                    // fatal/crash lines, in order
    uint32_t marks = 0, auto_marks = 0;
    bool closed = false;
    for (const LogLine& line : lines) {
        if (line.category == "diag") {
            unsigned n = 0;
            char picture[64];
            if (std::sscanf(line.text.c_str(), "MARK %u at", &n) == 1) ++marks;
            const size_t colon = line.text.find("): ");
            if (std::sscanf(line.text.c_str(), "AUTO MARK %u at %*ds, frame %*u (%63[^)])", &n, picture) == 2 &&
                colon != std::string::npos) {
                auto_marks = std::max<uint32_t>(auto_marks, n);
                auto_pictures[line.text.substr(colon + 3)] = picture;
            }
            continue;
        }
        if (line.category == "render" && line.text == "window closed") closed = true;
        if (line.category == "fatal" || line.category == "crash") {
            if (fatal.size() < 16) fatal.push_back(line.category + ": " + line.text);
            continue;
        }
        if (line.level != "warn" && line.level != "error") continue;
        const bool error = line.level == "error";
        Problem& p = (error ? errors : warnings)[line.category + ": " + line.text];
        if (!p.count++) p.first = line.seconds;
    }
    for (auto* group : {&errors, &warnings})
        for (auto& [message, p] : *group) {
            const bool error = group == &errors;
            auto it = auto_pictures.find((error ? "error " : "") + message);
            if (it != auto_pictures.end()) p.picture = it->second;
        }

    // The periodic screenshots, by second, for problems that were not marked.
    std::vector<std::pair<long, std::string>> shots;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        const std::string name = entry.path().filename().string();
        if (starts_with(name, "auto_") && name.size() > 9 && name.ends_with(".bmp"))
            shots.emplace_back(std::atol(name.c_str() + 5), name);
    }
    std::sort(shots.begin(), shots.end());
    auto nearest_shot = [&](double seconds) -> std::string {
        auto it = std::lower_bound(shots.begin(), shots.end(), std::make_pair(static_cast<long>(seconds), std::string()));
        if (it != shots.end()) return it->second + " (after)";
        return shots.empty() ? "-" : shots.back().second + " (before)";
    };

    std::FILE* out = std::fopen((dir / "summary.txt").string().c_str(), "w");
    if (!out) return;
    const double length = lines.back().seconds;
    std::fprintf(out, "Playtest summary (written when the game exited)\n\n");
    std::fprintf(out, "Length: %.0f s, %u title reboot%s\n", length, reboots, reboots == 1 ? "" : "s");
    if (!fatal.empty()) {
        std::fprintf(out, "Ended: the game stopped at %.0f s:\n", length);
        for (const std::string& f : fatal) std::fprintf(out, "  %s\n", f.c_str());
    } else if (closed) {
        std::fprintf(out, "Ended: you closed the window at %.0f s\n", length);
    } else if (lines.back().category == "loader" && starts_with(lines.back().text, "stopped by ")) {
        std::fprintf(out, "Ended: %s at %.0f s\n", lines.back().text.c_str(), length);
    } else {
        const LogLine& last = lines.back();
        std::fprintf(out, "Ended: the game exited at %.0f s (last line: %s %s)\n", length, last.category.c_str(),
                     last.text.c_str());
    }
    std::fprintf(out, "Marks: %u by you (F12), %u automatic - see marks.txt\n", marks, auto_marks);

    auto table = [&](const char* title, const std::map<std::string, Problem>& group) {
        std::vector<std::pair<std::string, Problem>> sorted(group.begin(), group.end());
        std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.second.first < b.second.first; });
        std::fprintf(out, "\n%s: %zu distinct\n", title, sorted.size());
        if (sorted.empty()) return;
        std::fprintf(out, "  %7s  %6s  %-28s  %s\n", "first", "count", "screenshot", "message");
        for (const auto& [message, p] : sorted) {
            const std::string picture = p.picture.empty() ? nearest_shot(p.first) : p.picture;
            std::fprintf(out, "  %6.0fs  %6u  %-28s  %s\n", p.first, p.count, picture.c_str(), message.c_str());
        }
    };
    table("Errors", errors);
    table("Warnings", warnings);
    std::fclose(out);
}

}  // namespace loader
