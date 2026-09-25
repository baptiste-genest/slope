#ifndef TEXT_H
#define TEXT_H

#include "content/screen_primitives/ScreenPrimitive.h"
#include "math/kernels.h"

namespace slope {

// Plain text drawn with ImGui, with no font control.
class Text : public TextualPrimitive {
public:
    Text() {}
    using TextPtr = std::shared_ptr<Text>;
    // Builds a text. It is kept for many small labels, where running pdflatex for each one is too slow.
    [[deprecated("prefer Latex()/Title(); Text has no font control")]]
    static TextPtr Add(const std::string& content);

private:
    std::string content;

    // Draws the text with the state of the slide.
    void display(const StateInSlide& sis) const;

    // Primitive interface
public:
    void draw(const TimeObject& t, const StateInSlide& sis) override {
        display(sis);
    }
    void playIntro(const TimeObject& t, const StateInSlide& sis) override;
    void playOutro(const TimeObject& t, const StateInSlide& sis) override;
    // Size in pixels.
    Size getSize() const override;
};

} // namespace slope

#endif // TEXT_H
