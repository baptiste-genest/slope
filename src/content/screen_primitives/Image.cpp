#include "Image.h"

size_t slope::Image::count = 0;
std::vector<slope::Image> slope::Image::images;

//#define STB_IMAGE_IMPLEMENTATION
#include "../../extern/stb_image.h"

#include <spdlog/spdlog.h>
#include <fmt/core.h>
#include "extern/stb_image.h"

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

std::vector<unsigned char> slope::areaReduceRGBA(const unsigned char *src, int sw, int sh, int dw, int dh)
{
    std::vector<unsigned char> out(size_t(dw)*size_t(dh)*4);
    const double rx = double(sw)/dw, ry = double(sh)/dh;

    for (int y = 0; y < dh; y++){
        const double y0 = y*ry, y1 = (y+1)*ry;
        const int iy0 = int(y0), iy1 = std::min(sh-1,int(std::ceil(y1))-1);
        for (int x = 0; x < dw; x++){
            const double x0 = x*rx, x1 = (x+1)*rx;
            const int ix0 = int(x0), ix1 = std::min(sw-1,int(std::ceil(x1))-1);

            double acc[4] = {0,0,0,0}, wsum = 0;
            for (int sy = iy0; sy <= iy1; sy++){
                const double wy = std::min<double>(sy+1,y1) - std::max<double>(sy,y0);
                if (wy <= 0) continue;
                for (int sx = ix0; sx <= ix1; sx++){
                    const double wx = std::min<double>(sx+1,x1) - std::max<double>(sx,x0);
                    if (wx <= 0) continue;
                    const unsigned char* p = src + (size_t(sy)*sw + sx)*4;
                    const double w = wx*wy, a = p[3]/255.0;
                    acc[0] += w*p[0]*a; acc[1] += w*p[1]*a; acc[2] += w*p[2]*a;
                    acc[3] += w*p[3];
                    wsum   += w;
                }
            }
            unsigned char* q = out.data() + (size_t(y)*dw + x)*4;
            if (wsum <= 0){ q[0]=q[1]=q[2]=q[3]=0; continue; }
            q[3] = (unsigned char)std::lround(std::clamp(acc[3]/wsum,0.,255.));
            // back out of premultiplied space, the weights cancel with acc[3]
            const double unpre = acc[3] > 0 ? 255.0/acc[3] : 0;
            for (int c = 0; c < 3; c++)
                q[c] = (unsigned char)std::lround(std::clamp(acc[c]*unpre,0.,255.));
        }
    }
    return out;
}

slope::ImageData slope::loadImage(path file, double xscale, double yscale)
{
    if (xscale >= 1 && yscale >= 1)
        return loadImage(file);

    std::string filename = file.string();
    int w,h;
    unsigned char* image_data = stbi_load(filename.c_str(), &w, &h, NULL, 4);
    if (image_data == NULL){
        spdlog::error("[image] couldn't load image {}", filename);
        throw std::runtime_error("could not load image " + filename);
    }

    const int dw = std::max(1,(int)std::lround(w*std::min(1.,xscale)));
    const int dh = std::max(1,(int)std::lround(h*std::min(1.,yscale)));
    auto reduced = areaReduceRGBA(image_data,w,h,dw,dh);
    stbi_image_free(image_data);

    ImageData data;
    // the logical size stays the source one, so placement and baseline maths
    // are untouched ; only the texel count follows the screen
    data.width  = w;
    data.height = h;

    glGenTextures(1, &data.texture);
    glBindTexture(GL_TEXTURE_2D, data.texture);
    // mipmaps only matter once a primitive is zoomed out past its stored size
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#if defined(GL_UNPACK_ROW_LENGTH) && !defined(__EMSCRIPTEN__)
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, dw, dh, 0, GL_RGBA, GL_UNSIGNED_BYTE, reduced.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    return data;
}

