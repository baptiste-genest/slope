#ifndef ALGORITHM_H
#define ALGORITHM_H

#include "content/screen_primitives/text/LateX.h"
#include "content/screen_primitives/text/Code.h"

namespace slope {

/*
 * A LaTeX algorithm, written with algorithmicx (algpseudocode, algcompatible and others) or
 * algorithm2e, with the reveal and focus of a Code block.
 *
 *   auto algo = Algorithm::FromFile("bfs.tex");   // \begin{algorithmic}...
 *   show << algo->at("algo") << algo->focus(3, 4);
 *
 * Lines are numbered as the package numbers them, whether they are shown or not,
 * and the count continues across several environments of the same file.
 * \slopemark{name} gives a name to the line where it is written.
 * The package goes in the preamble.
 * algorithm2e needs \begin{algorithm}[H], because a float cannot be put in a box.
 * Line positions come from \pdfsavepos, so focus and reveal need pdflatex.
 */
class Algorithm;
using AlgorithmPtr = std::shared_ptr<Algorithm>;

class Algorithm : public Latex {
public:
    // Builds an algorithm from LaTeX source.
    // The width in pt limits the algorithm. The rules of algorithm2e and the \tcp* comments use all of it.
    static AlgorithmPtr Add(const TexObject& tex, scalar scale = 1, int width = -1);
    // Builds an algorithm from a .tex file, which is reloaded when it changes.
    static AlgorithmPtr FromFile(const path& file, scalar scale = 1, int width = -1);

    // Color of the band behind the focused lines.
    Color highlight = Color("code/highlight", ColorType(1.00f, 0.85f, 0.30f, 0.35f));
    // Opacity of the lines outside the focus. 1 keeps them fully visible.
    float dim_factor = 0.35f;

    // Each cue applies from the slide where it is added. Reveal shows the lines up to a point,
    // given as START, END, a name set by \slopemark or a line number.
    SlideCue reveal(CodeAnchor where);
    SlideCue reveal(const std::string& label);
    SlideCue reveal(int line);
    // Focus highlights a line range, given by names or by numbers, and dims the other lines.
    SlideCue focus(const std::string& label);
    SlideCue focus(const std::string& from, const std::string& to);
    SlideCue focus(int first_line, int last_line);
    // Removes the focus.
    SlideCue unfocus();

    // Removes the cues of this algorithm.
    void clearCues() {
        reveal_at.clear();
        focus_at.clear();
        parsed_for.clear();
    }
    // Removes the cues of every algorithm.
    static void ClearAllCues();

    // Algorithms read from a file, used by the in-app editor and by the reload.
    static std::vector<path> WatchedFiles();
    // Reloads the algorithms whose file changed.
    static void HotReloadIfModified();

    void draw(const TimeObject& t, const StateInSlide& sis) override;
    void playIntro(const TimeObject& t, const StateInSlide& sis) override { draw(t, sis); }
    void playOutro(const TimeObject& t, const StateInSlide& sis) override { draw(t, sis); }

    // Builds the source again after a change of the preamble. The preamble is part of the key, so the cache entry gets a new name.
    void refreshSource() override { setSource(content); }

private:
    // Name of the .lines file in the cache.
    std::string key;
    path source_file;
    std::filesystem::file_time_type last_modified;
    int listing_width = -1;
    // Sets the LaTeX source.
    void setSource(const TexObject& tex);

    // A line given by number or by name. A name is resolved when the lines are read.
    struct LineRef {
        std::string label;
        int line = 0;
    };

    // Position of the baseline of line n at index n-1, in png pixels from the top.
    std::vector<double> baseline_px;
    // Position between line k and line k+1 at index k, for k from 0 to count.
    std::vector<double> edge_px;
    // Line number of each name set by \slopemark.
    std::map<std::string, int> marks;
    // Source for which the lines were read.
    std::string parsed_for;
    double pitch_px = 0;
    bool warned_plane = false;

    std::map<int, LineRef> reveal_at;
    std::map<int, std::pair<LineRef, LineRef>> focus_at;

    // Reads the line positions written by pdflatex.
    void parseLines();
    // Finds the boundary between lines in the blank rows above the ink of each line.
    void cutLines(const path& png);
    // Warns about names used by a cue and not defined.
    void warnUnknownMarks();
    // Number of lines.
    int count() const { return int(baseline_px.size()); }
    // Line number of a reference.
    int resolve(const LineRef& ref) const;
    // Line up to which the algorithm is shown on a slide.
    int revealOn(int slide) const;
    // Gives the focused lines of a slide. Returns false when there is no focus.
    bool focusOn(int slide, int& first, int& last) const;
    // Height in png pixels of the cut below line k, where k can be fractional, from 0 to count.
    double edgeOf(double k) const;

    // Every algorithm.
    inline static std::vector<Algorithm*> all;
};

} // namespace slope

#endif // ALGORITHM_H
