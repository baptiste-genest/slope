#ifndef BOARD_H
#define BOARD_H

#include "content/screen_primitives/gpu/Shader.h"
#include "content/authoring/color_tools.h"
#include "content/screen_primitives/text/LateX.h"
#include <optional>

namespace slope {

class Board;
using BoardPtr = std::shared_ptr<Board>;

/*
 * The settings one object publishes under its own name.
 *
 *   a snippet section of that name    drives it, never a parameter
 *   a value set here                  fixes it, never a parameter
 *   neither                           a Params entry, tunable and saved
 *
 *   fig->settings.set("xticks", "right");     // fixed, gone from the Tuner
 */
struct Settings {
    std::string owner;                     // "fig", so a key is "fig/xrange"
    std::map<std::string, json> stated;    // what a deck or a caller fixed

    // first colour default seen per key, so a live default is not re-applied every frame
    mutable std::map<std::string, RGBA> first_ink;

    std::string full(const std::string& key) const { return owner + "/" + key; }
    void set(const std::string& key, const json& value) { stated[key] = value; }
    bool isSet(const std::string& key) const { return stated.count(key) > 0; }

    scalar      num(const std::string& key, scalar def, scalar lo = 0, scalar hi = 0) const;
    // no ceiling, but nothing below `floor`
    scalar      atLeast(const std::string& key, scalar def, scalar floor = 0) const;
    vec2        rect(const std::string& key, const vec2& def) const;
    RGBA        ink(const std::string& key, const RGBA& def) const;
    bool        flag(const std::string& key, bool def) const;
    std::string choice(const std::string& key, const std::vector<std::string>& options,
                       const std::string& def) const;
};

// two columns of numbers, separated by whatever is not one
std::vector<vec2> readCsvPoints(const path& file);

// A data interval as a log axis draws it, in decades. A bound at or below zero
// is lifted the way a board lifts a range.
vec2 decadeSpan(const vec2& data);

// the next colour of the palette for that board, so two things drawn on one
// never match, and every board opens on the same first colour
RGBA nextInk(const std::string& board);

/*
 * Writes `fallback` to views/<name>.glsl the first time `name` is seen, and
 * returns that file, which the object draws with and reloads from. Empty path
 * when there is no project to write into.
 */
path shaderFileFor(const std::string& name, const std::string& fallback);

/*
 * The frame a plot is drawn in. A background, a data rectangle, a grid, axes
 * and tick labels.
 *
 *   auto fig = Board::Add("fig", vec2(-M_PI, M_PI), vec2(-1.2, 1.2));
 *   show << fig->at("figure");
 *
 * It draws no data and names no axis. A Plot is its own primitive borrowing
 * this one's referential (see Plot.h), and an axis name is a text primitive
 * placed with label() or with "follow: fig.<point>" from a deck.
 *
 * Everything it is made of is published under its name.
 *
 *   fig/xrange      vec2, the data interval across          (-1, 1)
 *   fig/yrange      vec2, the data interval up              (-1, 1)
 *   fig/xscale      linear | log, log being decades         linear
 *   fig/yscale        of the data interval                  linear
 *   fig/xstep       data units between ticks,               0
 *   fig/ystep         0 = round numbers, from the range     0
 *   fig/tick_font   latex or drawn text. Latex costs a      latex
 *                     compile per label, so a range that
 *                     moves every frame wants text
 *   fig/tick_size   how big the tick labels are             0.4
 *   fig/xticks      which edge the labels hang off,         bottom
 *                     bottom | top | none
 *   fig/yticks        left | right | none                   left
 *   fig/show_grid   whether the grid is drawn               true
 *   fig/show_axes   ... the two lines x = 0 and y = 0       true
 *   fig/show_frame  ... the border of the rectangle         true
 *   fig/background  colour of the rectangle                 the slide's own
 *   fig/grid        colour of the grid lines
 *   fig/axis        colour of the axes, frame and labels
 *   fig/line_width  pixels, the frame and axis weight       1.8
 *
 * An animation is a value written to one of those names, from an updater or
 * from a snippet section owning it.
 *
 *   fig->setRange(vec2(0.6, M_PI), vec2(-1.2, 1.2));
 *
 * ── log axes ────────────────────────────────────────────────────────────────
 * An axis set to log is drawn in decades, and everything on the board is drawn
 * through the same transform : a Plot resamples in log x and takes the log of
 * its values, a Scatter places its marks there, label() takes a data point.
 * Ranges, steps and label() stay in data units, so a range is written the way
 * it reads, fig/yrange = (1e-6, 1). A range reaching zero or below is lifted to
 * six decades under its top, and a value there has nothing to draw.
 */
class Board : public Shader {
public:
    // a range given here is fixed, omitted it is a tunable parameter
    static BoardPtr Add(const std::string& name,
                        std::optional<vec2> x = std::nullopt,
                        std::optional<vec2> y = std::nullopt,
                        int w = 1100, int h = 620);

