#include "Curve2D.h"
#include "../Options.h"
#include <fmt/core.h>

slope::Figure &slope::Figure::PlotFunction(scalar x1, scalar x2, const scalar_function &f, int N)
{
    root.style("path").set_attr("fill-opacity", "0")
        .set_attr("stroke", "#000000").set_attr("stroke-width","2");
    auto path = root.add_child<SVG::Path>();
    scalars X(N),Y(N);
    for (int i = 0;i<N;i++){
        X[i] = x1 + (x2-x1)*double(i)/(N-1);
        Y[i] = f(X[i]);
    }
    scalar r = 100;
    for (int i = 0;i<N;i++){
        path->line_to(r*X[i],r*Y[i]);
        //*shapes << SVG::Line(r*X[i],r*X[i+1],r*Y[i],r*Y[i+1]);
    }

    // Automatically scale width and height to fit elements
    root.autoscale();

    return *this;
}

slope::Figure::FigurePtr slope::Figure::Add(Figure& F,vec2 rel_size)
{
    std::string content = std::string(F.root);
    auto H = std::hash<std::string>{}(content);
    std::string filename = Options::ProjectDataPath + "figures/" + std::to_string(H) + ".png";


    if (!io::file_exists(filename)){
        // "/tmp" does not exist on Windows, and a bare "convert" there
        // resolves to System32's unrelated FAT-to-NTFS converter
        std::error_code ec;
        path svg = std::filesystem::temp_directory_path(ec) / "slope_buffer.svg";
        {
            std::ofstream buffer(svg);
            buffer << content;
        }
        std::string geometry = rel_size(0) > 0
            ? "-size " + std::to_string(int(rel_size(0)*1920)) + "x" + std::to_string(int(rel_size(1)*1080))
            : "-density 300";
        runCommand(fmt::format("\"{}\" -background none {} \"{}\" \"{}\"",
                               Options::PathToCONVERT, geometry, svg.string(), filename));
    }
    return Image::Add(filename.c_str());

}
