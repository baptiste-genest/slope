#include "polyscope/polyscope.h"
#include "../../src/slope.h"

using namespace slope;

Slideshow show;

void init() {
  //Slideshow title
  show << Title("Hello World!")->at(slope::CENTER);
  
  //New slide
  auto title = Title("The beauty of $\\pi$ ")->at(slope::TOP);
  show << newFrame << title;
}

int main(int argc,char** argv)
{
  show.init("slope_PROJECT_NAME",argc,argv);
  
  init();
  
  polyscope::state::userCallback = [](){
    show.play();
  };
  polyscope::show();
  return 0;
}
