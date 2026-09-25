#pragma once

#include <filesystem>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

namespace slope {

// Keeps the last reload error of each watched file, one per kind of reload, for the file editor.
struct ReloadErrors {
    using Path = std::filesystem::path;

    // Stores an error message for a file and a kind of reload.
    static void report(const Path& file, const std::string& kind, const std::string& msg)
    {
        if (file.empty())
            return;
        const Path k = key(file);
        std::lock_guard lock(mutex());
        table()[k][kind] = msg;
    }

    // Removes the error of a file for a kind of reload.
    static void clear(const Path& file, const std::string& kind)
    {
        if (file.empty())
            return;
        const Path k = key(file);
        std::lock_guard lock(mutex());
        auto it = table().find(k);
        if (it == table().end())
            return;
        it->second.erase(kind);
        if (it->second.empty())
            table().erase(it);
    }

    // Reports a file that is needed and absent, then throws. The editor offers to create it.
    [[noreturn]] static void missingFile(const Path& file, const std::string& msg)
    {
        report(file, "missing", msg);
        throw std::runtime_error(msg);
    }

    // Files reported as missing.
    static std::vector<Path> missingFiles()
    {
        std::lock_guard lock(mutex());
        std::vector<Path> out;
        for (const auto& [file, kinds] : table())
            if (kinds.count("missing"))
                out.push_back(file);
        return out;
    }

    // Forgets the missing files. Called before a rebuild, which reports again those still missing.
    static void clearMissing()
    {
        std::lock_guard lock(mutex());
        for (auto it = table().begin(); it != table().end();) {
            it->second.erase("missing");
            it = it->second.empty() ? table().erase(it) : std::next(it);
        }
    }

    // Every file with an error, with its messages joined, keyed by canonical path.
    static std::map<Path, std::string> all()
    {
        std::lock_guard lock(mutex());
        std::map<Path, std::string> out;
        for (const auto& [file, kinds] : table()) {
            std::string& m = out[file];
            for (const auto& [kind, msg] : kinds)
                m += (m.empty() ? "" : "\n") + msg;
        }
        return out;
    }

private:
    // Canonical form of a path, used as key of the table.
    static Path key(const Path& p)
    {
        std::error_code ec;
        auto v = std::filesystem::weakly_canonical(p, ec);
        return ec ? p : v;
    }
    static std::mutex& mutex() { static std::mutex m; return m; }
    static std::map<Path, std::map<std::string, std::string>>& table()
    {
        static std::map<Path, std::map<std::string, std::string>> t;
        return t;
    }
};

} // namespace slope
