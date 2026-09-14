#include "content/polyscope_primitives/PolyscopePrimitive.h"

size_t slope::PolyscopePrimitive::count = 0;
std::vector<glm::vec3>  slope::PolyscopePrimitive::colors = {};
int slope::PolyscopePrimitive::current_color_id = 0;

#include "polyscope/color_management.h"
#include "polyscope/volume_grid.h"

namespace {

glm::vec3 structureColor(polyscope::Structure* s)
{
    if (auto* m = dynamic_cast<polyscope::SurfaceMesh*>(s))  return m->getSurfaceColor();
    if (auto* p = dynamic_cast<polyscope::PointCloud*>(s))   return p->getPointColor();
    if (auto* c = dynamic_cast<polyscope::CurveNetwork*>(s)) return c->getColor();
    if (auto* g = dynamic_cast<polyscope::VolumeGrid*>(s))   return g->getColor();
    return glm::vec3(1);
}

void pushColor(polyscope::Structure* s, const glm::vec3& c)
{
    if (auto* m = dynamic_cast<polyscope::SurfaceMesh*>(s))       m->setSurfaceColor(c);
    else if (auto* p = dynamic_cast<polyscope::PointCloud*>(s))   p->setPointColor(c);
    else if (auto* n = dynamic_cast<polyscope::CurveNetwork*>(s)) n->setColor(c);
    else if (auto* g = dynamic_cast<polyscope::VolumeGrid*>(s))   g->setColor(c);
}

}

slope::PolyscopePrimitive::PolyscopePrimitive() {}

void slope::PolyscopePrimitive::initPolyscopeData(polyscope::Structure *pcptr, bool palette) {
    polyscope_ptr = pcptr;
    polyscope_ptr->setEnabled(false);
    count++;
    // registered again on a rebuild, which keeps the first colour
    if (!colored) {
        default_color = palette ? nextPaletteColor() : structureColor(pcptr);
        color = Color(ColorType(default_color, 1));
        colored = true;
    }
    reapplyColor();
}

void slope::PolyscopePrimitive::setColor(const Color &c) {
    color = c;
    reapplyColor();
}

void slope::PolyscopePrimitive::resetColor() {
    setColor(Color(ColorType(default_color, 1)));
}

void slope::PolyscopePrimitive::reapplyColor() {
    applied.reset();
    syncColor();
}

void slope::PolyscopePrimitive::syncColor() {
    if (!polyscope_ptr)
        return;
    const glm::vec3 c(color.getValue());
    if (applied && *applied == c)
        return;
    pushColor(polyscope_ptr, c);
    applied = c;
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
    syncColor();
    polyscope_ptr->setTransparency(sis.getAlpha());
    polyscope_ptr->setTransform(sis.getLocalToWorld().getMatrix()*localTransform.getMatrix());
}

void slope::PolyscopePrimitive::playIntro(const TimeObject &t, const StateInSlide &sis) {
    syncColor();
    polyscope_ptr->setTransparency(sis.getAlpha());
    polyscope_ptr->setTransform(sis.getLocalToWorld().getMatrix()*localTransform.getMatrix());
}

void slope::PolyscopePrimitive::playOutro(const TimeObject &t, const StateInSlide &sis) {
    syncColor();
    polyscope_ptr->setTransparency(sis.getAlpha());
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

glm::vec3 slope::PolyscopePrimitive::nextPaletteColor() {
    if (colors.size() == 0) {
        for (int i = 0;i<10;i++)
            colors.push_back(polyscope::getNextUniqueColor());
        current_color_id = 0;
    }
    return colors[(current_color_id++)%colors.size()];
}

void slope::PolyscopePrimitive::setTransform(const StateInSlide &sis)
{
    polyscope_ptr->setTransform(sis.getLocalToWorld().getMatrix()*localTransform.getMatrix());
}
