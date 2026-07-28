#ifndef ANCHOR_H
#define ANCHOR_H

#include "../Options.h"
#include "../io.h"
#include <unordered_map>
#include <array>

namespace slope {

class Anchor;
using AnchorPtr = std::shared_ptr<Anchor>;


class Anchor
{
public:
    virtual bool isPersistent() const {return false;}

    virtual void updatePos(const vec2& p) = 0;

    virtual vec2 getPos() const = 0;

    virtual scalar getScale() const {return 1;}
};

class AbsoluteAnchor;
using AbsoluteAnchorPtr = std::shared_ptr<Anchor>;
class AbsoluteAnchor : public Anchor
{
protected:
    vec2 pos;
public:
    AbsoluteAnchor(const vec2& p) {pos = p;}

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

extern AbsoluteAnchorPtr GlobalAnchor;

class LabelAnchor;
using LabelAnchorPtr = std::shared_ptr<LabelAnchor>;
class LabelAnchor : public Anchor
{
protected:
    std::string label = "";

    inline static std::unordered_map<std::string, std::array<double, 3>> session_cache;
    inline static std::set<std::string> dirty_labels;

    // bookkeeping for the startup report, see reportLabelIssues()
    inline static std::map<std::string,int> label_usage;   // how many anchors use each label
    inline static std::set<std::string> created_labels;    // had no .pos file yet
    inline static std::set<std::string> unreadable_labels; // .pos present but unusable

public:

    virtual bool isPersistent() const override { return true; }

    LabelAnchor(std::string l) : label(l) {
        label_usage[label]++;
        writeAtLabel(0.5,0.5,1,false);
    }

    const std::string& getLabel() const { return label; }

    // logs anchors created at their default position (usually a typo in a
    // label name) and .pos files no longer referenced by any anchor
    static void reportLabelIssues();

    static LabelAnchorPtr Add(const std::string& l) {
        return std::make_shared<LabelAnchor>(l);
    }

    virtual vec2 getPos() const override {
        return GlobalAnchor->getPos() + readFromLabel().head(2);
    }

    virtual void updatePos(const vec2& p) override {
        std::cerr << "[WARNING] cannot change labeled anchor position by hand" << std::endl;
    }

    virtual scalar getScale() const override {
        return readFromLabel()(2);
    }

    void writeAtLabel(double x, double y,scalar scale, bool overwrite) const;

    void writeToSession(double x, double y, scalar scale) const;
    // same, without needing an anchor instance : used by the editor's undo,
    // where building a LabelAnchor would register a spurious label use
    static void writeToSessionAt(const std::string& label, double x, double y, scalar scale);
    static void saveAllDirty();
    static bool hasDirty();

    void writePosAtLabel(scalar x, scalar y, bool /*overwrite*/) const {
        writeToSession(x, y, readFromLabel()(2));
    }
    void writeScaleAtLabel(scalar s, bool /*overwrite*/) const {
        vec p = readFromLabel();
        writeToSession(p(0), p(1), s);
    }
    vec readFromLabel() const;
};

vec2 WorldToScreen(const vec& p);

vec ScreenToWorld(const vec2& p);

class DynamicAnchor : public Anchor
{
protected:
    std::function<vec2()> anchor;

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

    DynamicAnchor(std::function<vec2()> f) : anchor(f) {}

    static std::shared_ptr<DynamicAnchor> AddTracker(const std::function<vec()>& f) {
        return std::make_shared<DynamicAnchor>(trackScreen(f));
    }

    static std::shared_ptr<DynamicAnchor> Add(std::function<vec2()> f) {
        return std::make_shared<DynamicAnchor>(f);
    }
    static std::shared_ptr<DynamicAnchor> Add(const vec& x) {
        std::function<vec()> p = [x](){return x;};
        return AddTracker(p);
    }

};
}

#endif // ANCHOR_H
