#include "LateX.h"
#include <spdlog/spdlog.h>
#include "../Options.h"
#include <string>
#include <fmt/core.h>
#include <cstring>
#include <cstdlib>
//#include <format>

slope::LatexPtr slope::Latex::Add(const TexObject &tex,scalar scale,int width)
{
    return MakeObject(tex,scale,width,false);
}

slope::LatexPtr slope::Formula::Add(const TexObject &tex,scalar scale,int width)
{
    return MakeObject(tex,scale,width,true);
}

// a cache hit is served right away ; a miss only queues the primitive, so that
// every formula a deck declares is compiled by one single pdflatex run the
// first time a size or a draw is asked for
slope::LatexPtr slope::Latex::MakeObject(const TexObject &tex, scalar scale, int width, bool formula)
{
    LatexPtr rslt = NewPrimitive<Latex>();
    rslt->content = tex;
    rslt->tex_source = tex;
    rslt->isFormula = formula;
    rslt->scale = scale;
    rslt->width = width;
    rslt->full_content = WriteTexFile(tex,formula,width,rslt->tintable);

    path filename = GetLatexPath(rslt->full_content);
    if (io::file_exists(filename) && !Options::ignore_cache) {
        try {
            rslt->data = loadImage(filename);
            rslt->baseline = ReadBaseline(filename);
            return rslt;
        } catch (const std::exception& e) {
            spdlog::error("[latex] {}",e.what());
        }
    }
    pending.push_back(rslt);
    return rslt;
}

void slope::Latex::FlushPending()
{
    if (pending.empty())
        return;
    auto todo = pending;
    pending.clear();

    std::vector<LatexJob> jobs;
    for (const auto& l : todo)
        jobs.push_back({GetLatexPath(l->full_content),TexBody(l->tex_source,l->isFormula,l->width),l->tintable});
    spdlog::info("compiling {} latex primitives in one batch...",jobs.size());
    GenerateLatexBatch(jobs);

    for (const auto& l : todo) {
        try {
            l->data = loadImage(GetLatexPath(l->full_content));
            l->baseline = ReadBaseline(GetLatexPath(l->full_content));
        } catch (const std::exception& e) {
            spdlog::error("[latex] '{}' : {}",l->tex_source,e.what());
        }
    }
}

void slope::Latex::updateContent(json j)
{
    isFormula = j[0] == 1;
    content = j[1];
    tex_source = content;
    width = LatexLoader::GetWidth(j);
}

void slope::Latex::ensureRendered()
{
    auto tex_content = WriteTexFile(tex_source,isFormula,width,tintable);
    if (full_content == tex_content && data.width != -1)
        return;
    path filename = GetLatexPath(tex_content);
    try {
        if (!io::file_exists(filename) || Options::ignore_cache)
            GenerateLatex(filename,tex_content);
        data = loadImage(filename);
        baseline = ReadBaseline(filename);
    } catch (const std::exception& e) {
        spdlog::error("[latex] '{}' : {}", tex_source, e.what());
    }
    full_content = tex_content;
}


void slope::Latex::DeclareMathOperator(const TexObject &name, const TexObject &content) {
    context += "\\DeclareMathOperator*{\\" + name + "}{" + content + "}";
}

void slope::Latex::AddFileToPrefix(const path &p)
{
    path fp = formatPath(p);
    std::ifstream t(fp);
    if (!t)
        spdlog::warn("could not read latex prefix file {}", fp.string());
    std::stringstream buffer;
    buffer << t.rdbuf();
    context += buffer.str();

    ContextPart part{true, p.string(), {}};
    try {
        part.last_modified = std::filesystem::last_write_time(fp);
    } catch (const std::exception&) {}
    context_parts.push_back(part);
}

void slope::Latex::rebuildContext()
{
    context = "";
    for (const auto& part : context_parts) {
        if (!part.is_file) {
            context += part.value;
            continue;
        }
        std::ifstream t(formatPath(part.value));
        std::stringstream buffer;
        buffer << t.rdbuf();
        context += buffer.str();
    }
}