slope::ImageData slope::loadImage(path file)
{
    // path::c_str() is wchar_t* on Windows, stbi_load needs the string
    std::string filename = file.string();
    int w,h;
    // Load from file
    unsigned char* image_data = stbi_load(filename.c_str(), &w, &h, NULL, 4);
    if (image_data == NULL){
        spdlog::error("[image] couldn't load image {}", filename);
        throw std::runtime_error("could not load image " + filename);
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

slope::Image::ImagePtr slope::Image::Blank(int w, int h)
{
    auto img = NewPrimitive<Image>();
    img->updateImage(nullptr,w,h);
    return img;
}

// an Image owns its texture alone, so the old one goes when the size changes
void slope::Image::updateImage(const unsigned char *rgba, int w, int h)
{
    if (w <= 0 || h <= 0)
        return;

    if (data.texture == 0 || data.width != w || data.height != h){
        if (data.texture && glfwGetCurrentContext())
            glDeleteTextures(1,&data.texture);
        glGenTextures(1,&data.texture);
        glBindTexture(GL_TEXTURE_2D,data.texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        data.width  = w;
        data.height = h;
        owns_texture = true;

        std::vector<unsigned char> blank;
        if (!rgba)
            blank.assign(size_t(w)*size_t(h)*4,0);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,
                     rgba ? rgba : blank.data());
        return;
    }

    if (!rgba)
        return;
    glBindTexture(GL_TEXTURE_2D,data.texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT,4);
    glPixelStorei(GL_UNPACK_ROW_LENGTH,0);
    glTexSubImage2D(GL_TEXTURE_2D,0,0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,rgba);
}

void slope::Image::updateImage(const std::string &file)
{
    try {
        ImageData fresh = loadImage(formatPath(file));
        if (data.texture && glfwGetCurrentContext())
            glDeleteTextures(1,&data.texture);
        data = fresh;
        owns_texture = true;
    } catch (const std::exception& e) {
        spdlog::error("[image] {}",e.what());   // never mid frame, never fatal
    }
}

// primitives outlive the window, deleting a texture then hits a dead context
slope::Image::~Image()
{
    if (owns_texture && data.texture && glfwGetCurrentContext())
        glDeleteTextures(1,&data.texture);
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
    // an unrotated quad is a blit, and a half pixel offset would have the
    // sampler resample a texture that already matches the screen
    if (std::abs(angle) < 1e-4){
        size = ImVec2(std::round(size.x),std::round(size.y));
        center = ImVec2(std::round(center.x - size.x*0.5f) + size.x*0.5f,
                        std::round(center.y - size.y*0.5f) + size.y*0.5f);
    }

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

void slope::DisplayImage(const ImageData &data, const StateInSlide &sis, scalar scale, const RGBA& tint, scalar y_offset)
{
    RGBA color_multiplier = ImColor(tint.Value.x,tint.Value.y,tint.Value.z,tint.Value.w*sis.getAlpha());
    auto P = sis.getAbsolutePosition();
    P.y += y_offset;

    bool notfullHD = (Options::ScreenResolutionWidth != 1920) ||(Options::ScreenResolutionHeight != 1080);

    if (std::abs(sis.getAngle()) > 0.001 || std::abs(1-scale) > 1e-2 || notfullHD){
        double sx =  Options::ScreenResolutionWidth/1920.;
        double sy =  Options::ScreenResolutionHeight/1080.;
        ImageRotated((intptr_t)data.texture,P,ImVec2(sx*data.width*scale,sy*data.height*scale),sis.getAngle(),color_multiplier);
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
    display(sis);
    upframe(t);
}

void slope::Gif::playOutro(const TimeObject &t, const StateInSlide &sis)
{
    display(sis);
    upframe(t);
}

slope::Primitive::Size slope::Gif::getSize() const {
    return Image::getScaledSize(images[current_img],scale);
}

std::vector<slope::ImageData> slope::loadGif(path filename)
{
    auto H = std::to_string(std::hash<std::string>{}(filename.string()));
    std::vector<slope::ImageData> data;
    std::string folder = slope::Options::CachePath + H;
    if (!io::folder_exists(folder) || Options::ignore_cache){
        spdlog::info("Decomposing gif " + filename.string());
        // std::filesystem instead of rm/mkdir, and the configured ImageMagick
        std::error_code ec;
        std::filesystem::remove_all(folder, ec);
        std::filesystem::create_directories(folder, ec);
        runCommand(fmt::format("\"{}\" \"{}\" -coalesce \"{}\"",
                               Options::PathToCONVERT, filename.string(),
                               (path(folder) / "gif_%05d.png").string()));
    }
    auto images = io::list_directory(folder);
    for (auto& f : images){
        data.push_back(loadImage(f));
    }
    return data;
}
