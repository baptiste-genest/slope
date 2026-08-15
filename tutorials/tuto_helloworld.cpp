#include "polyscope/polyscope.h"
#include "../../src/slope.h"


slope::Slideshow show;

int main(int argc,char** argv)
{
  show.init("tutorials",argc,argv);
  
  //Slideshow title
  show << slope::Latex::Add("Hello World!",slope::Options::TitleScale)->at(slope::CENTER);
  
  //New slide
  auto title = slope::Title("The beauty of $\\pi$ ")->at(slope::TOP);
  show << slope::newFrame << title;
  
  //Plain latex forumla
  auto formula = slope::Formula::Add("\\pi = 4 \\int_0^1 \\frac{1}{1+x^2}dx");
  show << formula;
  
  polyscope::state::userCallback = [](){
    show.play();
  };
  polyscope::show();
  return 0;
}
