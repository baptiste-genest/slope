#ifndef IO_H
#define IO_H
#include "../libslope.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include "Options.h"

#include <fstream>

namespace slope {

// const path&, not std::string, path::c_str() is wchar_t* on Windows
inline std::string formatPath(const path& p) {
    if (p.is_absolute()) return p.string();
    return Options::ProjectDataPath + p.string();
}

// the Options paths are concatenated with filenames, so keep a native separator
inline std::string normalizedDir(const path& p) {
    path np = p;
#ifdef _WIN32
    // expand 8.3 short names, TeX treats their '~' as an active character
    std::error_code ec;
    path canonical = std::filesystem::weakly_canonical(np, ec);
    if (!ec && !canonical.empty())
        np = canonical;
#endif
    np = np.lexically_normal();
    np.make_preferred();
    std::string s = np.string();
    char sep = static_cast<char>(path::preferred_separator);
    if (s.empty() || s.back() != sep)
        s += sep;
    return s;
}

// std::system() on Windows, where cmd.exe /c strips the outermost quote pair
inline int runCommand(const std::string& cmd) {
#ifdef _WIN32
    return std::system(("\"" + cmd + "\"").c_str());
#else
    return std::system(cmd.c_str());
#endif
}

namespace io {

inline bool file_exists(const path& filename) {
    return std::filesystem::exists(filename);
}
inline bool folder_exists(const path& filename) {
    return std::filesystem::exists(filename);
}


using namespace std;
using namespace Eigen;

template<class T>
using Matrix = Eigen::Matrix<T,-1,-1>;

template<class T>
using Vector = Eigen::Vector<T,-1>;

//https://aleksandarhaber.com/eigen-matrix-library-c-tutorial-saving-and-loading-data-in-from-a-csv-file/
template<class T>
void SaveMatrix(string fileName, const Matrix<T> &M) {
    //https://eigen.tuxfamily.org/dox/structEigen_1_1IOFormat.html
    const static IOFormat CSVFormat(FullPrecision, DontAlignCols, ", ", "\n");

    ofstream file(fileName);
    if (file.is_open())
    {
        file << M.format(CSVFormat);
        file.close();
    }
}

template<class T>
void SaveVec(string fileName, const Vector<T> &V) {
    //https://eigen.tuxfamily.org/dox/structEigen_1_1IOFormat.html
    const static IOFormat CSVFormat(FullPrecision, DontAlignCols, ", ", "\n");

    ofstream file(fileName);
    if (file.is_open())
    {
        file << V.format(CSVFormat);
        file.close();
    }
}



template<typename T>
Matrix<T> LoadMatrix(string fileToOpen)
{
    vector<T> matrixEntries;
    ifstream matrixDataFile(fileToOpen);
    string matrixRowString;
    string matrixEntry;
    int matrixRowNumber = 0;

    while (getline(matrixDataFile, matrixRowString)) // here we read a row by row of matrixDataFile and store every line into the string variable matrixRowString
    {
        stringstream matrixRowStringStream(matrixRowString); //convert matrixRowString that is a string to a stream variable.
        while (getline(matrixRowStringStream, matrixEntry, ',')) // here we read pieces of the stream matrixRowStringStream until every comma, and store the resulting character into the matrixEntry
            matrixEntries.push_back(stod(matrixEntry));   //here we convert the string to double and fill in the row vector storing all the matrix entries
        matrixRowNumber++; //update the column numbers
    }
    return Map<Eigen::Matrix<T, Dynamic, Dynamic, RowMajor>>(matrixEntries.data(), matrixRowNumber, matrixEntries.size() / matrixRowNumber);
}

template<typename T>
Vector<T> LoadVec(string fileToOpen)
{
    return LoadMatrix<T>(fileToOpen);
}

template<class T>
inline bool MatrixCache(std::string file,Matrix<T>& M){
    if (file_exists(file)){
        M = LoadMatrix<T>(file);
        return true;
    }
    return false;
}

template<class T>
inline bool VecCache(std::string file,Vector<T>& V){
    if (file_exists(file)){
        V = LoadMatrix<T>(file);
        return true;
    }
    return false;
}


inline std::vector<std::string> list_directory(std::string folder,bool sorted = true) {
    std::vector<std::string> ls;
    namespace fs = std::filesystem;
    for (const auto & entry : fs::directory_iterator(folder))
        ls.push_back(entry.path().string());
    if (sorted)
        std::sort(ls.begin(),ls.end());
    return ls;
}

}

}

#endif // IO_H
