#include "Curve2D.h"
#include "../Options.h"

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
        std::ofstream buffer("/tmp/buffer.svg");
        buffer << content;
        if (rel_size(0) > 0){
        std::string size = std::to_string(int(rel_size(0)*1920))+"x"+std::to_string(int(rel_size(1)*1080));
        system(("convert -background none -size "+size+ " /tmp/buffer.svg " + filename).c_str());
        }
        else {
            system(("convert -background none -density 300 /tmp/buffer.svg " + filename).c_str());
        }
    }
    return Image::Add(filename.c_str());

}
