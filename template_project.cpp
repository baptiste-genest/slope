#include "slope.h"

using namespace slope;

Slideshow show;

void init() {
  show << Title("Hello World!")->at(CENTER);
}

int main(int argc,char** argv)
{
  show.init("slope_PROJECT_NAME",argc,argv);
  
  init();
  show.run();
  return 0;
}
