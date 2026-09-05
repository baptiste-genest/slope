#ifndef SCATTER_H
#define SCATTER_H

#include "content/screen_primitives/plots/Board.h"

namespace slope {

class Scatter;
using ScatterPtr = std::shared_ptr<Scatter>;

/*
 * The measurements themselves, as marks on a Board, where a Plot draws a line
 * resampled from them.
 *
 *   show << Board::Add("conv")->at("figure")
 *        << Scatter::Add("points", "conv", "residuals.csv");
 *
 * Like a Plot it names its board, lands on its rectangle and arrives left to
 * right. A point outside the range is not drawn, so a moving board crops it,
 * and a log axis carries the marks into its decades.
 *
 * ── sources ───────────────────────────────────────────────────────────────
 *   Scatter::Add("s", fig, points);            // vector<vec2>
 *   Scatter::Add("s", fig, "residuals.csv");   // two columns, re-read on save
 *   Scatter::Add("s", fig, ys, vec2(0, 10));   // values on a grid
 *
 * ── the namespace ─────────────────────────────────────────────────────────
 *   s/color     the ink                            from the palette
 *   s/size      radius of a mark, in pixels        5
 *   s/reveal    how much of it is there, 0 to 1    1
 *
 * Marks are drawn over the board, so a scatter carries no shader of its own.
 */
class Scatter : public ScreenPrimitive, public Legendable {
public:
    static ScatterPtr Add(const std::string& name, BoardRef board,
                          const std::vector<vec2>& points);
    static ScatterPtr Add(const std::string& name, BoardRef board, const path& csv);
    static ScatterPtr Add(const std::string& name, BoardRef board,
                          const std::vector<scalar>& values, const vec2& span);

    static const std::vector<std::string>& settingNames();

    std::string name;
    // what a legend calls it, when the name is not what a reader wants
    std::string caption;
    Settings settings;

    // it lands on its board rather than where a slide would put it
    bool placesItself() const override {return true;}
    vec2 getSize() const override;

    // ── a legend entry (see Legendable) ─────────────────────────────────
    std::string legendCaption() const override {return caption.empty() ? name : caption;}
    bool legendDrawnAt(int frame) const override {return last_frame >= frame - 1;}
    scalar legendAlpha() const override {return appeared;}
    void legendSwatch(ImDrawList* dl, const ImVec2& a, const ImVec2& b,
                      scalar scale, scalar alpha) const override;

    void draw(const TimeObject& t, const StateInSlide& sis) override;
    void playIntro(const TimeObject& t, const StateInSlide& sis) override;
    void playOutro(const TimeObject& t, const StateInSlide& sis) override;

private:
    static ScatterPtr make(const std::string& name, const BoardRef& board);
    BoardPtr owner() const;
    void refresh();
    void paint(const TimeObject& t, const StateInSlide& sis, float appeared);

    std::string board;
    mutable std::weak_ptr<Board> cached;
    std::vector<vec2> points;
    RGBA default_ink;
    path file;                      // a csv, re-read when it is saved
    std::filesystem::file_time_type stamp{};
    bool read_once = false;
    float appeared = 1;      // how far its own arrival has got
    int last_frame = -1000;  // when it was last drawn, read by a legend
};

}

#endif // SCATTER_H
