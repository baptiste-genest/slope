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
 * The settings that one object publishes under its own name.
 * Each setting gets its value from the first of these that exists.
 *
 *   a snippet section of that name    drives it, and it is not a parameter
 *   a value set here                  fixes it, and it is not a parameter
 *   neither                           a Params entry, which can be tuned and saved
 *
 *   fig->settings.set("xticks", "right");     // fixed, removed from the Tuner
 */
struct Settings {
    // Name of the object, for example "fig", so a key is "fig/xrange".
    std::string owner;
    // Values fixed by a deck or by a caller.
    std::map<std::string, json> stated;

    // First default color seen for each key, so a live default is not applied again at every frame.
    mutable std::map<std::string, RGBA> first_ink;

    // Full name of a key, with the owner.
    std::string full(const std::string& key) const { return owner + "/" + key; }
    // Fixes a value.
    void set(const std::string& key, const json& value) { stated[key] = value; }
    // True when a value was fixed.
    bool isSet(const std::string& key) const { return stated.count(key) > 0; }

    // Current value of a setting. A number between lo and hi, where lo equal to hi means no bounds.
    scalar      num(const std::string& key, scalar def, scalar lo = 0, scalar hi = 0) const;
    // A number with no upper bound and a lower bound `floor`.
    scalar      atLeast(const std::string& key, scalar def, scalar floor = 0) const;
    // A pair of numbers, such as a range.
    vec2        rect(const std::string& key, const vec2& def) const;
    // A color.
    RGBA        ink(const std::string& key, const RGBA& def) const;
    // A boolean.
    bool        flag(const std::string& key, bool def) const;
    // One name among a list of options.
    std::string choice(const std::string& key, const std::vector<std::string>& options,
                       const std::string& def) const;
};

// Reads points from a file with two columns of numbers. Anything that is not part of a number separates them.
std::vector<vec2> readCsvPoints(const path& file);

// A data interval as a log axis draws it, in decades.
// A bound at or below zero is raised in the same way as a board raises a range.
vec2 decadeSpan(const vec2& data);

// Next color of the palette for a board. Two objects drawn on one board never have the same color,
// and every board starts with the same first color.
RGBA nextInk(const std::string& board);

// Writes `fallback` to views/<name>.glsl the first time `name` is seen, and returns that file.
// The object draws with this file and reloads it. The path is empty when there is no project to write into.

path shaderFileFor(const std::string& name, const std::string& fallback);

/*
 * The frame in which a plot is drawn. It has a background, a data rectangle, a grid, axes
 * and tick labels.
 *
 *   auto fig = Board::Add("fig", vec2(-M_PI, M_PI), vec2(-1.2, 1.2));
 *   show << fig->at("figure");
 *
 * It draws no data and gives no name to the axes. A Plot is a primitive of its own that uses the
 * coordinates of the board (see Plot.h). The name of an axis is a text primitive
 * placed with label(), or with "follow: fig.<point>" in a deck.
 *
 * All its settings are published under its name.
 *
 *   fig/xrange      vec2, the data interval across          (-1, 1)
 *   fig/yrange      vec2, the data interval up              (-1, 1)
 *   fig/xscale      linear | log, log being decades         linear
 *   fig/yscale        of the data interval                  linear
 *   fig/xstep       data units between ticks,               0
 *   fig/ystep         0 = round numbers, from the range     0
 *   fig/tick_font   latex or drawn text. Latex needs a      latex
 *                     compile per label, so use text when
 *                     the range moves at every frame
 *   fig/tick_size   size of the tick labels                 0.4
 *   fig/xticks      edge where the labels are put,          bottom
 *                     bottom | top | none
 *   fig/yticks        left | right | none                   left
 *   fig/show_grid   whether the grid is drawn               true
 *   fig/show_axes   ... the two lines x = 0 and y = 0       true
 *   fig/show_frame  ... the border of the rectangle         true
 *   fig/background  color of the rectangle                  the slide's own
 *   fig/grid        color of the grid lines
 *   fig/axis        color of the axes, frame and labels
 *   fig/line_width  pixels, width of the frame and axes     1.8
 *
 * An animation is a value written to one of these names, from an updater
 * or from a snippet section that owns the name.
 *
 *   fig->setRange(vec2(0.6, M_PI), vec2(-1.2, 1.2));
 *
 * Log axes
 * An axis set to log is drawn in decades, and everything on the board goes through the same transform.
 * A Plot samples in log x and takes the log of its values, a Scatter places its marks there,
 * and label() takes a data point.
 * Ranges, steps and label() stay in data units, so a range is written as it reads, fig/yrange = (1e-6, 1).
 * A range that reaches zero or below is raised to six decades under its top.
 * A value there has nothing to draw.
 */
