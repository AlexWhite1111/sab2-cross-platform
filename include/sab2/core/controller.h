#pragma once
#include "sab2/core/types.h"
namespace sab2 {
struct ControllerSettings {
  double gain=0.45;       // dimensionless proportional response per update
  double max_pixels=32.0; // maximum motion each update, not per second
  double deadzone=1.5;
  double smoothing=0.55;  // EMA retention of previous command
};
class ProportionalController {
public:
  explicit ProportionalController(ControllerSettings settings = {});
  Motion update(Point origin, const Track& target);
  void reset();
private:
  ControllerSettings settings_;
  Motion previous_{};
};
}
