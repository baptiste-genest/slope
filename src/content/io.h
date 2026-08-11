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

// takes `const path&` rather than `std::string` so a slope::path argument
// never goes through path::operator string_type(), which is std::wstring on
// Windows and would silently fail to convert to std::string there
inline std::string formatPath(const path& p) {
    if (p.is_absolute()) return p.string();
    return Options::ProjectDataPath + p.string();
}

// Options::CachePath/ProjectPath/ProjectViewsPath are plain strings with a
// trailing separator, built by simple concatenation everywhere they're used
// (Options::CachePath + filename, etc). Keep that separator the platform's
// native one rather than a hand-written "/", so a path never ends up mixing
// both on Windows.
inline std::string normalizedDir(const path& p) {
    path np = p;
#ifdef _WIN32
    // Resolve 8.3 short names ("C:\Users\RUNNER~1\...", which is what
    // %TEMP% expands to for a long user name) to their long form. TeX treats
    // '~' as an active character in the file name it is handed on the
    // command line, so a short path makes pdflatex split it at the tilde and
    // give up : "! I can't find file `C:/Users/RUNNER'".
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

// Runs a shell command the way std::system() does, but works around a
// cmd.exe parsing rule that would otherwise break every command we build.
//
// std::system() on Windows runs `cmd.exe /c <command>`. Per `cmd /?`, when
// the command line starts with a quote and does NOT consist of exactly two
// quote characters, cmd strips the first and the last quote of the whole
// line before parsing it. Our commands quote the executable *and* two or
// three path arguments, so that strip turns the leading `"prog.exe"` into a
// bare `prog.exe"` with a stray trailing quote -- an invalid Windows
// filename, rejected with "The filename, directory name, or volume label
// syntax is incorrect" before the program is ever launched. Wrapping the
// entire command line in one more pair of quotes makes cmd's strip a no-op
// and leaves the intended command intact.
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
