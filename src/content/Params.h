#ifndef PARAMS_H
#define PARAMS_H

#include "../libslope.h"
#include "Options.h"
#include <filesystem>
#include <map>
#include <set>

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
 */
class Params {
public:
    struct Entry {
        std::string name;
        long last_read = -1; // frame stamp of the last value read
        virtual ~Entry() {}
        virtual bool drawUI(const char* label) = 0;
        virtual json toJson() const = 0;
        virtual void fromJson(const json& j) = 0;
    };
    using EntryPtr = std::shared_ptr<Entry>;

    struct ScalarEntry : Entry {
        scalar value = 0, min = 0, max = 0;
        bool drawUI(const char* label) override;
        json toJson() const override {return value;}
        void fromJson(const json& j) override {value = j;}
    };
    struct IntEntry : Entry {
        int value = 0, min = 0, max = 0;
        bool drawUI(const char* label) override;
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

    // reading through a handle stamps the entry, so the Tuner panel can
    // show only the parameters used by the current slide's updaters
    template<class E, class T>
    struct Handle {
        std::shared_ptr<E> entry;
        operator T() const {entry->last_read = frame; return entry->value;}
        const T& operator*() const {entry->last_read = frame; return entry->value;}
    };
    using ScalarParam = Handle<ScalarEntry, scalar>;
    using IntParam    = Handle<IntEntry, int>;
    using BoolParam   = Handle<BoolEntry, bool>;
    using ColorParam  = Handle<ColorEntry, RGBA>;

    // min == max means unconstrained (drag instead of slider)
    static ScalarParam Add(const std::string& name, scalar def, scalar min = 0, scalar max = 0);
    static IntParam    AddInt(const std::string& name, int def, int min = 0, int max = 0);
    static BoolParam   AddBool(const std::string& name, bool def);
    static ColorParam  AddColor(const std::string& name, const RGBA& def);

    // lazy variant : registers on first use, then reads
    static scalar get(const std::string& name, scalar def, scalar min = 0, scalar max = 0);

    static void DrawPanel();

    // advances the frame stamp; called once per frame by the slideshow
    static void NewFrame() {frame++;}

    static bool hasDirty();
    static void saveAllDirty();
    static void HotReloadIfModified();

private:
    static std::map<std::string, EntryPtr> registry;
    static std::set<std::string> dirty;   // edited since last save
    static std::set<std::string> edited;  // edited ever (this session or a past one)
    static json file_values;
    static bool file_loaded;
    static long frame;
    static std::filesystem::file_time_type last_modified;

    static path file();
    static void ensureLoaded();

    template<class E>
    static std::shared_ptr<E> addEntry(const std::string& name);
};

}

#endif // PARAMS_H
