#include "sab2/platform/input.h"
#include <memory>
namespace sab2 {
class DryRunInput final:public InputSink {
public:
 bool move(Motion) override{return true;}
 const char* name()const override{return "dry-run";}
};
std::unique_ptr<InputSink> create_input_sink(bool){return std::make_unique<DryRunInput>();}
}
