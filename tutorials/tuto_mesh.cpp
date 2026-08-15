#include <polyscope/polyscope.h>
#include "../../src/slope.h"


slope::Slideshow show;


slope::vec f_scale(slope::vec x) {
  return x*0.5;
}

int main(int argc,char** argv)
{
  //Init
  show.init("tutorials",argc,argv);
  
  //Slideshow title
  show << slope::Latex::Add("Hello 3D World!",slope::Options::TitleScale)->at(slope::CENTER);
  
  //New slide
  auto title = slope::Title("The Bunny")->at(slope::TOP);
  show << slope::newFrame << title;
  
  //Loading a 3d object
  auto bunny = slope::Mesh::Add(slope::Options::ProjectDataPath + "meshes/bunny.obj");
  show << bunny;
  
  // Adding the bunny again but scaled down
  show <<slope::newFrame 
       << slope::Title("The Bunny (scaled down when loaded)")->at(slope::TOP)
       << slope::Mesh::Add(slope::Options::ProjectDataPath + "meshes/bunny.obj", 0.5);
  
  //Using apply()
  //Note that a new instance of the object is created
  show <<slope::newFrame << slope::Title("The Bunny (apply)")->at(slope::TOP)
       << bunny->apply(f_scale,false);
  
  auto lambda_scale = [](const slope::vec &v) { return slope::vec(v*0.5 + slope::vec(1.,0.,0.));};
  show << bunny->apply(lambda_scale,false);
  
  
  show <<slope::newFrame << slope::Title("The Bunny with quantities")->at(slope::TOP) ;
  auto bunnyred =  bunny->apply(lambda_scale,false);
  auto lambda_x = [](const slope::vec &v) { return sin(v(0));};
  auto vals = bunnyred->eval(lambda_x);
  bunnyred->setSmooth(true);
  show << bunnyred;
  show << slope::AddPolyscopeQuantity( bunnyred->pc->addVertexScalarQuantity("posX", vals)->setColorMap("jet") ) ;
  show << slope::Latex::Add("$\\sqrt{2}$");
  show << slope::PlaceBelow(slope::Latex::Add("$\\sqrt{3}$"));
  show << slope::PlaceBelow(slope::Latex::Add("$\\sqrt{5}$"));

  polyscope::state::userCallback = [](){
    show.play();
  };
  polyscope::show();
  return 0;
}

