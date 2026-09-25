#ifndef SCATTER_H
#define SCATTER_H

#include "content/screen_primitives/plots/Board.h"

namespace slope {

class Scatter;
using ScatterPtr = std::shared_ptr<Scatter>;

/*
 * The measurements drawn as marks on a Board. A Plot draws a line sampled from them instead.
 *
 *   show << Board::Add("conv")->at("figure")
 *        << Scatter::Add("points", "conv", "residuals.csv");
 *
 * Like a Plot, it names its board, goes on its rectangle and appears from left to right.
 * A point outside the range is not drawn, so a moving board crops it.
 * A log axis moves the marks to its decades.
 *
 * Sources
 *   Scatter::Add("s", fig, points);            // vector<vec2>
 *   Scatter::Add("s", fig, "residuals.csv");   // two columns, read again on save
 *   Scatter::Add("s", fig, ys, vec2(0, 10));   // values on a grid
 *
 * Settings
 *   s/color     color of the marks                 from the palette
 *   s/size      radius of a mark, in pixels        5
 *   s/reveal    how much is shown, 0 to 1          1
 *
 * Marks are drawn over the board, so a scatter has no shader of its own.
 */
class Scatter : public ScreenPrimitive, public Legendable {
public:
    // Builds a scatter on a board, given as the board or its name. The source is points, a csv file or values on a grid covering `span`.
    static ScatterPtr Add(const std::string& name, BoardRef board,
                          const std::vector<vec2>& points);
    static ScatterPtr Add(const std::string& name, BoardRef board, const path& csv);
    static ScatterPtr Add(const std::string& name, BoardRef board,
                          const std::vector<scalar>& values, const vec2& span);

    // Names of every setting, in the order of the Tuner.
    static const std::vector<std::string>& settingNames();

    std::string name;
    // Text shown in the legend, used instead of the name when set.
    std::string caption;
    Settings settings;

    // True, because it goes on its board and not where a slide would put it.
    bool placesItself() const override { return true; }
    // Size in pixels.
    vec2 getSize() const override;

    // Legend entry, see Legendable.
    std::string legendCaption() const override { return caption.empty() ? name : caption; }
    bool legendDrawnAt(int frame) const override { return last_frame >= frame - 1; }
    scalar legendAlpha() const override { return appeared; }
    void legendSwatch(ImDrawList* dl, const ImVec2& a, const ImVec2& b,
                      scalar scale, scalar alpha) const override;

    void draw(const TimeObject& t, const StateInSlide& sis) override;
    void playIntro(const TimeObject& t, const StateInSlide& sis) override;
    void playOutro(const TimeObject& t, const StateInSlide& sis) override;

private:
    // Creates the scatter and registers it in the legend of the board.
    static ScatterPtr make(const std::string& name, const BoardRef& board);
    // The board of this scatter, looked up on first use.
    BoardPtr owner() const;
    // Reads the csv file again when it changed.
    void refresh();
    // Draws the marks. `appeared` is the progress of the appearance.
    void paint(const TimeObject& t, const StateInSlide& sis, float appeared);

    std::string board;
    mutable std::weak_ptr<Board> cached;
    std::vector<vec2> points;
    RGBA default_ink;
    // A csv file, read again when it is saved.
    path file;
    std::filesystem::file_time_type stamp{};
    bool read_once = false;
    // Progress of the appearance.
    float appeared = 1;
    // Last frame where it was drawn, read by a legend.
    int last_frame = -1000;
};

} // namespace slope

#endif // SCATTER_H
