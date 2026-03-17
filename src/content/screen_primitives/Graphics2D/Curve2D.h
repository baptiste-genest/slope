#ifndef CURVE2D_H
#define CURVE2D_H

#include "../Image.h"
#include "../../io.h"
#include "../../../extern/svg.hpp"

namespace slope {

struct Figure
{
public:
    using FigurePtr = std::shared_ptr<Image>;
    Figure& PlotFunction(scalar x1,scalar x2,const scalar_function& f,int N = 500);
    static FigurePtr Add(Figure& fig,vec2 rel_size = vec2(-1,-1));
private:
    SVG::SVG root;
};

}

#endif // CURVE2D_H
