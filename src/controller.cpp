#include "sab2/core/controller.h"
#include <algorithm>
#include <cmath>
namespace sab2 {
ProportionalController::ProportionalController(ControllerSettings s):settings_(s){}
void ProportionalController::reset(){previous_={};}
Motion ProportionalController::update(Point origin,const Track& track){
  if(!track.valid){reset();return {};}
  const double dx=track.center.x-origin.x,dy=track.center.y-origin.y;
  const double dist=std::hypot(dx,dy);
  if(dist<=settings_.deadzone){reset();return {};}
  const double maxstep=std::max(0.0,settings_.max_pixels);
  const double factor=std::min(std::max(0.0,settings_.gain),maxstep/dist);
  const Motion raw{dx*factor,dy*factor};
  const double alpha=std::clamp(settings_.smoothing,0.0,1.0);
  Motion next{alpha*previous_.dx+(1-alpha)*raw.dx,alpha*previous_.dy+(1-alpha)*raw.dy};
  const double m=std::hypot(next.dx,next.dy);
  if(m>maxstep&&m>0){next.dx*=maxstep/m;next.dy*=maxstep/m;}
  return previous_=next;
}
}
