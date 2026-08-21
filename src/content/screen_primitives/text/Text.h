#ifndef TEXT_H
#define TEXT_H

#include "content/screen_primitives/ScreenPrimitive.h"
#include "math/kernels.h"

namespace slope {

class Text : public TextualPrimitive
{
public:
    Text() {}
    using TextPtr = std::shared_ptr<Text>;
    // kept for high-volume labels, where a pdflatex round trip is too costly
    [[deprecated("prefer Latex()/Title(); Text has no font control")]]
    static TextPtr Add(const std::string &content);

private:
    std::string content;

    void display(const StateInSlide& sis) const;

    // Primitive interface
public:
    void draw(const TimeObject& t, const StateInSlide &sis) override {
        display(sis);
    }
    void playIntro(const TimeObject& t, const StateInSlide &sis) override;
    void playOutro(const TimeObject& t, const StateInSlide &sis) override;
    Size getSize() const override;
};

}

#endif // TEXT_H
