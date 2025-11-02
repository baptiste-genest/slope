#ifndef SLIDEMANAGER_H
#define SLIDEMANAGER_H

#include "Slide.h"
#include "../math/utils.h"
#include "../content/PrimitiveGroup.h"
#include "../content/Placement.h"
#include "../content/PrimitiveGroup.h"
//#include "Panel.h"


namespace slope {

StateInSlide transition(parameter t, const StateInSlide &sa, const StateInSlide &sb);

vec2 computeOffsetToMean(const Slide& buffer);

class SlideManager;
using PlacementTemplate = std::function<void(SlideManager&,ScreenPrimitivePtr)>;

class SlideManager {
protected:

    std::vector<Slide> slides;
    bool initialized = false;

    using TransitionSets = std::tuple<Primitives,Primitives,Primitives>;

    TransitionSets computeTransitionsBetween(const Slide &A, const Slide &B);

    std::vector<TransitionSets> transitions;

    Primitives& common(TransitionSets& S);
    Primitives& uniquePrevious(TransitionSets& S);
    Primitives& uniqueNext(TransitionSets& S);

    std::vector<Primitives> appearing_primitives;

    void precomputeTransitions();

    ScreenPrimitivePtr last_screen_primitive_inserted,centering_root;
    PrimitivePtr last_primitive_inserted;

    Slide center_buffer;int center_start,center_end;
    bool centering = false;AnchorPtr center_anchor;


public:

    PlacementTemplate templater = [] (SlideManager& show,ScreenPrimitivePtr ptr) {
        show.addToLastSlide(ptr,StateInSlide(vec2(0.5,0.5)));
    };

    void newFrame();

    void duplicateLastSlide();

    void addSlide(const Slide& s);

    template<typename... S>
    void addSlides(const S& ... x) {
        (addSlide(x), ...);
    }

    int getNumberSlides() const;

    Slide& getCurrentSlide();
    Slide& getSlide(index i);

    void addToLastSlide(const PrimitiveInSlide& pis);

    void addToLastSlide(PrimitivePtr ptr,const StateInSlide& sis);


    void removeFromCurrentSlide(PrimitivePtr ptr);

    void removeFromCurrentSlide(const PrimitiveGroup& G);

    Slide& getLastSlide();

    ScreenPrimitivePtr getLastScreenPrimitive();
    PrimitivePtr getLastPrimitive();
    struct remove_last{};
    struct in_next_frame{};
    struct new_frame{
        bool same_title = false;
    };

    void handleCenter();

    struct center_tag{bool open;};
    SlideManager& operator<<(center_tag ct);
};

constexpr SlideManager::remove_last removeLast;
constexpr SlideManager::in_next_frame inNextFrame;
constexpr SlideManager::new_frame newFrame{false};
constexpr SlideManager::new_frame newFrameSameTitle{true};
constexpr SlideManager::center_tag beginCenter{true};
constexpr SlideManager::center_tag endCenter{false};


inline SlideManager& operator<<(SlideManager& SM,const Slide& S) {
    SM.addSlide(S);
    return SM;
}


inline SlideManager& operator<<(SlideManager& SM,SlideManager::in_next_frame) {
    SM.duplicateLastSlide();
    return SM;
}

inline SlideManager& operator<<(SlideManager& SM,SlideManager::remove_last) {
    SM.removeFromCurrentSlide(SM.getLastPrimitive());
    return SM;
}


SlideManager& operator<<(SlideManager& SM,SlideManager::new_frame nf);

SlideManager& operator<<(SlideManager& SM,const Replace& R);

SlideManager& operator<<(SlideManager& SM,const RelativePlacement& P);


inline SlideManager& operator<<(SlideManager& SM,PrimitiveInSlide obj) {
    SM.addToLastSlide(obj.first,obj.second);
    return SM;
}

SlideManager& operator<<(SlideManager& SM,PrimitivePtr ptr);

inline SlideManager& operator<<(SlideManager& SM,const StateInSlide& sis) {
    SM.getLastSlide()[SM.getLastScreenPrimitive()] = sis;
    return SM;
}

inline SlideManager& operator<<(SlideManager& SM,const PrimitiveGroup& G) {
    for (auto [ptr,sis] : G.buffer){
        SM.addToLastSlide(ptr,sis);
    }
    return SM;
}

inline SlideManager& operator<<(SlideManager& SM,CameraViewPtr cam) {
    SM.getLastSlide().camera = cam;
    return SM;
}



inline SlideManager& operator>>(SlideManager& SM,PrimitivePtr ptr) {
    SM.removeFromCurrentSlide(ptr);
    return SM;
}

inline SlideManager& operator>>(SlideManager& SM,const PrimitiveGroup& G) {
    SM.removeFromCurrentSlide(G);
    return SM;
}

}

#endif // SLIDEMANAGER_H
