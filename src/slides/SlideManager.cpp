#include "SlideManager.h"

slope::SlideManager::TransitionSets slope::SlideManager::computeTransitionsBetween(const Slide &A, const Slide &B)
{
    PrimitiveSet common,uniqueA,uniqueB;
    for (auto sb : B)
        uniqueB.insert(sb.first);

    for (auto sa : A){
        bool inB = false;
        for (auto sb : B){
            if (sa.first == sb.first){
                common.insert(sa.first);
                uniqueB.erase(sa.first);
                inB = true;
                break;
            }
        }
        if (!inB){
            uniqueA.insert(sa.first);
        }
    }
    //sort by depth, ties broken by insertion order in the relevant slide
    auto depthLess = [](const Slide& s) {
        return [&s](PrimitivePtr a, PrimitivePtr b) {
            if (a->getDepth() != b->getDepth())
                return a->getDepth() < b->getDepth();
            return s.orderOf(a) < s.orderOf(b);
        };
    };
    Primitives common_sorted(common.begin(),common.end());
    std::sort(common_sorted.begin(),common_sorted.end(),depthLess(B));
    Primitives uniqueA_sorted(uniqueA.begin(),uniqueA.end());
    std::sort(uniqueA_sorted.begin(),uniqueA_sorted.end(),depthLess(A));

    Primitives uniqueB_sorted(uniqueB.begin(),uniqueB.end());
    std::sort(uniqueB_sorted.begin(),uniqueB_sorted.end(),depthLess(B));

    return {common_sorted,uniqueA_sorted,uniqueB_sorted};
}

slope::Primitives &slope::SlideManager::common(TransitionSets &S) {return std::get<0>(S);}

slope::Primitives &slope::SlideManager::uniquePrevious(TransitionSets &S) {return std::get<1>(S);}

slope::Primitives &slope::SlideManager::uniqueNext(TransitionSets &S) {return std::get<2>(S);}

void slope::SlideManager::precomputeTransitions(){
    initialized = true;
    appearing_primitives.resize(slides.size());
    for (auto& p : slides[0])
        appearing_primitives[0].push_back(p.first);
    for (int i = 0;i<slides.size()-1;i++){
        transitions.push_back(
            computeTransitionsBetween(
                slides[i],
                slides[i+1]
                ));
        appearing_primitives[i+1] = std::get<2>(transitions.back());
    }
    computeFirstSlideNumbers();
}

void slope::SlideManager::computeFirstSlideNumbers()
{
    for (int i = 0; const auto& v : slides) {
        for (auto& p : v)
            p.first->upFirstSlideNumber(i);
        i++;
    }
}

int slope::SlideManager::getRelativeSlideNumber(Primitive* p) const
{
    return p->relativeSlideIndex(slides.size()-1);
}

void slope::SlideManager::newFrame() {
    handleCenter();
    addSlide(Slide());
    PolyscopePrimitive::resetColorId();
    last_primitive_inserted = nullptr;
    last_screen_primitive_inserted = nullptr;
}

void slope::SlideManager::duplicateLastSlide(){
    slides.push_back(slides.back());
    slides.back().pause_duration = 0;
}

void slope::SlideManager::addSlide(const Slide &s) {
    initialized = false;
    slides.push_back(s);
}

int slope::SlideManager::getNumberSlides() const {
    return slides.size();
}

slope::Slide &slope::SlideManager::getCurrentSlide(){return slides.back();}

slope::Slide &slope::SlideManager::getSlide(index i){return slides[i];}

void slope::SlideManager::addToLastSlide(const PrimitiveInSlide &pis) {
    addToLastSlide(pis.first,pis.second);
}

void slope::SlideManager::addToLastSlide(PrimitivePtr ptr, const StateInSlide &sis) {
    initialized = false;
    if (slides.empty())
        slides.push_back(Slide());
    if (ptr->isScreenSpace()){
        last_screen_primitive_inserted = std::static_pointer_cast<ScreenPrimitive>(ptr);
        last_screen_primitive_inserted->updateAnchor(sis.getPosition());
        if (centering){
            if (center_buffer.empty())
                centering_root = last_screen_primitive_inserted;
            center_buffer.add(ptr,sis);
        }
    }
    last_primitive_inserted = ptr;
    slides.back().add(ptr,sis);
}

void slope::SlideManager::removeFromCurrentSlide(PrimitivePtr ptr) {
    slides.back().remove(ptr);
}

void slope::SlideManager::removeFromCurrentSlide(const PrimitiveGroup &G) {
    for (const auto& p : G.buffer)
        removeFromCurrentSlide(p.first);
}

void slope::SlideManager::addToGroup(const std::string &tag, PrimitivePtr ptr) {
    auto& G = groups[tag];
    if (std::find(G.begin(), G.end(), ptr) == G.end())
        G.push_back(ptr);
}

bool slope::SlideManager::hasGroup(const std::string &tag) const {
    return groups.count(tag);
}

