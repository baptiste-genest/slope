#ifndef SLIDE_H
#define SLIDE_H

#include "content/core/primitive.h"
#include "content/screen_primitives/ScreenPrimitive.h"
#include "content/polyscope_primitives/CameraView.h"
#include "content/authoring/color_tools.h"
#include <optional>

namespace slope {

// Background color of a slide. It is streamed into a slide like a camera and is not a primitive.
struct Background {
    Color color;
    Background(const Color& c) : color(c) {}
    Background(const std::string& name) : color(Color(name)) {}
    Background(const char* name) : color(Color(std::string(name))) {}
    Background(float r, float g, float b, float a = 1) : color(Color(r, g, b, a)) {}
};

// One slide, made of primitives with their state. It also holds the title, the camera and the background.
struct Slide : public std::map<PrimitivePtr, StateInSlide> {

    // Adds a primitive with a state, or at a screen position.
    // A primitive that is already present gets the new state.
    void add(PrimitivePtr p, const StateInSlide& sis = {});
    void add(PrimitivePtr p, const vec2& pos);

    // Same as add(), but forced_order replaces the insertion rank given to a new primitive.
    void add(PrimitivePtr p, const StateInSlide& sis, int forced_order);

    // Primitives in drawing order, by depth and then by insertion, so the first added is behind.
    std::vector<PrimitiveInSlide> getDepthSorted();

    // Rank at which a primitive was added to this slide, or -1 if it is absent.
    int orderOf(const PrimitivePtr& p) const;

    // Adds a primitive together with its state.
    void add(PrimitiveInSlide pis);

    // Removes a primitive.
    void remove(PrimitivePtr ptr);

    // Title of the slide, set by a primitive that is marked exclusive.
    TextualPrimitivePtr title_primitive = nullptr;
    CameraViewPtr camera = nullptr;
    // When unset, the "background" parameter of the deck is used, see Slideshow.
    std::optional<Color> background;

    // The screen primitives of the slide with their state.
    std::map<ScreenPrimitivePtr, StateInSlide> getScreenPrimitives() const;

    // The polyscope primitives of the slide with their state.
    std::map<PolyscopePrimitivePtr, StateInSlide> getPolyscopePrimitives() const;

    // Text of the title, or an empty string.
    std::string getTitle() const;

    // Moves the camera to the view of the slide, smoothly when fly is true. Does nothing when the slide has no camera.
    void setCam(bool fly = true) const;

    // True when both slides use the same camera view, or both have none.
    bool sameCamera(const Slide& other) const;

    // Time in seconds that the show waits on this slide before moving on. 0 means no automatic move.
    TimeTypeSec pause_duration = 0;

private:
    std::map<PrimitivePtr, int> insertion_order;
    int insertion_counter = 0;
};

} // namespace slope

#endif // SLIDE_H
