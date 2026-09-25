#ifndef PARAMS_H
#define PARAMS_H

#include "libslope.h"
#include "content/config/Options.h"
#include <filesystem>
#include <map>
#include <set>
#include "extern/json.hpp"

namespace slope {

/*
 * Named animation parameters that can be tuned while the show runs and are saved.
 *
 * They are declared in C++ next to the updater that uses them.
 *
 *   auto amp = Params::Add("wobble/amplitude", 0.2, 0., 1.);
 *   spot->updater = [=](TimeObject t){ ... (scalar)amp ... };
 *
 * The handle reads the live value.
 * Values are edited in the Tuner panel and saved with Ctrl+S to views/params.json.
 * Only parameters that were edited are written, so the others follow the defaults of the code.
 * The file is loaded at startup and reloaded when it is edited by hand.
 * Names can be grouped as "group/name" in the panel.
 *
 * Code can also set a parameter with write() or Handle::set, for example to give a gizmo
 * the position where an animation left it. Such a value is never saved.
 */
class Params {
public:
    // What a parameter shows while the slide that reads it is on and the Tuner is closed.
    // Handle is a direct manipulator, a 3D gizmo for a vec3 and a screen handle for a vec2.
    // A type without one shows its widget instead.
    enum class Visible { None,
                         Panel,
                         Handle,
                         Both };

    // Storage of one parameter.
    struct Entry {
        std::string name;
        // Frame of the last read.
        long last_read = -1;
        Visible vis = Visible::None;
        virtual ~Entry() {}
        // Draws the widget of the panel and returns true when the value changed.
        virtual bool drawUI(const char* label) = 0;
        // Clamps, normalizes and updates the gizmo after a write from C++.
        virtual void onWritten() {}
        // Value as saved in the file, and back.
        virtual json toJson() const = 0;
        virtual void fromJson(const json& j) = 0;
    };
    using EntryPtr = std::shared_ptr<Entry>;

    struct ScalarEntry : Entry {
        scalar value = 0, min = 0, max = 0;
        // When true, there is no upper bound but the value stays above min.
        bool open_max = false;
        bool drawUI(const char* label) override;
        void onWritten() override;
        json toJson() const override { return value; }
        void fromJson(const json& j) override { value = j; }
    };
    struct IntEntry : Entry {
        int value = 0, min = 0, max = 0;
        bool drawUI(const char* label) override;
        void onWritten() override;
        json toJson() const override { return value; }
        void fromJson(const json& j) override { value = j; }
    };
    struct BoolEntry : Entry {
        bool value = false;
        bool drawUI(const char* label) override;
        json toJson() const override { return value; }
        void fromJson(const json& j) override { value = j; }
    };
    // One name of a fixed list, drawn as a dropdown.
    // It is saved by name, so reordering the options does not change the file.
    struct EnumEntry : Entry {
        int value = 0;
        std::vector<std::string> options;
        bool drawUI(const char* label) override;
        void onWritten() override;
        json toJson() const override;
        void fromJson(const json& j) override;
        // The name currently chosen.
        const std::string& choice() const;
    };
    struct ColorEntry : Entry {
        RGBA value = RGBA(1.f, 1.f, 1.f, 1.f);
        bool drawUI(const char* label) override;
        json toJson() const override;
        void fromJson(const json& j) override;
    };
    // The bounds min and max apply to both components.
    struct Vec2Entry : Entry {
        vec2 value = vec2::Zero();
        scalar min = 0, max = 0;
        bool drawUI(const char* label) override;
        void onWritten() override;
        json toJson() const override;
        void fromJson(const json& j) override;
    };
    // The bounds min and max apply to the three components.
    struct VecEntry : Entry {
        vec value = vec::Zero();
        scalar min = 0, max = 0;
        bool drawUI(const char* label) override;
        void onWritten() override;
        json toJson() const override;
        void fromJson(const json& j) override;
    };
    // A unit vector, edited by aiming on a ball facing the camera.
    struct DirEntry : Entry {
        vec value = vec(0, 0, 1);
        bool drawUI(const char* label) override;
        void onWritten() override;
        json toJson() const override;
        void fromJson(const json& j) override;
    };

    // Reads a parameter. Each read is recorded, so the Tuner shows only the parameters
    // used by the current slide.
    template <class E, class T>
    struct Handle {
        std::shared_ptr<E> entry;
        operator T() const {
            entry->last_read = frame;
            return entry->value;
        }
        const T& operator*() const {
            entry->last_read = frame;
            return entry->value;
        }
        // Sets the value from code, like Params::write.
        void set(const T& v) const {
            entry->value = v;
            entry->onWritten();
        }
        // Chosen visibility, meant to be chained after Add.
        Handle show(Visible v) const {
            entry->vis = v;
            return *this;
        }
        // For an enum, true when it holds this option.
        bool is(const std::string& option) const {
            entry->last_read = frame;
            if constexpr (std::is_same_v<E, EnumEntry>)
                return entry->choice() == option;
            else
                return false;
        }
        // For an enum, the option it holds.
        const std::string& choice() const {
            entry->last_read = frame;
            static const std::string none;
            if constexpr (std::is_same_v<E, EnumEntry>)
                return entry->choice();
            else
                return none;
        }
    };
    using ScalarParam = Handle<ScalarEntry, scalar>;
    using IntParam = Handle<IntEntry, int>;
    using BoolParam = Handle<BoolEntry, bool>;
    using ColorParam = Handle<ColorEntry, RGBA>;
    // Reads as the index of the chosen option.
    using EnumParam = Handle<EnumEntry, int>;
    using Vec2Param = Handle<Vec2Entry, vec2>;
    using VecParam = Handle<VecEntry, vec>;
    using DirParam = Handle<DirEntry, vec>;

