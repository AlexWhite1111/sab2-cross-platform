#pragma once
#include "sab2/core/types.h"
#include <vector>
namespace sab2 {
struct TrackerSettings { double gate_pixels=180.0; double smoothing=0.45; double hold_seconds=0.10; double min_score=0.35; };
class SingleTargetTracker {
public:
  explicit SingleTargetTracker(TrackerSettings settings = {});
  Track update(const std::vector<Detection>& observations, double dt, Point aim_origin);
  void reset();
private:
  TrackerSettings settings_;
  Track last_{};
  int next_id_=1;
};
}
