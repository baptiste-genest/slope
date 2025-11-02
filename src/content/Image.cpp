#include "Image.h"

size_t slope::Image::count = 0;
std::vector<slope::Image> slope::Image::images;

//#define STB_IMAGE_IMPLEMENTATION
#include "../extern/stb_image.h"

#include <spdlog/spdlog.h>

slope::Image::ImagePtr slope::Image::Add(std::string file,scalar scale)
{
    ImagePtr rslt = NewPrimitive<Image>();
    rslt->data = loadImage(formatPath(file));
    rslt->scale = scale;
    return rslt;
}

ImVec2 slope::Image::getSize(std::string filename)
{
    int w,h;
    // Load from file
    unsigned char* image_data = stbi_load(filename.c_str(), &w, &h, NULL, 4);
    return ImVec2(w,h);
}

slope::Primitive::Size slope::Image::getScaledSize(const ImageData &data, scalar scale)
{
    double sx = scale,sy = scale;
    bool notFullHD = (Options::ScreenResolutionWidth != 1920) ||(Options::ScreenResolutionHeight != 1080);
    if (notFullHD){
        sx *=  Options::ScreenResolutionWidth/1920.;
        sy *=  Options::ScreenResolutionHeight/1080.;
    }
    return Size(sx*data.width,sy*data.height);
}

slope::ImageData slope::loadImage(path file)
{
    auto filename = file.c_str();
    int w,h;
    // Load from file
    unsigned char* image_data = stbi_load(filename, &w, &h, NULL, 4);
    if (image_data == NULL){
        std::cerr << "[image] couldn't load image " << filename << std::endl;
        exit(1);
    }

    ImageData data;
    data.width = w;
    data.height = h;

    // Create a OpenGL texture identifier
    glGenTextures(1, &data.texture);
    glBindTexture(GL_TEXTURE_2D, data.texture);


    // Setup filtering parameters for display
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // This is required on WebGL for non power-of-two textures
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // Same


// Upload pixels into texture
#if defined(GL_UNPACK_ROW_LENGTH) && !defined(__EMSCRIPTEN__)
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, data.width, data.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);
    stbi_image_free(image_data);
    return data;
}

void slope::Image::draw(const TimeObject &, const StateInSlide &sis)
{
    display(sis);
}

void slope::Image::playIntro(const TimeObject& t, const StateInSlide &sis)
{
    display(sis);
}

void slope::Image::playOutro(const TimeObject& t, const StateInSlide &sis)
{
    display(sis);
}

slope::Primitive::Size slope::Image::getSize() const {
    return getScaledSize(data,scale);
}

void slope::Image::display(const StateInSlide &sis) const
{
    anchor->updatePos(sis.getPosition());
    DisplayImage(data,sis,scale*sis.getScale());
}

void slope::ImageRotated(ImTextureID tex_id, ImVec2 center, ImVec2 size, float angle, const RGBA &color_mult)
{
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    float cos_a = cosf(angle);
    float sin_a = sinf(angle);
    ImVec2 pos[4] =
        {
            center + ImRotate(ImVec2(-size.x * 0.5f, -size.y * 0.5f), cos_a, sin_a),
            center + ImRotate(ImVec2(+size.x * 0.5f, -size.y * 0.5f), cos_a, sin_a),
            center + ImRotate(ImVec2(+size.x * 0.5f, +size.y * 0.5f), cos_a, sin_a),
            center + ImRotate(ImVec2(-size.x * 0.5f, +size.y * 0.5f), cos_a, sin_a)
        };
    ImVec2 uvs[4] =
        {
            ImVec2(0.0f, 0.0f),
            ImVec2(1.0f, 0.0f),
            ImVec2(1.0f, 1.0f),
            ImVec2(0.0f, 1.0f)
        };

    draw_list->AddImageQuad(tex_id, pos[0], pos[1], pos[2], pos[3], uvs[0], uvs[1], uvs[2], uvs[3], color_mult);
}

void slope::DisplayImage(const ImageData &data, const StateInSlide &sis, scalar scale)
{
    RGBA color_multiplier = ImColor(1.f,1.f,1.f,sis.alpha);
    auto P = sis.getAbsolutePosition();

    bool notfullHD = (Options::ScreenResolutionWidth != 1920) ||(Options::ScreenResolutionHeight != 1080);

    if (std::abs(sis.angle) > 0.001 || std::abs(1-scale) > 1e-2 || notfullHD){
        double sx =  Options::ScreenResolutionWidth/1920.;
        double sy =  Options::ScreenResolutionHeight/1080.;
        ImageRotated((intptr_t)data.texture,P,ImVec2(sx*data.width*scale,sy*data.height*scale),sis.angle,color_multiplier);
    }
    else {
        P.x -= data.width*0.5*scale;
        P.y -= data.height*0.5*scale;
        ImGui::SetCursorPos(P);
        ImGui::ImageWithBg(data.texture, ImVec2(data.width*scale,data.height*scale), ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f),ImVec4(0, 0, 0, 0), color_multiplier);
    }
}

slope::Gif::GifPtr slope::Gif::Add(std::string filename,int fps,scalar scale,bool loop)
{
    auto data = loadGif(formatPath(filename));
    GifPtr rslt = NewPrimitive<Gif>(data,fps,scale,loop);
    return rslt;
}

slope::Gif::Gif(const std::vector<ImageData> &images, int fps, scalar scale, bool loop) : images(images),fps(fps),scale(scale),loop(loop) {}

bool slope::Gif::isValid() {return images[0].width != -1;}

void slope::Gif::display(const StateInSlide &sis) const {
    anchor->updatePos(sis.getPosition());
    DisplayImage(images[current_img],sis,scale*sis.getScale());
}

void slope::Gif::draw(const TimeObject &t, const StateInSlide &sis)
{
    display(sis);
    upframe(t);
}

void slope::Gif::playIntro(const TimeObject &t, const StateInSlide &sis) {
    auto sist = sis;
    sist.alpha = smoothstep(t.transitionParameter)*sis.alpha;
    display(sist);
    upframe(t);
}

void slope::Gif::playOutro(const TimeObject &t, const StateInSlide &sis)
{
    auto sist = sis;
    sist.alpha = smoothstep(1-t.transitionParameter)*sis.alpha;
    display(sist);
    upframe(t);
}

slope::Primitive::Size slope::Gif::getSize() const {
    return Image::getScaledSize(images[current_img],scale);
}

std::vector<slope::ImageData> slope::loadGif(path filename)
{
    auto H = std::to_string(std::hash<std::string>{}(filename));
    std::vector<slope::ImageData> data;
    std::string folder = slope::Options::ProjectDataPath + "cache/" + H;
    if (!io::folder_exists(folder) || Options::ignore_cache){
        spdlog::info("Decomposing gif " + filename.string());
        system(("rm -rf " + folder + " 2> /dev/null").data());
        system(("mkdir " + folder).data());
        system(("convert "+filename.string()+" -coalesce " + folder + "/gif_%05d.png").data());
    }
    auto images = io::list_directory(folder);
    for (auto& f : images){
        data.push_back(loadImage(f));
    }
    return data;
}
