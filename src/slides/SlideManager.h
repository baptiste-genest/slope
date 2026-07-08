#ifndef SLIDEMANAGER_H
#define SLIDEMANAGER_H

#include "Slide.h"
#include "../math/utils.h"
#include "../content/PrimitiveGroup.h"
#include "../content/screen_primitives/Placement.h"
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
    void computeFirstSlideNumbers();

    ScreenPrimitivePtr last_screen_primitive_inserted,centering_root;
    PrimitivePtr last_primitive_inserted;

    Slide center_buffer;int center_start,center_end;
    bool centering = false;AnchorPtr center_anchor;

    std::map<std::string, Primitives> groups;

    // named time marks : a keyframe labels the frame it is declared in, so
    // updaters can branch on t.afterKeyframe("label") instead of counting
    // relative frame numbers (which breaks whenever steps are reordered)
    std::map<std::string, int> keyframes;



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

    int getRelativeSlideNumber(Primitive* p) const;

    Slide& getCurrentSlide();
    Slide& getSlide(index i);

    void addToLastSlide(const PrimitiveInSlide& pis);

    void addToLastSlide(PrimitivePtr ptr,const StateInSlide& sis);


    void removeFromCurrentSlide(PrimitivePtr ptr);

    void removeFromCurrentSlide(const PrimitiveGroup& G);

    // tag-based groups : primitives declare membership, and group operations
    // map over the members present in the current slide. a group has no
    // position of its own : each member's placement stays authoritative
    // (and drag-editable)
    void addToGroup(const std::string& tag, PrimitivePtr ptr);
    bool hasGroup(const std::string& tag) const;
    void removeGroup(const std::string& tag);
    void clearGroups();

    // marks the frame currently being composed
    void markKeyframe(const std::string& name);
    const std::map<std::string, int>& getKeyframes() const {return keyframes;}
    void clearKeyframes() {keyframes.clear();}

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

struct Pause {
    TimeTypeSec duration;
    static Pause Add(TimeTypeSec d) { return {d}; }
};

inline SlideManager& operator<<(SlideManager& SM, const Pause& p) {
    SM.getLastSlide().pause_duration = p.duration;
    return SM;
}

// show << Keyframe("pipeline_done"); labels the frame under composition
struct Keyframe {
    std::string name;
    explicit Keyframe(const std::string& n) : name(n) {}
};

inline SlideManager& operator<<(SlideManager& SM, const Keyframe& k) {
    SM.markKeyframe(k.name);
    return SM;
}


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
    for (auto& [ptr,sis] : G.buffer){
        SM.addToLastSlide(ptr,sis);
    }
    return SM;
}

inline SlideManager& operator<<(SlideManager& SM,CameraViewPtr cam) {
    SM.getLastSlide().camera = cam;
    return SM;
}

inline SlideManager& operator<<(SlideManager& SM,OverrideUpdater update) {
    auto& S = SM.getCurrentSlide();
    auto primitive = Primitive::get(update.pid);
    S[primitive].updaterOverrided = true;
    S[primitive].updaterOverride = update.func;
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
