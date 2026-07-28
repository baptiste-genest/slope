#include "TimeTracker.h"
#include "../content/Options.h"
#include "imgui.h"
#include <sstream>
#include <iomanip>
#include <cmath>
#include <set>
#include <fstream>

std::string slope::TimeTracker::formatTime(float totalSeconds)
{
    int seconds = std::round(totalSeconds);
    int hours   = seconds / 3600;
    int minutes = (seconds % 3600) / 60;
    int secs    = seconds % 60;

    std::ostringstream out;
    out << std::setw(2) << std::setfill('0') << hours   << "h:"
        << std::setw(2) << minutes << "m:"
        << std::setw(2) << secs    << "s";
    return out.str();
}

std::string slope::TimeTracker::formatDelta(float seconds)
{
    int s = std::round(std::abs(seconds));
    std::ostringstream out;
    out << (seconds < 0 ? '-' : '+') << s/60 << "m" << std::setw(2) << std::setfill('0') << s%60 << "s";
    return out.str();
}

slope::path slope::TimeTracker::file()
{
    return path(Options::ProjectViewsPath) / "timings.json";
}

void slope::TimeTracker::load()
{
    // the rehearsal timer is opt-in : without it we never surface the previous
    // run, so there is no reason to read it back
    if (!Options::Rehearse)
        return;
    std::ifstream f(file());
    if (!f.is_open())
        return;
    try {
        json j;
        f >> j;
        previous_time_from_start = j.value("total", 0.0);
        previous_time_per_slide_group.clear();
        // must be a named object : calling .items() on the temporary returned
        // by value() leaves the iterators dangling
        const json sections = j.value("sections", json::object());
        for (const auto& [k, v] : sections.items())
            if (v.is_number())
                previous_time_per_slide_group[k] = v.get<double>();
        has_previous = true;
        spdlog::info("previous run: {}", formatTime(previous_time_from_start));
    } catch (const std::exception& e) {
        spdlog::warn("could not parse {}: {}", file().string(), e.what());
    }
}

void slope::TimeTracker::save() const
{
    // a run that never left the first slide is not a rehearsal worth recording
    if (!started || time_from_start < 1)
        return;
    json j;
    j["total"] = time_from_start;
    json sections = json::object();
    for (const auto& [k, v] : time_per_slide_group)
        sections[k] = v;
    j["sections"] = sections;

    std::error_code ec;
    std::filesystem::create_directories(Options::ProjectViewsPath, ec);
    std::ofstream f(file());
    if (!f.is_open()) {
        spdlog::error("could not write {}", file().string());
        return;
    }
    f << j.dump(1) << std::endl;
    spdlog::info("timings saved ({})", formatTime(time_from_start));
}

void slope::TimeTracker::start()
{
    if (!Options::Rehearse)
        return;
    last_recorded_time = Time::now();
    started = true;
}

void slope::TimeTracker::record(const std::string& slide_title)
{
    if (!Options::Rehearse)
        return;
    if (!started) {
        start();
        return;
    }
    if (paused) {
        // keep the reference moving, otherwise resuming would bill the whole
        // paused interval to the section we stopped on
        last_recorded_time = Time::now();
        return;
    }
    auto now = Time::now();
    auto dt  = TimeFrom(last_recorded_time);
    time_per_slide_group[slide_title] += dt;
    time_from_start += dt;
    last_recorded_time = now;
}

void slope::TimeTracker::togglePause()
{
    paused = !paused;
    last_recorded_time = Time::now();
    if (paused)
        spdlog::info("rehearsal timer paused at {}", formatTime(time_from_start));
    else
        spdlog::info("rehearsal timer resumed at {}", formatTime(time_from_start));
}

void slope::TimeTracker::reset()
{
    time_from_start = 0;
    for (auto& [k, v] : time_per_slide_group)
        v = 0;
    last_recorded_time = Time::now();
}

void slope::TimeTracker::drawMenu(int n_slides,
                                   const std::function<std::string(int)>& get_title,
                                   const std::function<void(int)>& go_to_slide)
{
    ImGui::Begin("Slides");
    ImGui::Text("Elapsed: %s", formatTime(time_from_start).c_str());
    if (paused) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.f, 0.7f, 0.2f, 1.f), "[PAUSED]");
    }
    if (has_previous) {
        ImGui::SameLine();
        ImGui::TextDisabled("| last run: %s (%s)",
                            formatTime(previous_time_from_start).c_str(),
                            formatDelta(time_from_start - previous_time_from_start).c_str());
    }
    ImGui::Separator();

    const int n_cols = has_previous ? 3 : 2;
    std::set<std::string> done;
    if (ImGui::BeginTable("SlideTable", n_cols, ImGuiTableFlags_SizingStretchProp)) {
        for (int i = 0; i < n_slides; i++) {
            auto title = get_title(i);
            if (done.contains(title)) continue;
            done.insert(title);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if (ImGui::Button(title.c_str()))
                go_to_slide(i);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(formatTime(time_per_slide_group[title]).c_str());
            if (has_previous) {
                ImGui::TableSetColumnIndex(2);
                auto it = previous_time_per_slide_group.find(title);
                if (it == previous_time_per_slide_group.end())
                    ImGui::TextDisabled("-");
                else {
                    // only meaningful once the section has actually been visited
                    auto d = time_per_slide_group[title] - it->second;
                    if (time_per_slide_group[title] <= 0)
                        ImGui::TextDisabled("%s", formatTime(it->second).c_str());
                    else
                        ImGui::TextDisabled("%s", formatDelta(d).c_str());
                }
            }
        }
        ImGui::EndTable();
    }
    ImGui::End();
}
