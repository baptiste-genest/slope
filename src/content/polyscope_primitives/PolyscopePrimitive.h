#ifndef POLYSCOPEPRIMITIVE_H
#define POLYSCOPEPRIMITIVE_H

#include "content/core/primitive.h"
#include "content/core/StateInSlide.h"
#include "content/authoring/color_tools.h"
#include "content/config/Options.h"

namespace slope {
class PolyscopePrimitive;
using PolyscopePrimitivePtr = std::shared_ptr<PolyscopePrimitive>;

// Base of the primitives that wrap a polyscope structure.
class PolyscopePrimitive : public Primitive
{
public:
    PolyscopePrimitive();

    // Registers the polyscope structure and gives it the next palette color.
    // With palette false, the color chosen by polyscope is kept.
    void initPolyscopeData(polyscope::Structure* pcptr, bool palette = true);

    // Name of the structure in polyscope.
    std::string getPolyscopeName() const;

    // Places the primitive at the origin of the scene, with opacity alpha.
    PrimitiveInSlide at(scalar alpha=1);

    // Primitive interface
public:
    void draw(const TimeObject& t, const StateInSlide &sis) override;
    void playIntro(const TimeObject& t,const StateInSlide &sis) override;
    void playOutro(const TimeObject& t,const StateInSlide &sis) override;

    // Places the primitive with a transform.
    PrimitiveInSlide at(const Transform& T,scalar alpha=1);

    // Places the primitive at a position.
    PrimitiveInSlide at(scalar x,scalar y,scalar z,scalar alpha=1);

    PrimitiveInSlide at(const vec& x,scalar alpha=1);

    // Places the primitive with the transform stored under a label, which a gizmo can edit.
    PrimitiveInSlide at(const std::string& label,scalar alpha = 1);

    // Places the primitive with a transform whose fields are values or snippet names read every frame.
    PrimitiveInSlide at(const LiveTransform& T,scalar alpha = 1);
    // Same, inside the frame of the label, which the gizmo can still move.
    PrimitiveInSlide at(const std::string& label,const LiveTransform& T,scalar alpha = 1);

    void forceDisable() override;

    void forceEnable() override;
    bool isScreenSpace() const override;

    // Removes the structure from polyscope, for a primitive dropped by the deck.
    void unregisterFromPolyscope();

    // Sets the color, either Color(r,g,b) or Color("name"), which can be tuned or given by a snippet.
    void setColor(const Color& c);
    const Color& getColor() const {return color;}
    // Color given when the structure was registered.
    glm::vec3 getDefaultColor() const {return default_color;}
    // Goes back to the default color.
    void resetColor();

    // Restarts the palette from its first color.
    static void resetColorId();

    // Next color of the palette. New structures take them in order of creation.
    static glm::vec3 nextPaletteColor();

    // Applies the transform of the slide state to the structure.
    void setTransform(const StateInSlide& sis);

    // Number of vertices, points or curve nodes. It is 0 for a structure without any.
    virtual size_t vertexCount() const { return 0; }
    // Position of vertex i as drawn now, including the placement of the slide.
    vec worldVertex(size_t i) const;

    // Transform of the structure before the placement of the slide.
    Transform localTransform;

    bool isPolyscopePrimitive() const override { return true; }
protected:
    polyscope::Structure* polyscope_ptr = nullptr;

    // Applies the color again, for a structure registered again without initPolyscopeData.
    void reapplyColor();

    // Position of vertex i in the coordinates of the structure, with i < vertexCount().
    virtual vec localVertex(size_t i) const { return vec::Zero(); }

    static size_t count;
    static std::vector<glm::vec3> colors;
    static int current_color_id;

private:
    Color color;
    glm::vec3 default_color = glm::vec3(1);
    bool colored = false;
    std::optional<glm::vec3> applied;

