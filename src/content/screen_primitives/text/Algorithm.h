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
 * Lines count as the package numbers them, shown or not. \slopemark{name}
 * names the line it sits on. The package goes in the preamble; algorithm2e
 * needs \begin{algorithm}[H], a float cannot be boxed.
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

    void clearCues() { reveal_at.clear(); focus_at.clear(); }
    static void ClearAllCues();

    // file-backed listings, for the in-app editor and hot reload
    static std::vector<path> WatchedFiles();
    static void HotReloadIfModified();

    void draw(const TimeObject& t, const StateInSlide& sis) override;
    void playIntro(const TimeObject& t, const StateInSlide& sis) override { draw(t, sis); }
    void playOutro(const TimeObject& t, const StateInSlide& sis) override { draw(t, sis); }

private:
    std::string key;                       // names the .lines file in the cache
    path source_file;
    std::filesystem::file_time_type last_modified;
    int listing_width = -1;
    void setSource(const TexObject& tex);
    std::vector<double> baseline_px;       // [n-1] = line n, png pixels from the top
    std::map<std::string, int> marks;
    std::string parsed_for;                // full_content the lines were read for
    double pitch_px = 0;

    std::map<int, int> reveal_at;
    std::map<int, std::pair<int,int>> focus_at;

    void parseLines();
    int count() const { return int(baseline_px.size()); }
    int markOf(const std::string& label) const;
    int revealOn(int slide) const;
    bool focusOn(int slide, int& first, int& last) const;
    // png y of line l's baseline, l fractional and 1-based
    double yOf(double l) const;

    inline static std::vector<Algorithm*> all;
};

}

#endif // ALGORITHM_H
