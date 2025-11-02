#ifndef SCREENSHOT_H
#define SCREENSHOT_H


#ifdef __APPLE__
#include <string>
#else
#include "../PCH.h"
#endif //__APPLE__

namespace slope {
  
  void screenshot(std::string file);
}
#endif // SCREENSHOT_H
