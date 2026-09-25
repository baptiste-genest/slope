#ifndef LEGEND_H
#define LEGEND_H

#include "content/screen_primitives/plots/Board.h"

namespace slope {

class Legend;
using LegendPtr = std::shared_ptr<Legend>;

/*
 * A box in a corner of a board that gives the names of its curves.
 *
 *   show << Board::Add("fig")->at("figure")
 *        << Plot::Add("sine", "fig", f)
 *        << Legend::Add("fig");
 *
 * A board has one legend, named after the board, with settings published as "fig_legend/...".
 * It shows the plots and scatters of the board in the order they were declared.
 * Each one draws its own swatch, followed by its `caption` or else its name.
 *
 * An entry is in the box while the object it names is on the slide, and it appears with that object,
 * so no sequencing has to be written.
 *
 * Settings
 *   fig_legend/corner      corner where it sits,          top_right
 *                            top_left | top_right
 *                            bottom_left | bottom_right
 *   fig_legend/text_size   size of the captions           0.4
 *   fig_legend/swatch      length of a swatch, in pixels  46
 *   fig_legend/padding     space around and between       12
 *   fig_legend/background  color of the box               the slide's, faint
 *   fig_legend/border      color of its edge              the axis color of the board
 *   fig_legend/text        color of the captions
 */
class Legend : public ScreenPrimitive {
public:
    // Builds the legend of a board, given as the board or its name (see BoardRef). The board is looked up when first needed.
    static LegendPtr Add(BoardRef board);

    // Name under which the settings are published, "<board>_legend".
    static std::string nameFor(const std::string& board) { return board + "_legend"; }

    // Names of every setting, in the order of the Tuner.
    static const std::vector<std::string>& settingNames();

    std::string name;
    Settings settings;

    // True, because it goes to a corner of its board and not where a slide would put it.
    bool placesItself() const override { return true; }
    // Size in pixels.
    vec2 getSize() const override;

    void draw(const TimeObject& t, const StateInSlide& sis) override;
    void playIntro(const TimeObject& t, const StateInSlide& sis) override;
    void playOutro(const TimeObject& t, const StateInSlide& sis) override;

private:
    // The board of this legend, looked up on first use.
    BoardPtr owner() const;
    // Draws the box and its entries. `appeared` is the progress of the appearance.
    void paint(const TimeObject& t, const StateInSlide& sis, float appeared);

    std::string board;
    mutable std::weak_ptr<Board> cached;
    // One anchor per row, kept between frames.
    std::vector<AnchorPtr> anchors;
};

} // namespace slope

#endif // LEGEND_H