    // Sends the current color to polyscope.
    void syncColor();
};
// Shows a polyscope quantity, such as a scalar field, during its slides.
template<class T>
class PolyscopeQuantity : public Primitive
{
public :
    using PCQuantityPtr = std::shared_ptr<PolyscopeQuantity>;
    // Wraps the quantity and hides it until its slide.
    static PCQuantityPtr Add(T* ptr) {
        auto rslt = NewPrimitive<PolyscopeQuantity<T>>();
        rslt->q = ptr;
        rslt->q->setEnabled(false);
        return rslt;
    }

    // Primitive interface
public:
    T* q;
    void draw(const TimeObject &time, const StateInSlide &sis) override {q->setEnabled(true);}
    void playIntro(const TimeObject& t, const StateInSlide &sis) override {q->setEnabled(true);}
    // A quantity cannot fade, so it stays visible until the end of the outro.
    void playOutro(const TimeObject& t, const StateInSlide &sis) override {
        if (t.transition_parameter > 0.95)
            q->setEnabled(false);
    }
    void forceDisable() override {q->setEnabled(false);}
    bool isScreenSpace() const override {return false;}
};

// Wraps a polyscope quantity in a primitive.
template<typename T>
static PolyscopeQuantity<T>::PCQuantityPtr AddPolyscopeQuantity(T* ptr) {
    return PolyscopeQuantity<T>::Add(ptr);
}

// Vector fields grow from zero length during the intro and shrink during the outro.
template<>
class PolyscopeQuantity<polyscope::SurfaceVertexVectorQuantity> : public Primitive
{
public :
    using T = polyscope::SurfaceVertexVectorQuantity;
    using PCQuantityPtr = std::shared_ptr<PolyscopeQuantity<T>>;
    static PCQuantityPtr Add(T* ptr) {
        auto rslt = NewPrimitive<PolyscopeQuantity<T>>();
        rslt->q = ptr;
        rslt->q->setEnabled(false);
        rslt->l0 = ptr->getVectorLengthScale();
        return rslt;
    }

    // Primitive interface
public:
    scalar l0;
    T* q;
    void draw(const TimeObject &time, const StateInSlide &sis) override {
        q->setEnabled(true);
        q->setVectorLengthScale(l0,false);
    }
    void playIntro(const TimeObject& t, const StateInSlide &sis) override {
        q->setVectorLengthScale(l0*t.transition_parameter,false);
        q->setEnabled(true);
    }
    void playOutro(const TimeObject& t, const StateInSlide &sis) override {
        q->setVectorLengthScale(l0*(1-t.transition_parameter),false);
        if (t.transition_parameter > 0.95)
            q->setEnabled(false);
    }
    void forceDisable() override {q->setEnabled(false);}
    bool isScreenSpace() const override {return false;}
};



template<>
class PolyscopeQuantity<polyscope::SurfaceVertexParameterizationQuantity> : public Primitive
{
public :
    using T = polyscope::SurfaceVertexParameterizationQuantity;
    using PCQuantityPtr = std::shared_ptr<PolyscopeQuantity<T>>;
    static PCQuantityPtr Add(T* ptr) {
        auto rslt = NewPrimitive<PolyscopeQuantity<T>>();
        rslt->q = ptr;
        rslt->q->setEnabled(false);
        return rslt;
    }

    // Primitive interface
public:
    scalar l0;
    T* q;
    void draw(const TimeObject &time, const StateInSlide &sis) override {
        q->setEnabled(true);
    }
    void playIntro(const TimeObject& t, const StateInSlide &sis) override {
        q->setEnabled(true);
    }
    void playOutro(const TimeObject& t, const StateInSlide &sis) override {
        if (t.transition_parameter > 0.95)
            q->setEnabled(false);
    }
    void forceDisable() override {q->setEnabled(false);}
    bool isScreenSpace() const override {return false;}
};

template<>
PolyscopeQuantity<polyscope::SurfaceVertexVectorQuantity>::PCQuantityPtr AddPolyscopeQuantity(polyscope::SurfaceVertexVectorQuantity* ptr) {
    return PolyscopeQuantity<polyscope::SurfaceVertexVectorQuantity>::Add(ptr);
}




}

#endif // POLYSCOPEPRIMITIVE_H
