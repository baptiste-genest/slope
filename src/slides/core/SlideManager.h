#ifndef SLIDEMANAGER_H
#define SLIDEMANAGER_H

#include "slides/core/Slide.h"
#include "math/utils.h"
#include "content/core/PrimitiveGroup.h"
#include "content/screen_primitives/layout/Placement.h"
//#include "slides/ui/Panel.h"


namespace slope {

// Blends two states, giving sa at t=0 and sb at t=1.
StateInSlide transition(parameter t, const StateInSlide &sa, const StateInSlide &sb);

// Half of the size of the bounding box of the screen primitives of a slide.
vec2 computeOffsetToMean(const Slide& buffer);

class SlideManager;
// Function that places a primitive added with no placement.
using PlacementTemplate = std::function<void(SlideManager&,ScreenPrimitivePtr)>;

// Builds the list of slides. Primitives are added to the last slide with operator<<.
class SlideManager {
protected:
    path frame_deck;
    std::vector<int> frame_starts;

    std::vector<Slide> slides;
    bool initialized = false;

    // Primitives that are common to two slides, only in the previous one, and only in the next one.
    using TransitionSets = std::tuple<Primitives,Primitives,Primitives>;

    TransitionSets computeTransitionsBetween(const Slide &A, const Slide &B);

    std::vector<TransitionSets> transitions;

    // Access to the three sets of a transition.
    Primitives& common(TransitionSets& S);
    Primitives& uniquePrevious(TransitionSets& S);
    Primitives& uniqueNext(TransitionSets& S);

    // Primitives that appear at each slide.
    std::vector<Primitives> appearing_primitives;

    // Computes the transitions between every pair of consecutive slides.
    void precomputeTransitions();
    // Gives each primitive the index of the first slide where it appears.
    void computeFirstSlideNumbers();

    ScreenPrimitivePtr last_screen_primitive_inserted,centering_root;
    PrimitivePtr last_primitive_inserted;

    // State of the centering started by beginCenter.
    Slide center_buffer;int center_start,center_end;
    bool centering = false;AnchorPtr center_anchor;

    std::map<std::string, Primitives> groups;

    // A keyframe is a name for the slide where it is declared.
    // Updaters can test t.afterKeyframe("label") and not slide numbers, which change when slides are reordered.
    std::map<std::string, int> keyframes;

public:

    // Places a primitive with no placement at the center of the screen.
    PlacementTemplate templater = [] (SlideManager& show,ScreenPrimitivePtr ptr) {
        show.addToLastSlide(ptr,StateInSlide(vec2(0.5,0.5)));
    };

    // Starts a new empty slide. The background is kept from the previous one.
    void newFrame();

    // Adds a copy of the last slide, so the next primitives are added to a new step.
    void duplicateLastSlide();

    // Adds a slide.
    void addSlide(const Slide& s);

    // Adds several slides.
    template<typename... S>
    void addSlides(const S& ... x) {
        (addSlide(x), ...);
    }

    // Number of slides built so far.
    int getNumberSlides() const;

    // Number of slides between the first appearance of p and the last slide.
    int getRelativeSlideNumber(Primitive* p) const;

    // The last slide, which is the one being built.
    Slide& getCurrentSlide();
    // The slide with index i.
    Slide& getSlide(index i);

    // Adds a primitive to the last slide.
    void addToLastSlide(const PrimitiveInSlide& pis);

    // Same. forced_order sets the rank used to order primitives of the same depth.
    void addToLastSlide(PrimitivePtr ptr,const StateInSlide& sis,int forced_order = -1);


    // Removes a primitive, or every primitive of a group, from the last slide.
    void removeFromCurrentSlide(PrimitivePtr ptr);

    // Same, for every primitive of a group.
    void removeFromCurrentSlide(const PrimitiveGroup& G);

    // Groups. A primitive declares that it belongs to a tag, and group operations apply to the members
    // that are in the current slide. A group has no position of its own.
    // The placement of each member stays as it is and can still be dragged.
    void addToGroup(const std::string& tag, PrimitivePtr ptr);
    // True when a group with this tag exists.
    bool hasGroup(const std::string& tag) const;
    void removeGroup(const std::string& tag);
    void clearGroups();

    // Gives a name to the slide being built.
    void markKeyframe(const std::string& name);
    // Slide index of each keyframe name.
    const std::map<std::string, int>& getKeyframes() const {return keyframes;}
    void clearKeyframes() {keyframes.clear();}

    // Index of the first slide of each frame of the deck file that built this show, and the path of that file.
    void setFrameStarts(const path& deck, std::vector<int> starts) {
        frame_deck = deck;
        frame_starts = std::move(starts);
    }
    const path& getFrameDeck() const {return frame_deck;}
    const std::vector<int>& getFrameStarts() const {return frame_starts;}

    // The last slide, which is the one being built.
    Slide& getLastSlide();

    // The last primitive added, or null after a new slide was started.
    ScreenPrimitivePtr getLastScreenPrimitive();
    PrimitivePtr getLastPrimitive();
    // Tags for operator<<. inNextFrame adds a copy of the last slide.
    struct in_next_frame{};
    // Starts a new slide. With same_title, the title of the previous slide is kept.
    struct new_frame{
        bool same_title = false;
    };

    // Ends the centering, which moves the primitives added since beginCenter so they are centered as a group.
    void handleCenter();

    // Tag that starts or ends the centering, see beginCenter and endCenter.
    struct center_tag{bool open;};
    SlideManager& operator<<(center_tag ct);
};

// Tags to use with operator<<.
constexpr SlideManager::in_next_frame inNextFrame;
constexpr SlideManager::new_frame newFrame{false};
constexpr SlideManager::new_frame newFrameSameTitle{true};
constexpr SlideManager::center_tag beginCenter{true};
constexpr SlideManager::center_tag endCenter{false};

// Sets how long the show waits on the last slide before moving on.
struct Pause {
    TimeTypeSec duration;
    static Pause Add(TimeTypeSec d) { return {d}; }
};

inline SlideManager& operator<<(SlideManager& SM, const Pause& p) {
    SM.getLastSlide().pause_duration = p.duration;
    return SM;
}

// Gives a name to the slide being built, as in show << Keyframe("pipeline_done").
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

inline SlideManager& operator<<(SlideManager& SM,const Background& bg) {
    SM.getLastSlide().background = bg.color;
    return SM;
}

// A cue adjusts a primitive that the slide already has, and adds nothing.
inline SlideManager& operator<<(SlideManager& SM,const SlideCue& cue) {
    cue.apply(SM.getNumberSlides()-1);
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