void slope::SlideManager::moveGroup(const std::string &tag, const vec2 &delta) {
    auto it = groups.find(tag);
    if (it == groups.end())
        return;
    auto& S = getLastSlide();
    for (const auto& p : it->second) {
        auto sit = S.find(p);
        if (sit != S.end())
            sit->second.addOffset(delta);
    }
}

void slope::SlideManager::removeGroup(const std::string &tag) {
    auto it = groups.find(tag);
    if (it == groups.end())
        return;
    for (const auto& p : it->second)
        removeFromCurrentSlide(p);
}

void slope::SlideManager::clearGroups() {
    groups.clear();
}

slope::Slide &slope::SlideManager::getLastSlide() {
    if (slides.empty())
        slides.push_back(Slide());
    return slides.back();
}

slope::ScreenPrimitivePtr slope::SlideManager::getLastScreenPrimitive() {
    return last_screen_primitive_inserted;
}

slope::PrimitivePtr slope::SlideManager::getLastPrimitive() {
    return last_primitive_inserted;
}

void slope::SlideManager::handleCenter() {
    if (!centering)
        return;
    centering = false;
    vec2 root_offset =  computeOffsetToMean(center_buffer) - centering_root->getRelativeSize()*0.5;
    for (int i = center_start;i<getNumberSlides();++i)
        slides[i][centering_root].setOffset(-root_offset);
    center_buffer = Slide();
}

slope::SlideManager &slope::SlideManager::operator<<(center_tag ct) {
    if (ct.open){
        centering = true;
        center_start = getNumberSlides()-1;
    } else {
        handleCenter();
    }
    return *this;
}

slope::SlideManager &slope::operator<<(SlideManager &SM, SlideManager::new_frame nf) {
    SM.newFrame();
    if (nf.same_title){
        int i = SM.getNumberSlides()-2;
        auto ex = SM.getSlide(i).title_primitive;
        if (ex != nullptr){
            SM.addToLastSlide(ex,SM.getSlide(i)[ex]);
        }
    }
    return SM;
}

slope::SlideManager &slope::operator<<(SlideManager &SM, const Replace &R) {
    PrimitiveInSlide old;
    if (!R.ptr_other)
        old = {SM.getLastScreenPrimitive(),SM.getLastSlide()[SM.getLastScreenPrimitive()]};
    else
        old = {R.ptr_other,SM.getLastSlide()[R.ptr_other]};
    auto pos = old.second;
    SM.removeFromCurrentSlide(old.first);
    SM.addToLastSlide(R.ptr,pos);
    return SM;
}

slope::SlideManager &slope::operator<<(SlideManager &SM, const RelativePlacement &P) {
    StateInSlide sis;
    if (!P.ptr_other)
        sis = P.computePlacement({SM.getLastScreenPrimitive(),SM.getLastSlide()[SM.getLastScreenPrimitive()]});
    else
        sis = P.computePlacement({P.ptr_other,SM.getLastSlide()[P.ptr_other]});
    SM.addToLastSlide(P.ptr,sis);
    return SM;
}

slope::SlideManager &slope::operator<<(SlideManager &SM, PrimitivePtr ptr) {
    if (!SM.getLastScreenPrimitive() || !ptr->isScreenSpace())
        SM.addToLastSlide(ptr,StateInSlide(vec2(0.5,0.5)));
    else {
        SM.templater(SM,std::static_pointer_cast<ScreenPrimitive>(ptr));
    }
    return SM;
}

slope::vec2 slope::computeOffsetToMean(const Slide &buffer) {
    double x_min = 100,y_min = 100,x_max = -100,y_max = -100;
    for (auto&& [ptr,sis] : buffer) {
        auto pos = sis.getPosition();
        auto size = std::dynamic_pointer_cast<ScreenPrimitive>(ptr)->getRelativeSize();
        x_min = std::min(x_min,pos(0)-size(0)*0.5f);
        y_min = std::min(y_min,pos(1)-size(1)*0.5f);
        x_max = std::max(x_max,pos(0)+size(0)*0.5f);
        y_max = std::max(y_max,pos(1)+size(1)*0.5f);
    }
    vec2 bbox = vec2(x_max-x_min,y_max-y_min);
    return bbox*0.5f;
}

slope::StateInSlide slope::transition(parameter t, const StateInSlide &sa, const StateInSlide &sb){
    StateInSlide St;

    t = smoothstep(t);

    St.setOffset(lerp(sa.getPosition(),sb.getPosition(),t));
    St.alpha = std::lerp(sa.alpha,sb.alpha,t);
    St.angle = std::lerp(sa.angle,sb.angle,t);
    St.LocalToWorld = Transform::Interpolate(sa.getLocalToWorld(),sb.getLocalToWorld(),t);
    St.scale = std::lerp(sa.getScale(),sb.getScale(),t);
    return St;
}
