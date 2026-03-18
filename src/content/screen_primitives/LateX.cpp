#include "LateX.h"
#include <spdlog/spdlog.h>
#include "../Options.h"
#include <string>
#include <fmt/core.h>
//#include <format>

slope::LatexPtr slope::Latex::Add(const TexObject &tex,scalar scale,int width)
{
    return MakeObject(tex,scale,width,false);
}

slope::LatexPtr slope::Formula::Add(const TexObject &tex,scalar scale,int width)
{
    return MakeObject(tex,scale,width,true);
}

slope::LatexPtr slope::Latex::MakeObject(const TexObject &tex, scalar scale, int width, bool formula)
{
    auto content = WriteTexFile(tex,formula,width);
    path filename = GetLatexPath(content);

    if (!io::file_exists(filename) || Options::ignore_cache)
        GenerateLatex(filename,content);
    LatexPtr rslt = NewPrimitive<Latex>();
    rslt->content = tex;
    rslt->full_content = content;
    rslt->data = loadImage(filename);
    rslt->isFormula = formula;
    rslt->scale = scale;
    return rslt;
}

void slope::Latex::updateContent(json j)
{
    isFormula = j[0] == 1;
    auto new_content = j[1];
    int width = LatexLoader::GetWidth(j);

    auto tex_content = WriteTexFile(new_content,isFormula,width);
    path filename = GetLatexPath(tex_content);

    if (full_content != tex_content) {
        GenerateLatex(filename,tex_content);
        content = new_content;
        full_content = tex_content;
        data = loadImage(filename);
    }
}


void slope::Latex::DeclareMathOperator(const TexObject &name, const TexObject &content) {
    context += "\\DeclareMathOperator*{\\" + name + "}{" + content + "}";
}

void slope::Latex::AddFileToPrefix(const path &p)
{
    std::ifstream t(formatPath(p));
    std::stringstream buffer;
    buffer << t.rdbuf();
    context += buffer.str();
}

slope::path slope::GetLatexPath(const TexObject &tex)
{
    auto H = std::hash<std::string>{}(tex);
    return Options::CachePath + std::to_string(H) + ".png";
}

slope::TexObject slope::Latex::context = "";

void slope::GenerateLatex(const path &filename,
                          const TexObject &texcontent)
{
    spdlog::info("Generating latex for '{}'...", texcontent);

    static path tex_file = "formula.tex";
    static path pdf_file = "formula.pdf";

    {
        std::ofstream formula_file(tex_file);
        formula_file << texcontent;
    }

    std::string latex_cmd = fmt::format("{} {} >> {}",
                                        Options::PathToPDFLATEX,
                                        tex_file.string(),
                                        Options::LogPath
                                        );

    if (std::system(latex_cmd.c_str())) {
        spdlog::error("[error while generating latex] cmd fail {}",latex_cmd);
        std::cerr << Tail(Options::LogPath,20) << std::endl;
        throw std::runtime_error("Fail to generate latex");
    }

    std::string convert_cmd = fmt::format("{} -density {} -quality 100 -trim -border 10 -bordercolor none {} -colorspace RGB {} >> {}",
                                          Options::PathToCONVERT,
                                          Options::PDFtoPNGDensity,
                                          pdf_file.string(),
                                          filename.string(),
                                          Options::LogPath
                                          );
    if (std::system(convert_cmd.c_str())) {
        spdlog::error("[error while converting to png] cmd fail {}",latex_cmd);
        throw std::runtime_error("could not convert pdf to png");
    }
    spdlog::info("Generating latex for '{}' done.", texcontent);
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
        spdlog::info("... done!");
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


std::string slope::WriteTexFile(const TexObject &tex, bool formula, int width)
{
    std::string width_block;
    if (width != -1)
        width_block = "\\geometry{textwidth=" + std::to_string(width) + "pt}\n";

    std::string align_begin = formula ? "\\begin{align*}\n" : "";
    std::string align_end   = formula ? "\\end{align*}\n"   : "";

    return R"(\documentclass{article}
\thispagestyle{empty}
\usepackage[top=0cm,bottom=0cm,left=1cm]{geometry}
)" + width_block +
           R"(\usepackage{amsmath}
\usepackage{amsfonts}
\usepackage{xcolor}
\usepackage{url}
\usepackage{aligned-overset}
\usepackage{ragged2e}
\usepackage{booktabs}
\setlength{\parindent}{0pt}
)" + Latex::context + "\n" +
           R"(\begin{document}
\pdfpageheight=120pt
)" + align_begin +
           tex + "\n" +
           align_end +
           R"(\end{document}
)";
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
