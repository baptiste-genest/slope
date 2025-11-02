#ifndef VECTORFIELD_H
#define VECTORFIELD_H
#include "PolyscopePrimitive.h"
#include "../math/utils.h"

namespace slope {

class VectorField : public PolyscopePrimitive
{
private:
    vecs V,X;
    scalar length = 1;
    polyscope::PointCloud* pc;
    polyscope::PointCloudVectorQuantity* pq;

    // Primitive interface
public:
    VectorField(const vecs &X, const vecs &V,double l0 = 0.1);
    virtual void draw(const TimeObject &time, const StateInSlide &sis) override {
        pc->setEnabled(true);
        pq->setEnabled(true);
        pq->setVectorLengthScale(length,false);
    }
    virtual void playIntro(const TimeObject &t, const StateInSlide &sis) override {
        pq->setVectorLengthScale(t.transitionParameter*length,false);
    }
    virtual void playOutro(const TimeObject &t, const StateInSlide &sis) override {
        pq->setVectorLengthScale((1-t.transitionParameter)*length,false);
    }
    virtual void forceDisable() override {
        pc->setEnabled(false);
        pq->setEnabled(false);
    }
    virtual void forceEnable() override {
        pc->setEnabled(true);
        pq->setEnabled(true);
    }


    using VectorFieldPtr = std::shared_ptr<VectorField>;
    inline static VectorFieldPtr Add(const vecs& X,const vecs& V,scalar l) {
        return NewPrimitive<VectorField>(X,V,l);
    }
    static VectorFieldPtr AddOnGrid(const vecs& V);
    static VectorFieldPtr EvalOnGrid(const std::function<vec(vec)>& V,int n = 10,scalar l = 1);

    // Primitive interface
public:
    virtual void initPolyscope() override;
};

}

#endif // VECTORFIELD_H
