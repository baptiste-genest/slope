#ifndef SCREENSHOT_H
#define SCREENSHOT_H


#ifdef __APPLE__
#include <string>
#include <spdlog/spdlog.h>
#else
#include "common.hpp"
#endif //__APPLE__

namespace slope {

  // Saves the content of the window to an image file.
  void screenshot(std::string file);
}
#endif // SCREENSHOT_H
