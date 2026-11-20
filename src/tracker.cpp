#include "sab2/core/tracker.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace sab2 {
SingleTargetTracker::SingleTargetTracker(TrackerSettings s) : settings_(s) {}
void SingleTargetTracker::reset() { last_={}; next_id_=1; }
Track SingleTargetTracker::update(const std::vector<Detection>& objects, double dt, Point origin) {
  dt=std::clamp(dt,0.001,0.25);
  Point expected=last_.valid ? Point{last_.center.x+last_.velocity.x*dt,last_.center.y+last_.velocity.y*dt} : origin;
  double best=std::numeric_limits<double>::infinity();
  const Detection* chosen=nullptr;
  for(const auto& d : objects) {
    if(d.score<settings_.min_score || !std::isfinite(d.center.x) || !std::isfinite(d.center.y)) continue;
    const double distance=std::hypot(d.center.x-expected.x,d.center.y-expected.y);
    if(last_.valid && distance>settings_.gate_pixels) continue;
    const double cost=distance - 8.0*d.score; // confidence only breaks near-ties
    if(cost<best) { best=cost;chosen=&d; }
  }
  if(chosen) {
    Track out;
    out.valid=true;out.observed=true;out.age=0;
    out.id=last_.valid ? last_.id : next_id_++;
    out.center=chosen->center;
    if(last_.valid) {
      const Point estimate{(out.center.x-last_.center.x)/dt,(out.center.y-last_.center.y)/dt};
      const double a=std::clamp(settings_.smoothing,0.0,1.0);
      out.velocity={a*last_.velocity.x+(1-a)*estimate.x,a*last_.velocity.y+(1-a)*estimate.y};
    }
    return last_=out;
  }
  if(last_.valid && last_.age+dt<=settings_.hold_seconds) {
    last_.center=expected;last_.age+=dt;last_.observed=false;
    return last_;
  }
  last_={};return last_;
}
}
