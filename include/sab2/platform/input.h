#pragma once
#include "sab2/core/types.h"
#include <memory>
#include <string>
namespace sab2 {
class InputSink {
public:
  virtual ~InputSink()=default;
  virtual bool move(Motion motion)=0;
  virtual const char* name() const=0;
};
// A dry-run sink is always the default; real input requires an explicit CLI flag.
std::unique_ptr<InputSink> create_input_sink(bool enable_os_mouse);
}
