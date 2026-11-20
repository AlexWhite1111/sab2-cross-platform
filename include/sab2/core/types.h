#pragma once
#include <cstdint>
#include <vector>
namespace sab2 {
struct Point { double x=0.0; double y=0.0; };
struct Detection { Point center; double width=0.0; double height=0.0; double score=0.0; int class_id=0; };
struct Track { Point center; Point velocity; int id=-1; bool valid=false; bool observed=false; double age=0.0; };
struct Motion { double dx=0.0; double dy=0.0; };
}
