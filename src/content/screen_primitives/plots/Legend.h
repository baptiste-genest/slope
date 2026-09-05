#ifndef LEGEND_H
#define LEGEND_H

#include "content/screen_primitives/plots/Board.h"

namespace slope {

class Legend;
using LegendPtr = std::shared_ptr<Legend>;

/*
 * What the curves of a board are called, in a box in one of its corners.
 *
 *   show << Board::Add("fig")->at("figure")
 *        << Plot::Add("sine", "fig", f)
 *        << Legend::Add("fig");
 *
 * A board has one legend, named by that board and publishing "fig_legend/...".
 * It lists the plots and scatters of the board in declaration order, each
 * drawing its own swatch, under its `caption` or else its name.
 *
 * An entry is in the box while what it names is on the slide, and carries that
 * plot's own arrival, so there is no sequencing to write.
 *
 * ── the namespace ─────────────────────────────────────────────────────────
 *   fig_legend/corner      which corner it sits in,       top_right
 *                            top_left | top_right
 *                            bottom_left | bottom_right
 *   fig_legend/text_size   how big the captions are       0.4
 *   fig_legend/swatch      length of a swatch, in pixels  46
 *   fig_legend/padding     the air around and between     12
 *   fig_legend/background  the box it is read against     the slide's, faintly
 *   fig_legend/border      its edge                       the board's axis ink
 *   fig_legend/text        the ink of the captions
 */
class Legend : public ScreenPrimitive {
public:
    // the board, or its name (see BoardRef), looked up when first needed
    static LegendPtr Add(BoardRef board);

    // what it publishes its settings under, "<board>_legend"
    static std::string nameFor(const std::string& board) {return board + "_legend";}

    static const std::vector<std::string>& settingNames();

    std::string name;
    Settings settings;

    // it lands in a corner of its board, not where a slide would put it
    bool placesItself() const override {return true;}
    vec2 getSize() const override;

    void draw(const TimeObject& t, const StateInSlide& sis) override;
    void playIntro(const TimeObject& t, const StateInSlide& sis) override;
    void playOutro(const TimeObject& t, const StateInSlide& sis) override;

private:
    BoardPtr owner() const;
    void paint(const TimeObject& t, const StateInSlide& sis, float appeared);

    std::string board;
    mutable std::weak_ptr<Board> cached;
    std::vector<AnchorPtr> anchors;   // one per row, kept across frames
};

}

#endif // LEGEND_H