// the compilation itself runs off the render thread : every primitive keeps
// showing its previous image until PumpBatch picks the new ones up
void slope::Latex::RegenerateAll()
{
    FlushPending();
    if (batch_future.valid())
        batch_future.wait();
    PumpBatch();

    std::vector<LatexJob> jobs;
    batch_targets.clear();
    for (const auto& p : Primitive::primitives) {
        auto l = std::dynamic_pointer_cast<Latex>(p);
        if (!l)
            continue;
        auto tex_content = WriteTexFile(l->tex_source,l->isFormula,l->width,l->tintable);
        if (l->full_content == tex_content && l->data.width != -1)
            continue;
        l->full_content = tex_content;
        path filename = GetLatexPath(tex_content);
        if (!io::file_exists(filename) || Options::ignore_cache)
            jobs.push_back({filename,TexBody(l->tex_source,l->isFormula,l->width),l->tintable});
        batch_targets.push_back(l);
    }
    if (batch_targets.empty()) {
        spdlog::info("... latex up to date!");
        return;
    }
    spdlog::info("recompiling {} latex primitives ({} to render)...",batch_targets.size(),jobs.size());
    batch_future = std::async(std::launch::async,[jobs]{GenerateLatexBatch(jobs);});
}

void slope::Latex::PumpBatch()
{
    if (!batch_future.valid())
        return;
    if (batch_future.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        return;
    batch_future.get();
    for (const auto& l : batch_targets) {
        try {
            l->data = loadImage(GetLatexPath(l->full_content));
            l->baseline = ReadBaseline(GetLatexPath(l->full_content));
        } catch (const std::exception& e) {
            spdlog::error("[latex] {}",e.what());
        }
    }
    spdlog::info("... {} latex primitives reloaded!",batch_targets.size());
    batch_targets.clear();
}

void slope::Latex::HotReloadPrefixIfModified()
{
    FlushPending();
    PumpBatch();

    static auto last_refresh = Time::now();
    if (TimeFrom(last_refresh) < 0.2)
        return;
    last_refresh = Time::now();

    bool changed = false;
    for (auto& part : context_parts) {
        if (!part.is_file)
            continue;
        try {
            auto last_write = std::filesystem::last_write_time(formatPath(part.value));
            if (part.last_modified < last_write) {
                part.last_modified = last_write;
                changed = true;
            }
        } catch (const std::exception&) {}
    }
    if (!changed)
        return;

    spdlog::info("latex prefix changed, recompiling latex...");
    rebuildContext();
    RegenerateAll();
}

static std::string quote(const std::string& s) {
    std::string t = s;
#ifdef _WIN32
    // drop the trailing separator, a backslash before the quote escapes it
    while (!t.empty() && (t.back() == '\\' || t.back() == '/'))
        t.pop_back();
#endif
    return "\"" + t + "\"";
}

// a border fixed in pixels makes the on-screen size of the very same formula
// depend on the density (43% spread between 300 and 1200 dpi) : keep it at a
// constant 1.2pt instead, which is exactly the historical 10px at 600 dpi
static std::string borderPx() {
    return std::to_string(std::max<std::size_t>(1,(slope::Options::PDFtoPNGDensity*12+360)/720));
}

// glyphs overshoot their tex box (accents, italics, big operators) and a page
// cut exactly on the box clips them, so the preview page keeps a margin that
// -trim then removes
static constexpr double PreviewBorderPt = 5;

// heights above the first baseline, in pt, one per \typeout of the run
static std::vector<double> ReadBaselineHeights(const slope::path& logfile)
{
    std::vector<double> rslt;
    std::ifstream f(logfile);
    std::string line;
    while (std::getline(f,line)) {
        auto at = line.find("SLOPEBASELINE ");
        if (at == std::string::npos)
            continue;
        rslt.push_back(std::atof(line.c_str() + at + 14));
    }
    return rslt;
}

// -trim records the crop it applied in the png's caNv chunk, which is the only
// way left to know where the box sat before the ink was cropped out of it
static int ReadTrimYOffset(const slope::path& png)
{
    std::ifstream f(png,std::ios::binary);
    char sig[8];
    if (!f.read(sig,8))
        return -1;
    while (f) {
        unsigned char head[8];
        if (!f.read(reinterpret_cast<char*>(head),8))
            break;
        std::uint32_t len = (head[0]<<24)|(head[1]<<16)|(head[2]<<8)|head[3];
        if (std::memcmp(head+4,"caNv",4) == 0) {
            unsigned char d[16];
            if (!f.read(reinterpret_cast<char*>(d),16))
                break;
            return (d[12]<<24)|(d[13]<<16)|(d[14]<<8)|d[15];
        }
        if (std::memcmp(head+4,"IDAT",4) == 0)
            break;
        f.seekg(len + 4,std::ios::cur);
    }
    return -1;
}

slope::path slope::BaselinePath(const path& png) {return png.string() + ".bl";}

// distance, in image pixels, from the top of the png down to the tex baseline
static void WriteBaseline(const slope::path& png,double height_pt)
{
    using namespace slope;
    int y_off = ReadTrimYOffset(png);
    if (y_off < 0)
        return;
    double baseline = std::atof(borderPx().c_str())
                      + (height_pt + PreviewBorderPt)*Options::PDFtoPNGDensity/72.0 - y_off;
    std::ofstream(BaselinePath(png)) << baseline;
}

double slope::ReadBaseline(const path& png)
{
    std::ifstream f(BaselinePath(png));
    double rslt = -1;
    if (f)
        f >> rslt;
    return rslt;
}

// stable across compilers and platforms, unlike std::hash, so a cache can be
// moved with a project
slope::path slope::GetLatexPath(const TexObject &tex)
{
    std::uint64_t h = 1469598103934665603ull;
    for (unsigned char c : tex + "|density=" + std::to_string(Options::PDFtoPNGDensity)) {
        h ^= c;
        h *= 1099511628211ull;
    }
    return Options::CachePath + fmt::format("{:016x}",h) + ".png";
}

slope::TexObject slope::Latex::context = "";
std::vector<slope::Latex::ContextPart> slope::Latex::context_parts;
std::future<void> slope::Latex::batch_future;
std::vector<slope::LatexPtr> slope::Latex::batch_targets;
std::vector<slope::LatexPtr> slope::Latex::pending;

void slope::GenerateLatex(const path &filename,
                          const TexObject &texcontent)
{
    spdlog::info("Generating latex for '{}'...", texcontent);

    // one job name per formula, inside the cache : no cwd pollution and no
    // clash between two slope instances
    std::string job = (Options::CachePath + filename.stem().string());
    path tex_file = job + ".tex";
    path pdf_file = job + ".pdf";

    {
        std::ofstream formula_file(tex_file);
        formula_file << texcontent;
    }

    std::string latex_cmd = fmt::format("{} -interaction=nonstopmode -halt-on-error -no-shell-escape -output-directory={} {} >> {} 2>&1",
                                        quote(Options::PathToPDFLATEX),
                                        quote(Options::CachePath),
                                        quote(tex_file.string()),
                                        quote(Options::LogPath)
                                        );

    if (int rc = runCommand(latex_cmd)) {
        spdlog::error("[error while generating latex] cmd fail (exit {}) {}",rc,latex_cmd);
        std::cerr << Tail(Options::LogPath,20) << std::endl;
        throw std::runtime_error("Fail to generate latex");
    }

    // settings before the input, operators after, ImageMagick 7 requires it
    std::string convert_cmd = fmt::format("{} -density {} -quality 100 {} -trim -bordercolor none -border {} -colorspace sRGB {} >> {} 2>&1",
                                          quote(Options::PathToCONVERT),
                                          Options::PDFtoPNGDensity,
                                          quote(pdf_file.string()),
                                          borderPx(),
                                          quote(filename.string()),
                                          quote(Options::LogPath)
                                          );
    if (runCommand(convert_cmd)) {
        spdlog::error("[error while converting to png] cmd fail {}",convert_cmd);
        throw std::runtime_error("could not convert pdf to png");
    }

    auto heights = ReadBaselineHeights(job + ".log");
    for (auto ext : {".tex",".pdf",".aux",".log"})
        std::filesystem::remove(job + ext);

    // a multi-page pdf makes the converter write <name>-0.png, <name>-1.png ...
    // and never <name>.png, which would fail much later at load time
    if (!io::file_exists(filename))
        throw std::runtime_error("latex produced no image");
    if (heights.size() == 1)
        WriteBaseline(filename,heights[0]);
    spdlog::info("Generating latex for '{}' done.", texcontent);
}

// one pdflatex run for every formula sharing a preamble, then a single
// conversion splitting the pdf pages : N compiles become 1
static void CompileGroup(bool white,const std::vector<const slope::LatexJob*>& group)
{
    using namespace slope;
    std::string doc = TexPreamble(white);
    for (const auto* it : group)
        doc += it->body;
    doc += "\\end{document}\n";

    std::string job = Options::CachePath + "batch";
    {
        std::ofstream f(job + ".tex");
        f << doc;
    }

    bool ok = runCommand(fmt::format("{} -interaction=nonstopmode -halt-on-error -no-shell-escape -output-directory={} {} >> {} 2>&1",
                                     quote(Options::PathToPDFLATEX),quote(Options::CachePath),
                                     quote(job + ".tex"),quote(Options::LogPath))) == 0;
    if (ok)
        ok = runCommand(fmt::format("{} -density {} -quality 100 {} -trim -bordercolor none -border {} -colorspace sRGB -scene 0 {} >> {} 2>&1",
                                    quote(Options::PathToCONVERT),Options::PDFtoPNGDensity,
                                    quote(job + ".pdf"),borderPx(),
                                    quote(job + "-%d.png"),
                                    quote(Options::LogPath))) == 0;

    auto heights = ReadBaselineHeights(job + ".log");
    for (auto ext : {".tex",".pdf",".aux",".log"})
        std::filesystem::remove(job + ext);

    // page i is formula i : should one of them span two pages, every following
    // page would silently land on the wrong primitive, so refuse the mapping
    if (ok && io::file_exists(job + "-" + std::to_string(group.size()) + ".png")) {
        spdlog::warn("[latex] a formula of the batch spans several pages");
        ok = false;
    }

    // a single bad formula fails the whole document : fall back to compiling
    // the group one by one so only the offender is lost
    if (!ok) {
        for (std::size_t i = 0;io::file_exists(job + "-" + std::to_string(i) + ".png");i++)
            std::filesystem::remove(job + "-" + std::to_string(i) + ".png");
        spdlog::warn("[latex] batch of {} failed, falling back to one compile per formula",group.size());
        for (auto* it : group) {
            try {
                GenerateLatex(it->png,TexPreamble(white) + it->body + "\\end{document}\n");
            } catch (const std::exception& e) {
                spdlog::error("[latex] {}",e.what());
            }
        }
        return;
    }

    for (std::size_t i = 0;i<group.size();i++) {
        std::string page = job + "-" + std::to_string(i) + ".png";
        std::error_code ec;
        std::filesystem::rename(page,group[i]->png,ec);
        if (ec) {
            spdlog::error("[latex] missing page {} of the batch",i);
            continue;
        }
        if (i < heights.size())
            WriteBaseline(group[i]->png,heights[i]);
    }
}

void slope::GenerateLatexBatch(const std::vector<LatexJob> &jobs)
{
    // the width now travels in the body, so only the glyph color splits a batch
    std::map<bool,std::vector<const LatexJob*>> groups;
    for (const auto& j : jobs)
        groups[j.white].push_back(&j);
    for (const auto& [white,group] : groups)
        CompileGroup(white,group);
}

void slope::LatexLoader::Init(path P)
{
    source_path = formatPath(P);
    parseJson();
    source_last_modified = std::filesystem::last_write_time(source_path);
    initialized = true;
}

slope::ScreenPrimitiveInSlide slope::LatexLoader::LoadWithAnchor(key k)
{
    return Load(k)->at(k);
}

slope::LatexPtr slope::LatexLoader::Load(key k)
{
    if (! source.contains(k))
        throw std::runtime_error("Latex source does not contain key " + k);

    auto obj = source[k];
    LatexPtr rslt;

    if (!obj.is_array())
        throw std::runtime_error("Latex source object " + k + " is not an array");

    int width = GetWidth(obj);

    rslt = Latex::MakeObject(obj[1],1,width,obj[0] == 1);
    loaded[k] = rslt;
    return rslt;
}

void slope::LatexLoader::parseJson()
{
    if (!io::file_exists(source_path)){
        throw std::runtime_error("did not find latex source json file");
    }
    std::ifstream t(source_path);
    std::stringstream buffer;
    buffer << t.rdbuf();
    std::string content = buffer.str();

    std::regex backslash_regex(R"(\\)");
    content = std::regex_replace(content, backslash_regex, R"(\\)");

    if (!json::accept(content)){
        throw std::runtime_error("invalid json in latex source");
    }
    source = json::parse(content);
}

void slope::LatexLoader::ReloadContentAndUpdate()
{
    spdlog::info("reloading latex from latex source...");
    try {
        parseJson();
        for (auto& [key,objptr] : loaded) {
            const auto& content = source[key];
            objptr->updateContent(content);
        }
        Latex::RegenerateAll();
    }
    catch (const std::exception& e) {
        spdlog::error("Failed to reload latex: {}", e.what());
    }
}

void slope::LatexLoader::HotReloadIfModified()
{
    static auto last_refresh = Time::now();
    if (TimeFrom(last_refresh) > 0.2){
        last_refresh = Time::now();
        try {
            auto last_write = std::filesystem::last_write_time(source_path);
            if (source_last_modified < last_write ){
                source_last_modified = last_write ;
                ReloadContentAndUpdate();
            }
        } catch (std::exception& e) {
            spdlog::warn("Latex source unavailable {}",e.what());
        }
    }
}

slope::path slope::LatexLoader::source_path;
slope::json slope::LatexLoader::source;
std::map<slope::LatexLoader::key,slope::LatexPtr> slope::LatexLoader::loaded;
std::filesystem::file_time_type slope::LatexLoader::source_last_modified;
bool slope::LatexLoader::initialized = false;


// preview/tightpage makes each body its own page, cropped to its own content :
// nothing can overflow a fixed page any more, and the converter only
// rasterizes what is actually there.
// `white` renders the glyphs white so that the draw-time tint can give them any
// color ; it is opt-in because the tint multiplies, and would otherwise turn
// every \textcolor of a normal formula black
std::string slope::TexPreamble(bool white)
{
    return R"(\documentclass{article}
\usepackage[active,tightpage]{preview}
\setlength\PreviewBorder{5pt}
\newsavebox\slopebox
\usepackage{varwidth}
\usepackage{amsmath}
\usepackage{amsfonts}
\usepackage{xcolor}
\usepackage{url}
\usepackage{aligned-overset}
\usepackage{ragged2e}
\usepackage{booktabs}
\setlength{\parindent}{0pt}
)" + Latex::context + "\n" +
           R"(\begin{document}
)" + (white ? "\\color{white}\n" : "");
}