    // Declares a scalar. With min equal to max there is no bound and the panel uses a drag box.
    static ScalarParam Add(const std::string& name, scalar def, scalar min = 0, scalar max = 0);
    // Declares a scalar without upper bound that stays at or above `min`.
    static ScalarParam AddAtLeast(const std::string& name, scalar def, scalar min = 0);
    // Declares an integer. With min equal to max there is no bound.
    static IntParam AddInt(const std::string& name, int def, int min = 0, int max = 0);
    // Declares a boolean.
    static BoolParam AddBool(const std::string& name, bool def);
    // Declares a color.
    static ColorParam AddColor(const std::string& name, const RGBA& def);
    // Declares a choice among names, with the default given by name.
    //   auto side = Params::AddEnum("fig/yticks", {"left","right","none"}, "left");
    //   if (side.is("none")) ...
    // Declaring it again keeps the current option if the new list still has it.
    static EnumParam AddEnum(const std::string& name,
                             std::vector<std::string> options,
                             const std::string& def);
    // The chosen option of an enum, for code without a handle. Empty when the name is unknown.
    static std::string choice(const std::string& name);

    // Changes the default of a parameter, keeping its type and bounds.
    // A saved or edited value still wins over it.
    // The value must have the shape used in the file, otherwise it throws.
    // Returns false when the name is not registered.
    static bool setDefault(const std::string& name, const json& value);

    // Sets a parameter from a json value, like write() does from numbers.
    // It is never saved and it wins over the value held by the Tuner.
    static bool drive(const std::string& name, const json& value);
    // Value in the shape used in the file, or null when the name is unknown.
    static json valueOf(const std::string& name);
    // Declares a 2D vector.
    static Vec2Param AddVec2(const std::string& name, const vec2& def,
                             scalar min = 0, scalar max = 0);
    // Declares a 3D vector.
    static VecParam AddVec(const std::string& name, const vec& def,
                           scalar min = 0, scalar max = 0);
    // Declares a unit 3D vector.
    static DirParam AddDir(const std::string& name, const vec& def);

    // Declares the parameter on the first call, then returns its value.
    static scalar get(const std::string& name, scalar def, scalar min = 0, scalar max = 0);

    // Read a parameter by name, for the deck and snippets which hold no handle.
    // Both return the number of components, or 0 when the name is not registered.
    // Number of components of a parameter.
    static int components(const std::string& name);
    // Copies the components of a parameter into out4, which has room for 4 values.
    static int read(const std::string& name, scalar* out4);

    // Write a parameter by name, the opposite of read().
    // They return the number of components written, or 0 when the name is not registered
    // or fewer values than needed were given.
    // Values are clamped to the declared bounds.
    // A write is not an edit, so it is never saved and it wins over the value held by the Tuner.
    static int write(const std::string& name, const scalar* in, int n);
    static bool write(const std::string& name, scalar v);
    static bool write(const std::string& name, const vec2& v);
    static bool write(const std::string& name, const vec& v);
    static bool write(const std::string& name, const RGBA& v);

    // Shows a parameter by itself, so its handle can be grabbed as soon as the slide is reached.
    //   Params::AddVec("arap/handle", p0).show(Params::Visible::Handle);
    static void setVisible(const std::string& name, Visible v);
    // Current visibility of a parameter.
    static Visible getVisible(const std::string& name);
    // Converts "none", "panel", "handle" or "both", as written in a deck.
    static Visible parseVisible(const std::string& mode);

    // Draws the Tuner panel.
    static void DrawPanel();
    // Draws the manipulators and widgets of the visible parameters read by this slide.
    // The widgets are skipped when the panel is open.
    static void DrawVisible(bool panel_open);

    // Starts a new frame. Called once per frame by the slideshow.
    static void NewFrame() { frame++; }

    // True when some parameter was edited and not saved.
    static bool hasDirty();

    // True when a 3D manipulator is on, turned on from the panel.
    static bool hasLiveGizmo();
    // True when the cursor is on a manipulator, so the mouse is not used to change slide.
    static bool cursorOnGizmo();
    // Removes every manipulator.
    static void clearGizmos();

    // Writes the edited parameters to the file.
    static void saveAllDirty();
    // Reads the file again when it changed on disk.
    static void HotReloadIfModified();

private:
    static std::map<std::string, EntryPtr> registry;
    // Edited since the last save.
    static std::set<std::string> dirty;
    // Manipulators turned on by a visible parameter and not by the panel.
    static std::set<std::string> auto_manipulators;
    // Edited at any time, in this session or a past one.
    static std::set<std::string> edited;
    static json file_values;
    static bool file_loaded;
    static long frame;
    static std::filesystem::file_time_type last_modified;

    // Path of the parameter file.
    static path file();
    // Loads the file once.
    static void ensureLoaded();

    template <class E>
    static std::shared_ptr<E> addEntry(const std::string& name);

    // A parameter can be declared again while the show runs, for example when the deck reloads.
    // An edit not yet saved is then the newest value and wins over the default and the file.
    // Returns true when the caller must leave the value alone.
    static bool keepEditedValue(const std::string& name);
    // Replaces the freshly declared default by the saved value, if any.
    static void applyFileValue(const EntryPtr& e, const std::string& name);
};

} // namespace slope

#endif // PARAMS_H