    // null while no board has that name, so a plot may be declared first
    static BoardPtr find(const std::string& name);

    // every setting a board publishes, in Tuner order
    static const std::vector<std::string>& settingNames();

    Settings settings;
    std::string name;

    // the live data rectangle, whoever owns the names
    vec2 xrange() const;
    vec2 yrange() const;
    // which axes are drawn in decades
    bool xlog() const;
    bool ylog() const;
    // a data point in the referential the board is drawn in, log10 on a log
    // axis. Everything drawn on a board goes through it.
    vec2 toView(const vec2& p) const;
    // xrange() and yrange() there, what the shaders are given
    vec2 xview() const;
    vec2 yview() const;
    // call it from an updater to move the view
    void setRange(const vec2& x, const vec2& y);

    // any primitive at a data point (data units), riding the live view
    ScreenPrimitiveInSlide label(const ScreenPrimitivePtr& p, const vec2& at,
                                 const vec2& offset = vec2::Zero());

    // where this board was last drawn, what a Plot borrows to land on it
    StateInSlide frame(const StateInSlide& own) const;

    // tick spacing in the drawn referential, fig/xstep and fig/ystep or round
    // numbers, and always one decade on a log axis
    vec2 step() const;

    void draw(const TimeObject& t, const StateInSlide& sis) override;
    void playIntro(const TimeObject& t, const StateInSlide& sis) override;
    void playOutro(const TimeObject& t, const StateInSlide& sis) override;

private:
    // one edge label. The Latex is made only when it is the font asked for,
    // and shared between every board showing that number.
    struct Tick {
        scalar value;               // where it sits, in the drawn referential
        bool horizontal;
        std::string text;
        LatexPtr tex;
        AnchorPtr anchor;
    };
    std::vector<Tick> tick_labels;
    // re-chosen from the live range and spacing every frame
    void syncTicks();
    // over the blit, just outside the rectangle the board was drawn in
    void drawTicks(const TimeObject& t, const StateInSlide& sis);
};

// What a legend needs of whatever it lists. A Plot and a Scatter both answer
// it, and each draws its own swatch.
struct Legendable {
    virtual ~Legendable() = default;
    virtual std::string legendCaption() const = 0;
    // drawn on this frame or the one before, which is the whole of the rule
    virtual bool legendDrawnAt(int frame) const = 0;
    virtual scalar legendAlpha() const = 0;
    virtual void legendSwatch(ImDrawList* dl, const ImVec2& a, const ImVec2& b,
                              scalar scale, scalar alpha) const = 0;
};

// Plots and scatters join the list of the board they name as they are
// declared, weakly, so a legend has them in the order they were written.
void registerLegendEntry(const std::string& board, const std::weak_ptr<Legendable>& e);
std::vector<std::shared_ptr<Legendable>> legendEntries(const std::string& board);

/*
 * Which board a plot is drawn in, the board itself or its name.
 *
 *   Plot::Add("sine", fig,   f);
 *   Plot::Add("sine", "fig", f);
 *
 * Only the name is kept, so the two forms are the same plot and neither holds
 * the board alive.
 */
struct BoardRef {
    std::string name;
    BoardRef(const char* n) : name(n) {}
    BoardRef(std::string n) : name(std::move(n)) {}
    BoardRef(const BoardPtr& p) : name(p ? p->name : std::string()) {}
};

}

#endif // BOARD_H
