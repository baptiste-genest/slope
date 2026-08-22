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
 * Named, runtime-tunable, persistent animation parameters.
 *
 * Declared in C++ next to the updater that uses them :
 *
 *   auto amp = Params::Add("wobble/amplitude", 0.2, 0., 1.);
 *   spot->updater = [=](TimeObject t){ ... (scalar)amp ... };
 *
 * The handle reads the live value (plain deref, usable in hot loops).
 * Values are edited at runtime in the Tuner panel (ImGui), saved with
 * Ctrl+S to views/params.json (only ever-edited parameters are written,
 * so untouched ones keep following their code defaults), loaded back on
 * startup, and hot-reloaded when the file is edited by hand.
 * Names may be grouped as "group/name" for the panel layout.
 *
 * A parameter is also drivable from code, with write() or Handle::set, to
 * hand a gizmo the position an animation just left it at. Such a value is
 * never saved to the file, only what was edited by hand is.
 */
class Params {
public:
    // What a parameter shows of itself while the slide reading it is on, with
    // no Tuner panel open. Handle is its direct manipulator, a 3D gizmo for a
    // vec3 and a screen handle for a vec2 ; a type that has none shows the
    // widget instead.
    enum class Visible { None, Panel, Handle, Both };

    struct Entry {
        std::string name;
        long last_read = -1; // frame stamp of the last value read
        Visible vis = Visible::None;
        virtual ~Entry() {}
        virtual bool drawUI(const char* label) = 0;
        // clamping, normalizing and gizmo sync of a value written from C++
        virtual void onWritten() {}
        virtual json toJson() const = 0;
        virtual void fromJson(const json& j) = 0;
    };
    using EntryPtr = std::shared_ptr<Entry>;

    struct ScalarEntry : Entry {
        scalar value = 0, min = 0, max = 0;
        bool drawUI(const char* label) override;
        void onWritten() override;
        json toJson() const override {return value;}
        void fromJson(const json& j) override {value = j;}
    };
    struct IntEntry : Entry {
        int value = 0, min = 0, max = 0;
        bool drawUI(const char* label) override;
        void onWritten() override;
        json toJson() const override {return value;}
        void fromJson(const json& j) override {value = j;}
    };
    struct BoolEntry : Entry {
        bool value = false;
        bool drawUI(const char* label) override;
        json toJson() const override {return value;}
        void fromJson(const json& j) override {value = j;}
    };
    struct ColorEntry : Entry {
        RGBA value = RGBA(1.f,1.f,1.f,1.f);
        bool drawUI(const char* label) override;
        json toJson() const override;
        void fromJson(const json& j) override;
    };
    // min/max apply to every component at once (a direction, a position...)
    struct Vec2Entry : Entry {
        vec2 value = vec2::Zero();
        scalar min = 0, max = 0;
        bool drawUI(const char* label) override;
        void onWritten() override;
        json toJson() const override;
        void fromJson(const json& j) override;
    };
    struct VecEntry : Entry {
        vec value = vec::Zero();
        scalar min = 0, max = 0;
        bool drawUI(const char* label) override;
        void onWritten() override;
        json toJson() const override;
        void fromJson(const json& j) override;
    };
    // a unit vector, aimed on a camera aligned ball rather than by components
    struct DirEntry : Entry {
        vec value = vec(0,0,1);
        bool drawUI(const char* label) override;
        void onWritten() override;
        json toJson() const override;
        void fromJson(const json& j) override;
    };

    // reading through a handle stamps the entry, so the Tuner panel can
    // show only the parameters used by the current slide's updaters
    template<class E, class T>
    struct Handle {
        std::shared_ptr<E> entry;
        operator T() const {entry->last_read = frame; return entry->value;}
        const T& operator*() const {entry->last_read = frame; return entry->value;}
        // drives the parameter from code, see Params::write
        void set(const T& v) const {entry->value = v; entry->onWritten();}
        // shows the parameter without the panel, chained onto Add
        Handle show(Visible v) const {entry->vis = v; return *this;}
    };
    using ScalarParam = Handle<ScalarEntry, scalar>;
    using IntParam    = Handle<IntEntry, int>;
    using BoolParam   = Handle<BoolEntry, bool>;
    using ColorParam  = Handle<ColorEntry, RGBA>;
    using Vec2Param   = Handle<Vec2Entry, vec2>;
    using VecParam    = Handle<VecEntry, vec>;
    using DirParam    = Handle<DirEntry, vec>;

