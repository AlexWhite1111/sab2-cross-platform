#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "sab2/platform/input.h"
#include <cmath>
#include <memory>
namespace sab2 {
class WindowsInput final : public InputSink {
public:
 bool move(Motion m) override {
  INPUT event{};event.type=INPUT_MOUSE;event.mi.dwFlags=MOUSEEVENTF_MOVE;
  event.mi.dx=static_cast<LONG>(std::lround(m.dx)); event.mi.dy=static_cast<LONG>(std::lround(m.dy));
  return SendInput(1,&event,sizeof(event))==1;
 }
 const char* name()const override{return "Windows SendInput";}
};
class DryRunInput final : public InputSink {
public:bool move(Motion)override{return true;}const char* name()const override{return "dry-run";}
};
std::unique_ptr<InputSink> create_input_sink(bool on) {
 if(on)return std::make_unique<WindowsInput>();return std::make_unique<DryRunInput>();
}
}
