#ifndef POLYSCOPEPRIMITIVE_H
#define POLYSCOPEPRIMITIVE_H

#include "primitive.h"
#include "StateInSlide.h"
#include "color_tools.h"
#include "Options.h"

namespace slope {
class PolyscopePrimitive;
using PolyscopePrimitivePtr = std::shared_ptr<PolyscopePrimitive>;

class PolyscopePrimitive : public Primitive
{
public:
    PolyscopePrimitive();

    void initPolyscopeData(polyscope::Structure* pcptr);

    std::string getPolyscopeName() const;

    PrimitiveInSlide at(scalar alpha=1);

    // Primitive interface
public:
    void draw(const TimeObject& t, const StateInSlide &sis) override;
    void playIntro(const TimeObject& t,const StateInSlide &sis) override;
    void playOutro(const TimeObject& t,const StateInSlide &sis) override;

    PrimitiveInSlide at(const Transform& T,scalar alpha=1);

    PrimitiveInSlide at(scalar x,scalar y,scalar z,scalar alpha=1);

    PrimitiveInSlide at(const vec& x,scalar alpha=1);

    PrimitiveInSlide at(const std::string& label,scalar alpha = 1);


    void forceDisable() override;

    void forceEnable() override;
    bool isScreenSpace() const override;

    static void resetColorId();

    static glm::vec3 getColor();

    Transform localTransform;
protected:
    polyscope::Structure* polyscope_ptr;

    static size_t count;
    static std::vector<glm::vec3> colors;
    static int current_color_id;

};
template<class T>
class PolyscopeQuantity : public Primitive
{
public :
    using PCQuantityPtr = std::shared_ptr<PolyscopeQuantity>;
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
    void playOutro(const TimeObject& t, const StateInSlide &sis) override {q->setEnabled(false);}
    void forceDisable() override {q->setEnabled(false);}
    bool isScreenSpace() const override {return false;}
};

template<typename T>
static PolyscopeQuantity<T>::PCQuantityPtr AddPolyscopeQuantity(T* ptr) {
    return PolyscopeQuantity<T>::Add(ptr);
}

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
        //std::cout << "ok" << std::endl;
        q->setEnabled(true);
        q->setVectorLengthScale(l0,false);
    }
    void playIntro(const TimeObject& t, const StateInSlide &sis) override {
        q->setVectorLengthScale(l0*smoothstep(t.transitionParameter),false);
        q->setEnabled(true);
    }
    void playOutro(const TimeObject& t, const StateInSlide &sis) override {
        q->setVectorLengthScale(l0*(1-smoothstep(t.transitionParameter)),false);
        if (t.transitionParameter > 0.95)
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
        //std::cout << "ok" << std::endl;
        q->setEnabled(true);
    }
    void playIntro(const TimeObject& t, const StateInSlide &sis) override {
        q->setEnabled(true);
    }
    void playOutro(const TimeObject& t, const StateInSlide &sis) override {
        if (t.transitionParameter > 0.95)
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
