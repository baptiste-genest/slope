#ifndef ALGORITHM_H
#define ALGORITHM_H

#include "content/screen_primitives/text/LateX.h"
#include "content/screen_primitives/text/Code.h"

namespace slope {

/*
 * A LaTeX algorithm, algorithmicx (algpseudocode, algcompatible...) or
 * algorithm2e, with the reveal and focus of a Code listing.
 *
 *   auto algo = Algorithm::FromFile("bfs.tex");   // \begin{algorithmic}...
 *   show << algo->at("listing") << algo->focus(3, 4);
 *
 * Lines count as the package numbers them, shown or not, and keep counting
 * across several environments of the same file. \slopemark{name} names the
 * line it sits on. The package goes in the preamble; algorithm2e needs
 * \begin{algorithm}[H], a float cannot be boxed. Line positions come from
 * \pdfsavepos, so focus and reveal need pdflatex.
 */
class Algorithm;
using AlgorithmPtr = std::shared_ptr<Algorithm>;

class Algorithm : public Latex
{
public:
    // width in pt caps the listing; algorithm2e rules and \tcp* comments span all of it
    static AlgorithmPtr Add(const TexObject& tex, scalar scale = 1, int width = -1);
    static AlgorithmPtr FromFile(const path& file, scalar scale = 1, int width = -1);

    Color highlight = Color("code/highlight", ColorType(1.00f, 0.85f, 0.30f, 0.35f));
    float dim_factor = 0.35f;

    SlideCue reveal(CodeAnchor where);
    SlideCue reveal(const std::string& label);
    SlideCue reveal(int line);
    SlideCue focus(const std::string& label);
    SlideCue focus(const std::string& from, const std::string& to);
    SlideCue focus(int first_line, int last_line);
    SlideCue unfocus();

    void clearCues() { reveal_at.clear(); focus_at.clear(); parsed_for.clear(); }
    static void ClearAllCues();

    // file-backed listings, for the in-app editor and hot reload
    static std::vector<path> WatchedFiles();
    static void HotReloadIfModified();

    void draw(const TimeObject& t, const StateInSlide& sis) override;
    void playIntro(const TimeObject& t, const StateInSlide& sis) override { draw(t, sis); }
    void playOutro(const TimeObject& t, const StateInSlide& sis) override { draw(t, sis); }

    // the preamble is in the key, so a prefix reload renames the cache entry
    void refreshSource() override { setSource(content); }

private:
    std::string key;                       // names the .lines file in the cache
    path source_file;
    std::filesystem::file_time_type last_modified;
    int listing_width = -1;
    void setSource(const TexObject& tex);

    // a cue names a line either by number, or by the mark it resolves to when
    // the positions are read, so that an edited \slopemark moves the cue
    struct LineRef {
        std::string label;
        int line = 0;
    };

    std::vector<double> baseline_px;       // [n-1] = line n, png pixels from the top
    std::vector<double> edge_px;           // [k] = between line k and k+1, k in [0,count]
    std::map<std::string, int> marks;
    std::string parsed_for;                // full_content the lines were read for
    double pitch_px = 0;
    bool warned_plane = false;

    std::map<int, LineRef> reveal_at;
    std::map<int, std::pair<LineRef, LineRef>> focus_at;

    void parseLines();
    void cutLines(const path& png);
    void warnUnknownMarks();
    int count() const { return int(baseline_px.size()); }
    int resolve(const LineRef& ref) const;
    int revealOn(int slide) const;
    bool focusOn(int slide, int& first, int& last) const;
    // png y of the cut below line k, k fractional in [0,count]
    double edgeOf(double k) const;

    inline static std::vector<Algorithm*> all;
};

}

#endif // ALGORITHM_H
