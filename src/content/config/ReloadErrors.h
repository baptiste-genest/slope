#pragma once

#include <filesystem>
#include <map>
#include <mutex>
#include <string>

namespace slope {

// the last reload error of each watched file, one per kind of reload, for the file editor
struct ReloadErrors {
    using Path = std::filesystem::path;

    static void report(const Path& file, const std::string& kind, const std::string& msg)
    {
        if (file.empty())
            return;
        const Path k = key(file);
        std::lock_guard lock(mutex());
        table()[k][kind] = msg;
    }

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

    // every file with an error, its messages joined, keyed by canonical path
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