// the body is boxed before being previewed so that TeX can report the height
// of that very box : [t] makes it the height above the *first* baseline, which
// is what lets two formulas of different heights be aligned on it
std::string slope::TexBody(const TexObject &tex, bool formula, int width)
{
    // the historical textwidth of the article page : wrapping without an
    // explicit width has to keep breaking lines where it used to
    std::string w = width == -1 ? "493.69707" : std::to_string(width);
    std::string body = formula
        ? "$\\displaystyle\\begin{aligned}[t]\n" + tex + "\n\\end{aligned}$"
        : "\\begin{varwidth}[t]{" + w + "pt}\n" + tex + "\n\\end{varwidth}";
    return "\\sbox\\slopebox{" + body + "}\n"
           "\\typeout{SLOPEBASELINE \\the\\ht\\slopebox}\n"
           "\\begin{preview}\\usebox\\slopebox\\end{preview}\n";
}

std::string slope::WriteTexFile(const TexObject &tex, bool formula, int width, bool white)
{
    return TexPreamble(white) + TexBody(tex,formula,width) + "\\end{document}\n";
}

std::string slope::Tail(const path &p, std::size_t n)
{
    // read last n lines of file at p
    std::ifstream file(p);
    std::deque<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
        if (lines.size() > n) {
            lines.pop_front();
        }
    }
    std::string result;
    for (const auto& l : lines) {
        result += l + "\n";
    }
    return result;

}
