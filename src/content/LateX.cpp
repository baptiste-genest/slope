#include "LateX.h"
#include <spdlog/spdlog.h>
#include "Options.h"
#include <string>

slope::LatexPtr slope::Latex::Add(const TexObject &tex,scalar scale)
{
    path filename = GetLatexPath(tex,false);

    if (!io::file_exists(filename) || Options::ignore_cache)
        GenerateLatex(filename,tex,false);
    LatexPtr rslt = NewPrimitive<Latex>();
    rslt->content = tex;
    rslt->data = loadImage(filename);
    rslt->isFormula = false;
    rslt->scale = scale;
    return rslt;
}

slope::LatexPtr slope::Formula::Add(const TexObject &tex,scalar scale)
{
    path filename = GetLatexPath(tex,true);

    if (!io::file_exists(filename) || Options::ignore_cache)
        GenerateLatex(filename,tex,true);
    LatexPtr rslt = NewPrimitive<Latex>();
    rslt->content = tex;
    rslt->data = loadImage(filename);
    rslt->isFormula = true;
    rslt->scale = scale;
    return rslt;
}

void slope::Latex::updateContent(const TexObject &new_content, scalar s)
{
    path filename = GetLatexPath(new_content,isFormula);

    if (!io::file_exists(filename))
        GenerateLatex(filename,new_content,isFormula);
    content = new_content;
    data = loadImage(filename);
    scale = s;
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

slope::path slope::GetLatexPath(const TexObject &tex,bool formula)
{
    std::string bit = formula ? "1" : "0";
    auto H = std::hash<std::string>{}(Latex::context + tex+ bit +std::to_string(slope::Options::PDFtoPNGDensity));
    return Options::CachePath + std::to_string(H) + ".png";
//    return Options::ProjectDataPath + "formulas/" + std::to_string(H) + ".png";
}

slope::TexObject slope::Latex::context = "";

void slope::GenerateLatex(const path &filename,
                         const TexObject &tex,
                         bool formula)
{
    spdlog::info("Generating latex for '{}'...", tex);
    std::ofstream formula_file("/tmp/formula.tex");
    //formula_file << "\\documentclass[varwidth,border=3pt]{standalone}" << std::endl;
    formula_file << "\\documentclass{article}" << std::endl;
//    formula_file << "\\usepackage{standalone}" << std::endl;
    formula_file << "\\thispagestyle{empty}"<< std::endl;
    //formula_file << "\\usepackage[top=0cm,bottom=0cm,left=0cm,right=0cm]{geometry}"<< std::endl;
    formula_file << "\\usepackage[top=0cm,bottom=0cm,right=7cm]{geometry}"<< std::endl;
    formula_file << "\\usepackage{amsmath}"<< std::endl;
    formula_file << "\\usepackage{amsfonts}"<< std::endl;
    formula_file << "\\usepackage{xcolor}"<< std::endl;
    formula_file << "\\usepackage{url}"<< std::endl;
    formula_file << "\\usepackage{aligned-overset}"<< std::endl;
    formula_file << "\\usepackage{ragged2e}"<< std::endl;
    formula_file << "\\usepackage{booktabs}"<< std::endl;
    formula_file << "\\setlength{\\parindent}{0pt}"<< std::endl;

    formula_file << Latex::context << std::endl;
    formula_file << "\\begin{document}"<< std::endl;
    formula_file << "\\pdfpageheight=80pt" << std::endl;
    if (formula)
        formula_file << "\\begin{align*}"<< std::endl;
    formula_file << tex  << std::endl;
    if (formula)
        formula_file << "\\end{align*}"<< std::endl;
    formula_file << "\\end{document}"<< std::endl;

    std::string latex_cmd = Options::PathToPDFLATEX+" -output-directory=/tmp /tmp/formula.tex  >>/tmp/slope.log";
    if (std::system(latex_cmd.c_str())) {
        spdlog::error("[error while generating latex] cmd fail {}",latex_cmd);
        throw std::runtime_error("could not generate latex");
    }
    std::string convert_cmd = Options::PathToCONVERT+" -density "+std::to_string(Options::PDFtoPNGDensity)+" -quality 100 -trim -border 10 -bordercolor none /tmp/formula.pdf -colorspace RGB " + filename.string() + " >>/tmp/slope.log";
    if (std::system(convert_cmd.c_str())) {
        spdlog::error("[error while converting to png] cmd fail {}",latex_cmd);
        throw std::runtime_error("could not convert pdf to png");
    }
//    int h = (1080.*height_ratio/99.)*Image::getSize(filename).y;
    /*
    spdlog::info((Options::Slope_CONVERT+" " + filename.string() + " -resize x" + std::to_string(h) + " " + filename.string() + " >>/tmp/UPS.log"));
    if (std::system((Options::Slope_CONVERT+" " + filename.string() + " -resize x" + std::to_string(h) + " " + filename.string() + " >>/tmp/UPS.log").c_str())){
        spdlog::error("[error while resizing]");
        assert(false);
    }
*/
    spdlog::info("Generating latex for '{}' done.", tex);
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
    auto obj = source[k];
    LatexPtr rslt;
    if (obj[0] == 0)
        rslt = Latex::Add(obj[1],obj[2]);
    else
        rslt = Formula::Add(obj[1],obj[2]);
    loaded[k] = rslt;
    return rslt;
}

void slope::LatexLoader::parseJson()
{
    if (!io::file_exists(source_path)){
        spdlog::critical("invalid path {}",source_path.string());
        exit(1);
    }
    std::ifstream t(source_path);
    std::stringstream buffer;
    buffer << t.rdbuf();
    std::string content = buffer.str();

    std::regex backslash_regex(R"(\\)");
    content = std::regex_replace(content, backslash_regex, R"(\\)");




    if (!json::accept(content)){
        spdlog::critical("invalid json file {}",content);
        exit(1);
    }
    source = json::parse(content);
}

void slope::LatexLoader::ReloadContentAndUpdate()
{
    spdlog::info("reloading latex from latex source...");
    parseJson();


    for (auto& [key,objptr] : loaded) {
        const auto& content = source[key];
        int isFormula = content[0];
        if (objptr->isFormula != isFormula)
            objptr->isFormula = isFormula;
        objptr->updateContent(content[1],content[2]);
    }
    spdlog::info("... done!");
}

void slope::LatexLoader::HotReloadIfModified()
{
    try {
        auto last_write = std::filesystem::last_write_time(source_path);
        if (source_last_modified < last_write) {
            source_last_modified = last_write;
            ReloadContentAndUpdate();
        }
    } catch (...) {
        spdlog::warn("Latex source unavailable");
    }
}

slope::path slope::LatexLoader::source_path;
slope::json slope::LatexLoader::source;
std::map<slope::LatexLoader::key,slope::LatexPtr> slope::LatexLoader::loaded;
std::filesystem::file_time_type slope::LatexLoader::source_last_modified;
bool slope::LatexLoader::initialized = false;

