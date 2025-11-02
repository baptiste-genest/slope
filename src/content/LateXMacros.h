#ifndef LATEXMACROS_H
#define LATEXMACROS_H

#include "../libslope.h"
#include "Image.h"
#include "io.h"
#include "Options.h"

namespace slope {

using TexObject = std::string;

namespace tex {

inline TexObject frac(const TexObject& A,const TexObject& B) {
    return "\\frac{" + A +"}{" + B + "}";
}

inline TexObject dot(const TexObject& A,const TexObject& B) {
    return "\\langle{" + A +"," + B + "\\rangle}";
}

inline TexObject transpose(const TexObject& A) {
    return A + "^t";
}

inline TexObject text(const TexObject& A) {
    return "\\text{" + A + "}";
}


inline TexObject center(const TexObject& A) {
    return "\\begin{center} \n" + A + "\n\\end{center}";
}

inline TexObject equation(const TexObject& A,bool number = false) {
    if (number)
        return "\\begin{equation} \n" + A + "\n\\end{equation}";
    return "\\begin{equation*} \n" + A + "\n\\end{equation*}";
}

inline TexObject pow(const TexObject& A,const TexObject& B) {
    return A + "^{" + B + "}";
}

inline TexObject AaboveB(const TexObject& A,const TexObject& B) {
    return "\\overset{" + A + "}{" + B + "}";
}

inline TexObject AbelowB(const TexObject& A,const TexObject& B) {
    return "\\underset{" + A + "}{" + B + "}";
}

template<int N = 1>
inline TexObject del(int i,bool xyz = true) {
    TexObject D;
    if (N == 1)
        D = "\\partial ";
    else
        D = "\\partial^{" + std::to_string(N) + "}";
    if (xyz) {
        if (i == 0)
            return D + "x";
        else if (i == 1)
            return D + "y";
        return D + "z";
    }
    return D + "x_{"+std::to_string(i)+"}";
}

inline TexObject align(const std::vector<TexObject>& texs) {
    TexObject rslt = "\\begin{align*}\n";
    for (int i= 0;i<texs.size()-1;i++)
        rslt += texs[i] + "\\\\ \n";
    rslt += texs.back();
    rslt += "\n \\end{align*}";
    return rslt;
}

template <typename... ARGS>
TexObject align(ARGS... arguments) {
    std::vector<TexObject> data;
    data.reserve(sizeof...(arguments));
    (data.emplace_back(arguments), ...);
    return align(data);
}


inline TexObject Vec(const std::vector<TexObject>& texs) {
    TexObject rslt = "\\begin{pmatrix}\n";
    for (int i= 0;i<texs.size()-1;i++)
        rslt += texs[i] + "\\\\";
    rslt += texs.back();
    rslt += "\\end{pmatrix}";
    return rslt;
}

template <typename... ARGS>
TexObject Vec(ARGS... arguments) {
    std::vector<TexObject> data;
    data.reserve(sizeof...(arguments));
    (data.emplace_back(arguments), ...);
    return Vec(data);
}

inline TexObject cases(const std::vector<TexObject>& texs) {
    TexObject rslt = "\\begin{cases}\n";
    for (int i= 0;i<texs.size();i+=2)
        rslt += texs[i] + " ,&\\quad " + texs[i+1] + " \\\\";
    rslt += "\\end{cases}";
    return rslt;
}

template <typename... ARGS>
TexObject cases(ARGS... arguments) {
    std::vector<TexObject> data;
    data.reserve(sizeof...(arguments));
    (data.emplace_back(arguments), ...);
    return cases(data);
}

inline TexObject enumerate(const std::vector<TexObject>& texs) {
    TexObject rslt = "\\begin{enumerate}\n";
    for (int i= 0;i<texs.size();i++)
        rslt +=  "\\item " + texs[i] + '\n';
    rslt += "\\end{enumerate}";
    return rslt;
}

template <typename... ARGS>
TexObject enumerate(ARGS... arguments) {
    std::vector<TexObject> data;
    data.reserve(sizeof...(arguments));
    (data.emplace_back(arguments), ...);
    return enumerate(data);
}



template <int col,int row>
inline TexObject Mat(const std::vector<TexObject>& texs) {
    TexObject rslt = "\\begin{pmatrix}\n";
    for (int j = 0;j<row;j++){
        for (int i= 0;i<col-1;i++)
            rslt += texs[j*col + i]+ " & " ;
        if (j < row-1 )
            rslt += texs[j*col + (col-1)] + " \\\\ ";
        else
            rslt += texs[j*col + (col-1)];
    }
    rslt += "\\end{pmatrix}";
    return rslt;
}

template <int col,int row,typename... ARGS>
TexObject Mat(ARGS... arguments) {
    std::vector<TexObject> data;
    data.reserve(sizeof...(arguments));
    (data.emplace_back(arguments), ...);
    return Mat<col,row>(data);
}

}

}

#endif // LATEXMACROS_H