    // min == max means unconstrained (drag instead of slider)
    static ScalarParam Add(const std::string& name, scalar def, scalar min = 0, scalar max = 0);
    static IntParam    AddInt(const std::string& name, int def, int min = 0, int max = 0);
    static BoolParam   AddBool(const std::string& name, bool def);
    static ColorParam  AddColor(const std::string& name, const RGBA& def);
    static Vec2Param   AddVec2(const std::string& name, const vec2& def,
                               scalar min = 0, scalar max = 0);
    static VecParam    AddVec(const std::string& name, const vec& def,
                              scalar min = 0, scalar max = 0);
    static DirParam    AddDir(const std::string& name, const vec& def);

    // lazy variant, registers on first use then reads
    static scalar get(const std::string& name, scalar def, scalar min = 0, scalar max = 0);

    // Reading a parameter by name, for callers that never hold a handle, which
    // is all the deck and the snippet namespace ever have. Both return how many
    // components the parameter has, 0 when the name is not registered at all,
    // so they also answer whether it exists.
    static int components(const std::string& name);
    static int read(const std::string& name, scalar* out4);

    // Writing a parameter by name, the counterpart of read(). Returns how many
    // components were written, 0 when the name is not registered or when fewer
    // than that many were given. Values are clamped to the declared bounds.
    // A write is a drive and not an edit, so it never marks the parameter for
    // saving, and it outranks a value the Tuner is holding.
    static int  write(const std::string& name, const scalar* in, int n);
    static bool write(const std::string& name, scalar v);
    static bool write(const std::string& name, const vec2& v);
    static bool write(const std::string& name, const vec& v);
    static bool write(const std::string& name, const RGBA& v);

    // A parameter shown on its own, so a handle is grabbable as soon as the
    // slide reading it is reached. The Tuner panel stays the way to reach
    // every other one.
    //   Params::AddVec("arap/handle", p0).show(Params::Visible::Handle);
    static void setVisible(const std::string& name, Visible v);
    static Visible getVisible(const std::string& name);
    // "none", "panel", "handle" or "both", how a deck manifest says it
    static Visible parseVisible(const std::string& mode);

    static void DrawPanel();
    // the manipulators and widgets of the visible parameters this slide reads,
    // called every frame. The widgets are left to the panel when it is open
    static void DrawVisible(bool panel_open);

    // advances the frame stamp; called once per frame by the slideshow
    static void NewFrame() {frame++;}

    static bool hasDirty();

    // 3D manipulators on the vec parameters, opted in from the panel
    static bool hasLiveGizmo();
    // the cursor is on one of them, the only moment a shown handle takes the
    // mouse from the slides
    static bool cursorOnGizmo();
    static void clearGizmos();

    static void saveAllDirty();
    static void HotReloadIfModified();

private:
    static std::map<std::string, EntryPtr> registry;
    static std::set<std::string> dirty;   // edited since last save
    // manipulators turned on by a visible parameter rather than by the panel
    static std::set<std::string> auto_manipulators;
    static std::set<std::string> edited;  // edited ever (this session or a past one)
    static json file_values;
    static bool file_loaded;
    static long frame;
    static std::filesystem::file_time_type last_modified;

    static path file();
    static void ensureLoaded();

    template<class E>
    static std::shared_ptr<E> addEntry(const std::string& name);

    // A parameter can be declared again while the show runs (the deck loader
    // re-declares a shader's uniforms on every hot reload). An edit not yet
    // saved is then the newest value there is, and outranks both the declared
    // default and the file, true when the caller must leave the value alone.
    static bool keepEditedValue(const std::string& name);
    // the saved value, if any, over the freshly declared default
    static void applyFileValue(const EntryPtr& e, const std::string& name);
};

}

#endif // PARAMS_H
