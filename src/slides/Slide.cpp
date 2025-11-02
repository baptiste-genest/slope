#include "Slide.h"
#include "../content/Referential.h"

void slope::Slide::add(PrimitivePtr p, const StateInSlide &sis){
    if (p->isScreenSpace())
        if (p->isExclusive()){
            if (title_primitive != nullptr)
                this->erase(title_primitive);
            title_primitive = std::static_pointer_cast<TextualPrimitive>(p);
        }
    (*this)[p] = sis;
}

void slope::Slide::add(PrimitivePtr p, const vec2 &pos){
    add(p,StateInSlide(pos));
}

std::vector<slope::PrimitiveInSlide> slope::Slide::getDepthSorted() {
    std::vector<PrimitiveInSlide> rslt;
    for (auto&& [ptr,sis] : *this)
        rslt.push_back({ptr,sis});
    std::sort(rslt.begin(),rslt.end(),[](PrimitiveInSlide a,PrimitiveInSlide b){
        return a.first->getDepth() < b.first->getDepth();
    });
    return rslt;
}

void slope::Slide::add(PrimitiveInSlide pis){
    add(pis.first,pis.second);
}

void slope::Slide::remove(PrimitivePtr ptr) {
    this->erase(ptr);
}

std::map<slope::ScreenPrimitivePtr, slope::StateInSlide> slope::Slide::getScreenPrimitives() const {
    std::map<ScreenPrimitivePtr,StateInSlide> rslt;
    for (auto&& [ptr,sis] : *this) {
        if (!ptr->isScreenSpace())
            continue;
        rslt[std::dynamic_pointer_cast<ScreenPrimitive>(ptr)] = sis;
    }
    return rslt;
}

std::map<slope::PolyscopePrimitivePtr, slope::StateInSlide> slope::Slide::getPolyscopePrimitives() const {
    std::map<PolyscopePrimitivePtr,StateInSlide> rslt;
    for (auto&& [ptr,sis] : *this) {
        if (ptr->isScreenSpace())
            continue;
        rslt[std::dynamic_pointer_cast<PolyscopePrimitive>(ptr)] = sis;
    }
    return rslt;
}

std::string slope::Slide::getTitle() const {
    if (title_primitive == nullptr)
        return "";
    return title_primitive->content;
}

void slope::Slide::setCam() const {
    if (camera)
        camera->enable();
    /*
        else
            polyscope::view::resetCameraToHomeView();
        */
}

bool slope::Slide::sameCamera(const Slide &other) const {
    if (camera && !other.camera)
        return false;
    if (!camera && other.camera)
        return false;
    if (!camera && !other.camera)
        return true;
    return camera == other.camera;
}
