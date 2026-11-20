#import <ApplicationServices/ApplicationServices.h>
#include "sab2/platform/input.h"
#include <memory>
namespace sab2 {
class MacInput final : public InputSink {
public:
 bool move(Motion m) override {
   // Quartz provides pointer movement, NOT raw HID injection. Many games use raw input.
   CGEventRef event=CGEventCreate(nullptr);
   if(!event)return false;
   CGPoint at=CGEventGetLocation(event);
   CFRelease(event);
   CGPoint to=CGPointMake(at.x+m.dx,at.y+m.dy);
   CGEventRef mouse=CGEventCreateMouseEvent(nullptr,kCGEventMouseMoved,to,kCGMouseButtonLeft);
   if(!mouse)return false;
   CGEventPost(kCGHIDEventTap,mouse);CFRelease(mouse);
   return true;
 }
 const char* name()const override{return "macOS Quartz (Accessibility permission required)";}
};
class DryRunInput final : public InputSink {
public:bool move(Motion)override{return true;}const char* name()const override{return "dry-run";}
};
std::unique_ptr<InputSink> create_input_sink(bool on) {
 if(on)return std::make_unique<MacInput>();return std::make_unique<DryRunInput>();
}
}
