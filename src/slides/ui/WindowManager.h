#pragma once

#include "imgui.h"
#include <stdexcept>
#include <string>
#include <functional>
#include <set>

namespace slope {

// Windows that can be open. Only one is open at a time.
enum class WindowType {
    none,
    Camera,
    SlideMenu,
    Transform,
    DragAndDrop,
    PolyscopeGUI,
    Tuner,
    FileEditor,
    QuitWarning
};

// Keeps track of which window is open.
class WindowManager {
public:

    // Opens w if no window is open and returns true. Closes w if it is the open one.
    // Does nothing when another window is open.
    bool Toggle(WindowType w) {
        if (active == WindowType::none){
            active = w;
            return true;
        }
        else if (active == w)
            active = WindowType::none;
        return false;
    }

    // True when any window is open.
    bool isAnyOpen() const {
        return active != WindowType::none;
    }

    // True when w is open.
    bool isOpen(WindowType w) const {
        return active == w;
    }

    // Closes the open window.
    void CloseAll() {
        active = WindowType::none;
    }

    // True when a window other than w is open.
    bool isOtherOpen(WindowType w) const {
        return active != WindowType::none && active != w;
    }

    // True when the open window is a modal one, which blocks the other inputs.
    bool isModalOpen() const {
        return active == WindowType::Camera || active == WindowType::QuitWarning;
    }

    WindowManager(){}

private:
    WindowType active = WindowType::none;
};

// List of keyboard shortcuts with their description.
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

    // Every registered input.
    const std::vector<KeyboardInput>& getInputs() const {
        return inputs;
    }

    // Throws if two inputs use the same trigger.
    void checkConflict() {
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

    // Registers an input with a description, the text of the shortcut and its key.
    // Throws if the key is already used.
    void addInput(std::string description,std::string shortcut,ImGuiKey trigger,bool isPopUp = false) {
        if (active_triggers.contains(trigger))
            throw std::runtime_error("Trigger " + std::to_string(trigger) + " already used for another input");
        inputs.emplace_back(description,shortcut,trigger,isPopUp,[]() {});
    }

    // Same, with a function called when the key is pressed.
    void addInput(std::string description,std::string shortcut,ImGuiKey trigger,std::function<void(void)> callback,bool isPopUp = false) {
        if (active_triggers.contains(trigger))
            throw std::runtime_error("Trigger " + std::to_string(trigger) + " already used for another input");
        inputs.emplace_back(description,shortcut,trigger,isPopUp,callback);
    }

    // Registers an entry that only documents a shortcut. The shortcut is handled elsewhere and not by InputManager.
    void addInput(std::string description,std::string shortcut) {
        inputs.emplace_back(description,shortcut,ImGuiKey_None,false,[]() {});
    }

    // Prints every input to the log.
    void printInputs();

};

} // namespace slope
