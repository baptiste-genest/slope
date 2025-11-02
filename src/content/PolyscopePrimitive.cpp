#include "PolyscopePrimitive.h"

size_t slope::PolyscopePrimitive::count = 0;
std::vector<glm::vec3>  slope::PolyscopePrimitive::colors = {};
int slope::PolyscopePrimitive::current_color_id = 0;

#include "polyscope/color_management.h"

slope::PolyscopePrimitive::PolyscopePrimitive() {}

void slope::PolyscopePrimitive::initPolyscopeData(polyscope::Structure *pcptr) {
    polyscope_ptr = pcptr;
    polyscope_ptr->setEnabled(false);
    count++;
}

std::string slope::PolyscopePrimitive::getPolyscopeName() const {
    return "polyscope_obj" + std::to_string(pid);
}

slope::PrimitiveInSlide slope::PolyscopePrimitive::at(scalar alpha) {
    StateInSlide sis;
    sis.alpha = alpha;
    return {get(pid),sis};
}

void slope::PolyscopePrimitive::draw(const TimeObject &t, const StateInSlide &sis) {
    polyscope_ptr->setTransparency(sis.alpha);
    polyscope_ptr->setTransform(sis.getLocalToWorld().getMatrix()*localTransform.getMatrix());
}

void slope::PolyscopePrimitive::playIntro(const TimeObject &t, const StateInSlide &sis) {
    polyscope_ptr->setTransparency(sis.alpha);
    polyscope_ptr->setTransform(sis.getLocalToWorld().getMatrix()*localTransform.getMatrix());
}

void slope::PolyscopePrimitive::playOutro(const TimeObject &t, const StateInSlide &sis) {
    polyscope_ptr->setTransparency(sis.alpha);
    polyscope_ptr->setTransform(sis.getLocalToWorld().getMatrix()*localTransform.getMatrix());
}

slope::PrimitiveInSlide slope::PolyscopePrimitive::at(const Transform &T, scalar alpha) {
    StateInSlide sis(T);
    sis.alpha = alpha;
    return {get(pid),sis};
}

slope::PrimitiveInSlide slope::PolyscopePrimitive::at(scalar x, scalar y, scalar z, scalar alpha) {
    return at(vec(x,y,z),alpha);
}

slope::PrimitiveInSlide slope::PolyscopePrimitive::at(const vec &x, scalar alpha) {
    return at(Transform::Translation(x),alpha);
}

slope::PrimitiveInSlide slope::PolyscopePrimitive::at(const std::string &label, scalar alpha){
    StateInSlide sis;
    sis.alpha = alpha;
    sis.persistentTransform = PersistentTransform(label);
    return {get(pid),sis};
}

void slope::PolyscopePrimitive::forceDisable() {
    polyscope_ptr->setEnabled(false);
}

void slope::PolyscopePrimitive::forceEnable() {
    polyscope_ptr->setEnabled(true);
    polyscope_ptr->setTransparency(0);
}

bool slope::PolyscopePrimitive::isScreenSpace() const {return false;}

void slope::PolyscopePrimitive::resetColorId() {current_color_id = 0;}

glm::vec3 slope::PolyscopePrimitive::getColor() {
    if (colors.size() == 0) {
        for (int i = 0;i<10;i++)
            colors.push_back(polyscope::getNextUniqueColor());
        current_color_id = 0;
    }
    return colors[(current_color_id++)%colors.size()];
}
