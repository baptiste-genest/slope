#ifndef REFERENTIAL_H
#define REFERENTIAL_H

#include "../libslope.h"
#include "Options.h"
#include "io.h"

namespace slope {

class Slide;

/*
struct Position {
    virtual vec2 getPosition() const = 0;
    virtual void writeAtLabel(scalar,scalar,bool) const {}
    virtual bool isPersistent() const { return false; }
};

using PositionPtr = std::shared_ptr<Position>;

struct AbsolutePosition : public Position {
    vec2 p;
    AbsolutePosition(const vec2& p) : p(p) {}
    virtual vec2 getPosition() const override {
        return p;
    }
};

inline PositionPtr MakePosition(const vec2& p) {
    return std::make_shared<AbsolutePosition>(p);
}

class ScreenPrimitive;
using RelativePlacer = std::function<vec2(vec2)>;

struct RelativePosition : public Position {
    PositionPtr ref;
    RelativePlacer placer;

    virtual vec2 getPosition() const override {
        return placer(ref->getPosition());
    }

    RelativePosition(PositionPtr p,RelativePlacer rel) : ref(p),placer(rel) {}
    RelativePosition(PositionPtr p,vec2 offset) : ref(p) {
        placer = [offset](vec2 p) { return p+offset; };
    }
};

struct PersistentPosition : public Position {
    std::string label;

    PersistentPosition(std::string label) : label(label) {}

    void setLabel(std::string label);
    void writeAtLabel(double x,double y,bool overwrite) const override;
    vec2 readFromLabel() const;
    virtual bool isPersistent() const override { return true; }

    virtual vec2 getPosition() const override {
        return readFromLabel();
    }
};
*/


}

#endif // REFERENTIAL_H
