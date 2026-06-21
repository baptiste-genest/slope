#include "TimeTracker.h"
#include "imgui.h"
#include <sstream>
#include <iomanip>
#include <cmath>
#include <set>

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

void slope::TimeTracker::start()
{
    last_recorded_time = Time::now();
    started = true;
}

void slope::TimeTracker::record(const std::string& slide_title)
{
    if (!started) {
        start();
        return;
    }
    auto now = Time::now();
    auto dt  = TimeFrom(last_recorded_time);
    time_per_slide_group[slide_title] += dt;
    time_from_start += dt;
    last_recorded_time = now;
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
    ImGui::Separator();

    std::set<std::string> done;
    if (ImGui::BeginTable("SlideTable", 2, ImGuiTableFlags_SizingStretchProp)) {
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
        }
        ImGui::EndTable();
    }
    ImGui::End();
}
