#include "sab2/core/tracker.h"
#include "sab2/core/controller.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>
using namespace sab2;
static bool near(double a,double b,double e=1e-6){return std::abs(a-b)<e;}
int main(){
 SingleTargetTracker tracker;
 Point center{320,240};
 auto a=tracker.update({{{350,250},20,20,0.9,0}},0.016,center);
 assert(a.valid&&a.observed&&a.id==1&&near(a.center.x,350));
 auto b=tracker.update({{{357,250},20,20,0.91,0},{{700,500},20,20,0.99,0}},0.016,center);
 assert(b.valid&&b.id==1&&near(b.center.x,357));
 auto predicted=tracker.update({},0.016,center);
 assert(predicted.valid&&!predicted.observed&&predicted.id==1);
 for(int i=0;i<15;i++)predicted=tracker.update({},0.016,center);
 assert(!predicted.valid);
 auto fresh=tracker.update({{{300,230},20,20,0.99,0}},0.016,center);
 assert(fresh.valid&&fresh.id==2);
 ProportionalController controller({0.9,20.0,1.5,0.0});
 Motion result=controller.update(center,{{820,540},{},3,true,true,0});
 assert(std::hypot(result.dx,result.dy)<=20.0001);
 Motion none=controller.update(center,Track{});
 assert(near(none.dx,0)&&near(none.dy,0));
 Motion close=controller.update(center,{{320.1,240.1},{},3,true,true,0});
 assert(near(close.dx,0)&&near(close.dy,0));
 std::cout<<"core tests passed\n";
}
