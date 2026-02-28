#include "Referential.h"


/*
void slope::PersistentPosition::setLabel(std::string l) {
    label = l;
    writeAtLabel(0.5,0.5,false);
    readFromLabel();
}

void slope::PersistentPosition::writeAtLabel(double x, double y,bool overwrite) const
{
    if (label == "")
        return;
    system(("mkdir " + slope::Options::ProjectViewsPath + " 2>/dev/null").c_str());
    std::string filepath = slope::Options::ProjectViewsPath + label + ".pos";
    if (!io::file_exists(filepath) || overwrite){
        std::ofstream file(filepath);
        if (!file.is_open()){
            std::cerr << "couldn't open file" << std::endl;
            exit(1);
        }
        file << x << " " << y << std::endl;
    }
}

slope::vec2 slope::PersistentPosition::readFromLabel() const
{
    std::ifstream file (slope::Options::ProjectViewsPath + label + ".pos");
    if (!file.is_open()){
        std::cerr << "couldn't read label file" << std::endl;
        exit(1);
    }
    vec2 rslt;
    file >> rslt(0) >> rslt(1);
    return rslt;
}

*/
