#include "screenshot.h"

#ifdef __APPLE__
void slope::screenshot(std::string file)
{
  spdlog::error("not implemented yet for APPLE");
}
#else

Window get_focus_window(Display* d){
    Window w;
    int revert_to;
    XGetInputFocus(d, &w, &revert_to); // see man
    if(w == None){
        printf("no focus window\n");
        exit(1);
    }
    return w;
}
void slope::screenshot(std::string file)
{
    Display* display = XOpenDisplay(nullptr);
    Window root = get_focus_window(display);

    XWindowAttributes attributes = {0};
    XGetWindowAttributes(display, root, &attributes);

    int Width = attributes.width;
    int Height = attributes.height;

    XImage* img = XGetImage(display, root, 0, 0 , Width, Height, AllPlanes, ZPixmap);
    std::vector<char> Pixels(Width * Height * 4);

    memcpy(&Pixels[0], img->data, Pixels.size());

    XDestroyImage(img);
    XCloseDisplay(display);
    std::vector<unsigned char> IMG(Width*Height*3);

    for (int i = 0; i < Height; i++)
        for (int j = 0; j < Width; j++)
            for (int k = 2; k >= 0; k--)
                IMG[(i*Width+j)*3+k] = Pixels[((Height-1-i)*Width+j)*4+(2-k)];
    polyscope::saveImage(file,IMG.data(),Width,Height,3);
}
#endif
