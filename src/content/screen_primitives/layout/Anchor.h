#ifndef ANCHOR_H
#define ANCHOR_H

#include "content/config/Options.h"
#include "content/config/io.h"
#include <unordered_map>
#include <array>

namespace slope {

class Anchor;
using AnchorPtr = std::shared_ptr<Anchor>;

// What the editor can change about a labeled primitive. It is saved in its .pos file.
struct AnchorState {
    // Position relative to the global anchor.
    double x = 0.5, y = 0.5;
    scalar scale = 1;
    // In radians.
    scalar angle = 0;
    scalar alpha = 1;
};

// A screen position with an optional scale, angle and opacity, used to place primitives.
class Anchor
{
public:
    // True when the anchor is stored under a label and can be edited.
    virtual bool isPersistent() const {return false;}

    // Moves the anchor, ignored by anchors that cannot move.
    virtual void updatePos(const vec2& p) = 0;

    // Relative position.
    virtual vec2 getPos() const = 0;

    virtual scalar getScale() const {return 1;}

    // In radians.
    virtual scalar getAngle() const {return 0;}

    virtual scalar getAlpha() const {return 1;}
};

class AbsoluteAnchor;
using AbsoluteAnchorPtr = std::shared_ptr<Anchor>;
// An anchor at a fixed position.
class AbsoluteAnchor : public Anchor
{
protected:
    vec2 pos;
public:
    // Anchor at the relative position p.
    AbsoluteAnchor(const vec2& p) {pos = p;}

    // Builds an anchor at p.
    static AbsoluteAnchorPtr Add(const vec2& p) {
        return std::make_shared<AbsoluteAnchor>(p);
    }
    virtual vec2 getPos() const override {
        return pos;
    }
    virtual void updatePos(const vec2 &p) override {
        pos = p;
    }
};

// Origin of the screen, used by anchors that are relative to something.
extern AbsoluteAnchorPtr GlobalAnchor;

class LabelAnchor;
using LabelAnchorPtr = std::shared_ptr<LabelAnchor>;
// An anchor whose position, scale, angle and opacity are stored in views/<label>.pos and edited by dragging.
class LabelAnchor : public Anchor
{
protected:
    std::string label = "";

    inline static std::unordered_map<std::string, AnchorState> session_cache;
    inline static std::set<std::string> dirty_labels;

    // Data for the startup report, see reportLabelIssues().
    // Number of anchors that use each label.
    inline static std::map<std::string,int> label_usage;
    // Labels that had no .pos file yet.
    inline static std::set<std::string> created_labels;
    // Same, emptied by whoever suggests a shorter name for them.
    inline static std::set<std::string> fresh_labels;
    // Names the deck already uses for something, set by its loader.
    inline static std::set<std::string> reserved_names;
    // Labels whose .pos file exists but cannot be read.
    inline static std::set<std::string> unreadable_labels;

public:

    virtual bool isPersistent() const override { return true; }

    // Anchor for a label. A label with no file starts at the default position.
    LabelAnchor(std::string l) : label(l) {
        label_usage[label]++;
        writeAtLabel(AnchorState{},false);
    }

    // The label.
    const std::string& getLabel() const { return label; }

    // Logs the anchors created at their default position, which is usually a typo in a label,
    // and the .pos files that no anchor uses.
    static void reportLabelIssues();

    // Builds an anchor for a label.
    static LabelAnchorPtr Add(const std::string& l) {
        return std::make_shared<LabelAnchor>(l);
    }

    virtual vec2 getPos() const override {
        AnchorState s = readFromLabel();
        return GlobalAnchor->getPos() + vec2(s.x,s.y);
    }

    virtual void updatePos(const vec2& p) override {
        spdlog::warn("cannot change labeled anchor position by hand");
    }

    virtual scalar getScale() const override {
        return readFromLabel().scale;
    }

    virtual scalar getAngle() const override {
        return readFromLabel().angle;
    }

    virtual scalar getAlpha() const override {
        return readFromLabel().alpha;
    }

    // Stores a state for the label. Without overwrite, an existing state is kept.
    void writeAtLabel(const AnchorState& s, bool overwrite) const;

    // Stores a state for the session. It is written to disk by saveAllDirty.
    void writeToSession(const AnchorState& s) const;
    // Same, without an anchor. The editor uses it for undo, where a new LabelAnchor would count as a use of the label.
    static void writeToSessionAt(const std::string& label, const AnchorState& s);
    // Writes every edited label to its .pos file.
    static void saveAllDirty();

    // Suggests a short new label made of consonant and vowel pairs, so it can be read aloud and has no digits.
    // Empty when every name tried was taken.
    static std::string suggestLabel();

    // Sets the names used by the deck for its items, which suggestions avoid.
    static void reserveNames(std::set<std::string> names) {
        reserved_names = std::move(names);
    }

    // Returns the labels placed for the first time since the last call.
    static std::set<std::string> takeFreshLabels();
    // True when some label was edited and not saved.
    static bool hasDirty();

    // Change one field of the stored state. The overwrite argument is unused.
    void writePosAtLabel(scalar x, scalar y, bool /*overwrite*/) const {
        AnchorState s = readFromLabel();
        s.x = x; s.y = y;
        writeToSession(s);
    }
    void writeScaleAtLabel(scalar v, bool /*overwrite*/) const {
        AnchorState s = readFromLabel();
        s.scale = v;
        writeToSession(s);
    }
    void writeAngleAtLabel(scalar v) const {
        AnchorState s = readFromLabel();
        s.angle = v;
        writeToSession(s);
    }
    void writeAlphaAtLabel(scalar v) const {
        AnchorState s = readFromLabel();
        s.alpha = v;
        writeToSession(s);
    }
    // Stored state of the label.
    AnchorState readFromLabel() const;
};

// Relative screen position of a point of the scene.
vec2 WorldToScreen(const vec& p);

// Point of the scene that is drawn at a relative screen position.
vec ScreenToWorld(const vec2& p);

// An anchor whose position is computed every frame by a function.
class DynamicAnchor : public Anchor
{
protected:
    std::function<vec2()> anchor;

    // Function giving the screen position of the 3D point returned by track.
    static std::function<vec2()> trackScreen(const std::function<vec()>& track) {
        return [track] () -> vec2 {
            return WorldToScreen(track());
        };
    }


    // Anchor interface
public:
    virtual void updatePos(const vec2 &p) override {}
    virtual vec2 getPos() const override {
        return anchor();
    }

    // Anchor at the position returned by f.
    DynamicAnchor(std::function<vec2()> f) : anchor(f) {}

    // Anchor at the screen position of the 3D point returned by f.
    static std::shared_ptr<DynamicAnchor> AddTracker(const std::function<vec()>& f) {
        return std::make_shared<DynamicAnchor>(trackScreen(f));
    }

    // Anchor at the position returned by f.
    static std::shared_ptr<DynamicAnchor> Add(std::function<vec2()> f) {
        return std::make_shared<DynamicAnchor>(f);
    }
    // Anchor at the screen position of a fixed 3D point.
    static std::shared_ptr<DynamicAnchor> Add(const vec& x) {
        std::function<vec()> p = [x](){return x;};
        return AddTracker(p);
    }

};
}

#endif // ANCHOR_H