class Board : public Shader {
public:
    // Builds a board. A range given here is fixed, and an omitted one is a tunable parameter.
    // The size w by h is the size in pixels of the rendering.
    static BoardPtr Add(const std::string& name,
                        std::optional<vec2> x = std::nullopt,
                        std::optional<vec2> y = std::nullopt,
                        int w = 1100, int h = 620);

    // Returns the board with this name, or null while none has it, so a plot can be declared first.
    static BoardPtr find(const std::string& name);

    // Names of every setting that a board publishes, in the order of the Tuner.
    static const std::vector<std::string>& settingNames();

    Settings settings;
    std::string name;

    // Current data range, whoever owns the setting.
    vec2 xrange() const;
    vec2 yrange() const;
    // True when the axis is drawn in decades.
    bool xlog() const;
    bool ylog() const;
    // Converts a data point to the coordinates where the board is drawn, using log10 on a log axis.
    // Everything drawn on a board goes through it.
    vec2 toView(const vec2& p) const;
    // The ranges in those coordinates, which is what the shaders receive.
    vec2 xview() const;
    vec2 yview() const;
    // Sets both ranges. Call it from an updater to move the view.
    void setRange(const vec2& x, const vec2& y);

    // Places a primitive at a data point, in data units. It follows the view when the range changes.
    ScreenPrimitiveInSlide label(const ScreenPrimitivePtr& p, const vec2& at,
                                 const vec2& offset = vec2::Zero());

    // State with the place where this board was last drawn. A Plot uses it to land on the board.
    StateInSlide frame(const StateInSlide& own) const;

    // Spacing of the ticks in the drawn coordinates. It is fig/xstep and fig/ystep or round numbers,
    // and always one decade on a log axis.
    vec2 step() const;

    void draw(const TimeObject& t, const StateInSlide& sis) override;
    void playIntro(const TimeObject& t, const StateInSlide& sis) override;
    void playOutro(const TimeObject& t, const StateInSlide& sis) override;

private:
    // One label on an edge. The Latex object is made only when the latex font is chosen,
    // and it is shared by every board that shows the same number.
    struct Tick {
        // Position in the drawn coordinates.
        scalar value;
        bool horizontal;
        std::string text;
        LatexPtr tex;
        AnchorPtr anchor;
    };
    std::vector<Tick> tick_labels;
    // Chooses the ticks again from the current range and spacing. Called at every frame.
    void syncTicks();
    // Draws the labels over the rendered board, just outside its rectangle.
    void drawTicks(const TimeObject& t, const StateInSlide& sis);
};

// What a legend needs from each object it shows. A Plot and a Scatter both implement it,
// and each draws its own swatch.
struct Legendable {
    virtual ~Legendable() = default;
    // Text shown in the legend.
    virtual std::string legendCaption() const = 0;
    // True when the object was drawn on this frame or the previous one.
    virtual bool legendDrawnAt(int frame) const = 0;
    // Opacity of the entry, following the appearance of the object.
    virtual scalar legendAlpha() const = 0;
    // Draws the sample of the object in the box from a to b.
    virtual void legendSwatch(ImDrawList* dl, const ImVec2& a, const ImVec2& b,
                              scalar scale, scalar alpha) const = 0;
};

// Adds an entry to the list of a board. Plots and scatters do it when they are declared,
// with a weak pointer, so a legend shows them in the order they were written.
void registerLegendEntry(const std::string& board, const std::weak_ptr<Legendable>& e);
// Entries of a board that still exist.
std::vector<std::shared_ptr<Legendable>> legendEntries(const std::string& board);

/*
 * The board where a plot is drawn, given as the board itself or as its name.
 *
 *   Plot::Add("sine", fig,   f);
 *   Plot::Add("sine", "fig", f);
 *
 * Only the name is kept, so the two forms give the same plot and neither keeps the board alive.
 */
struct BoardRef {
    std::string name;
    BoardRef(const char* n) : name(n) {}
    BoardRef(std::string n) : name(std::move(n)) {}
    BoardRef(const BoardPtr& p) : name(p ? p->name : std::string()) {}
};

}

#endif // BOARD_H
