#ifndef IO_H
#define IO_H
#include "libslope.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include "content/config/Options.h"

#include <fstream>

namespace slope {

// Turns a project path into a string. A relative path is taken from the project data folder.
// It takes a path and not a string because path::c_str() is a wchar_t* on Windows.
inline std::string formatPath(const path& p) {
    if (p.is_absolute()) return p.string();
    return Options::ProjectDataPath + p.string();
}

// Cleans a folder path and ends it with a separator, because Options paths are followed by file names.
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

// Runs a shell command and returns its exit code.
// On Windows the command is quoted again because cmd.exe /c removes the outer quotes.
inline int runCommand(const std::string& cmd) {
#ifdef _WIN32
    return std::system(("\"" + cmd + "\"").c_str());
#else
    return std::system(cmd.c_str());
#endif
}

namespace io {

// True when the path exists.
inline bool file_exists(const path& filename) {
    return std::filesystem::exists(filename);
}
// True when the path exists. It does not check that it is a folder.
inline bool folder_exists(const path& filename) {
    return std::filesystem::exists(filename);
}

template <class T>
using Matrix = Eigen::Matrix<T, -1, -1>;

template <class T>
using Vector = Eigen::Vector<T, -1>;

// Writes a matrix as comma separated values, one row per line.
// Adapted from https://aleksandarhaber.com/eigen-matrix-library-c-tutorial-saving-and-loading-data-in-from-a-csv-file/
template <class T>
void SaveMatrix(std::string fileName, const Matrix<T>& M) {
    //https://eigen.tuxfamily.org/dox/structEigen_1_1IOFormat.html
    const static Eigen::IOFormat CSVFormat(Eigen::FullPrecision, Eigen::DontAlignCols, ", ", "\n");

    std::ofstream file(fileName);
    if (file.is_open()) {
        file << M.format(CSVFormat);
        file.close();
    }
}

// Writes a vector as comma separated values.
template <class T>
void SaveVec(std::string fileName, const Vector<T>& V) {
    //https://eigen.tuxfamily.org/dox/structEigen_1_1IOFormat.html
    const static Eigen::IOFormat CSVFormat(Eigen::FullPrecision, Eigen::DontAlignCols, ", ", "\n");

    std::ofstream file(fileName);
    if (file.is_open()) {
        file << V.format(CSVFormat);
        file.close();
    }
}

// Reads a matrix from comma separated values. Throws if the file is missing, empty or ragged.
template <typename T>
Matrix<T> LoadMatrix(std::string fileToOpen) {
    std::vector<T> matrixEntries;
    std::ifstream matrixDataFile(fileToOpen);
    if (!matrixDataFile)
        throw std::runtime_error("cannot open matrix file \"" + fileToOpen + "\"");
    std::string matrixRowString;
    std::string matrixEntry;
    int matrixRowNumber = 0;
    size_t columns = 0;

    while (std::getline(matrixDataFile, matrixRowString)) // here we read a row by row of matrixDataFile and store every line into the std::string variable matrixRowString
    {
        if (matrixRowString.find_first_not_of(" \t\r") == std::string::npos)
            continue;
        const size_t before = matrixEntries.size();
        std::stringstream matrixRowStringStream(matrixRowString);     //convert matrixRowString that is a std::string to a stream variable.
        while (std::getline(matrixRowStringStream, matrixEntry, ',')) // here we read pieces of the stream matrixRowStringStream until every comma, and store the resulting character into the matrixEntry
        {
            try {
                matrixEntries.push_back(std::stod(matrixEntry));
            } catch (const std::exception&) {
                throw std::runtime_error("matrix file \"" + fileToOpen + "\": \"" + matrixEntry + "\" is not a number");
            }
        }
        matrixRowNumber++; //update the column numbers
        const size_t n = matrixEntries.size() - before;
        if (matrixRowNumber == 1)
            columns = n;
        else if (n != columns)
            throw std::runtime_error("matrix file \"" + fileToOpen + "\": row " + std::to_string(matrixRowNumber) + " has " + std::to_string(n) + " entries, the first has " + std::to_string(columns));
    }
    if (matrixEntries.empty())
        throw std::runtime_error("matrix file \"" + fileToOpen + "\" is empty");
    return Eigen::Map<Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>>(matrixEntries.data(), matrixRowNumber, matrixEntries.size() / matrixRowNumber);
}

// Reads a vector from a file with one column.
template <typename T>
Vector<T> LoadVec(std::string fileToOpen) {
    return LoadMatrix<T>(fileToOpen);
}

// Loads M from the file if it exists and returns true, otherwise leaves M unchanged.
template <class T>
inline bool MatrixCache(std::string file, Matrix<T>& M) {
    if (file_exists(file)) {
        M = LoadMatrix<T>(file);
        return true;
    }
    return false;
}

// Loads V from the file if it exists and returns true, otherwise leaves V unchanged.
template <class T>
inline bool VecCache(std::string file, Vector<T>& V) {
    if (file_exists(file)) {
        V = LoadMatrix<T>(file);
        return true;
    }
    return false;
}

// Full paths of the entries of a folder, sorted by default.
inline std::vector<std::string> list_directory(std::string folder, bool sorted = true) {
    std::vector<std::string> ls;
    namespace fs = std::filesystem;
    for (const auto& entry : fs::directory_iterator(folder))
        ls.push_back(entry.path().string());
    if (sorted)
        std::sort(ls.begin(), ls.end());
    return ls;
}

} // namespace io

} // namespace slope

#endif // IO_H
