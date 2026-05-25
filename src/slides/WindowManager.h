#pragma once

#include "imgui.h"
#include <stdexcept>
#include <string>
#include <functional>
#include <set>

namespace slope {

enum class WindowType {
    none,
    Palette,
    Camera,
    SlideMenu,
    Transform,
    DragAndDrop,
    PolyscopeGUI
};

class WindowManager {
public:

    bool Toggle(WindowType w) {
        if (active == WindowType::none){
            active = w;
            return true;
        }
        else if (active == w)
            active = WindowType::none;
        return false;
    }

    bool isAnyOpen() const {
        return active != WindowType::none;
    }

    bool isOpen(WindowType w) const {
        return active == w;
    }

    void CloseAll() {
        active = WindowType::none;
    }

    bool isOtherOpen(WindowType w) const {
        return active != WindowType::none && active != w;
    }

    WindowManager(){}

private:
    WindowType active = WindowType::none;
};

class InputManager {

    struct KeyboardInput {
        std::string description,shortcut;
        ImGuiKey trigger;
        bool isPopUp;
        std::function<void(void)> callback;
    };
    std::vector<KeyboardInput> inputs;
    std::set<ImGuiKey> active_triggers;

public:

    const std::vector<KeyboardInput>& getInputs() const {
        return inputs;
    }

    void checkConflict() {
        // check if two triggers are the same, throw if so
        std::unordered_map<ImGuiKey,std::vector<std::string>> trigger_map;
        for (const auto& input : inputs) {
            trigger_map[input.trigger].push_back(input.description);
        }
        for (const auto& [trigger, descriptions] : trigger_map) {
            if (descriptions.size() > 1) {
                std::string error_msg = "Conflict detected for trigger " + std::to_string(trigger) + ": ";
                for (const auto& desc : descriptions) {
                    error_msg += desc + ", ";
                }
                throw std::runtime_error(error_msg);
            }
        }
    }

    void addInput(std::string description,std::string shortcut,ImGuiKey trigger,bool isPopUp = false) {
        if (active_triggers.contains(trigger))
            throw std::runtime_error("Trigger " + std::to_string(trigger) + " already used for another input");
        inputs.emplace_back(description,shortcut,trigger,isPopUp,[]() {});
    }

    // with callback
    void addInput(std::string description,std::string shortcut,ImGuiKey trigger,std::function<void(void)> callback,bool isPopUp = false) {
        if (active_triggers.contains(trigger))
            throw std::runtime_error("Trigger " + std::to_string(trigger) + " already used for another input");
        inputs.emplace_back(description,shortcut,trigger,isPopUp,callback);
    }

    void printInputs();

};

} // namespace slope
