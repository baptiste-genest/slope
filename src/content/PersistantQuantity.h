#ifndef PERSISTANTQUANTITY_H
#define PERSISTANTQUANTITY_H

#include "../slope.h"


namespace slope {

template<class T>
void WriteAtFile(const std::string& path,const T& value) {
    std::ofstream file(path);
    if (!file.is_open()){
        throw std::runtime_error("could not open file " + path);
    }
    file << value;
}

template<class T>
T ReadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()){
        throw std::runtime_error("could not open file " + path);
    }
    T value;
    file >> value;
    return value;
}



template<class T>
class PersistantQuantity
{
    std::string label;

public:

    bool isActive() const {
        return !label.empty();
    }

    PersistantQuantity(std::string label);

    std::string getPath() const {
        std::string type_name = typeid(T).name();
        return Options::ProjectViewsPath + label + "." + type_name;
    }

    void write(const T& value) {
        WriteAtFile<T>(getPath(),value);
    }

    T read() const {
        return ReadFromFile<T>(getPath());
    }
};

}

#endif // PERSISTANTQUANTITY_H
