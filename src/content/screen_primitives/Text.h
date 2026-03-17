#ifndef TEXT_H
#define TEXT_H

#include "ScreenPrimitive.h"
#include "../../math/kernels.h"
#include "../../style/Style.h"
#include "../../style/FontManager.h"

namespace slope {

class Text : public TextualPrimitive
{
public:
    Text() {}
    using TextPtr = std::shared_ptr<Text>;
    static TextPtr Add(const std::string &content,FontID font = -1);

private:
    scalar alpha = 1;
    std::string content;
    FontID fontID;
    FontSize fontSize;

    void display(const StateInSlide& sis) const;
    void pushFont() const;

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
