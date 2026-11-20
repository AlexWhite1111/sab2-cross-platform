#include "sab2/core/tracker.h"
#include "sab2/core/controller.h"
#include "sab2/platform/input.h"
#ifdef SAB2_WITH_OPENCV
#include "sab2/vision/opencv_pipeline.h"
#include <opencv2/opencv.hpp>
#endif
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

using namespace sab2;
namespace {
void help(){
 std::cout << "SAB2 Cross-Platform Foundation (not the upstream SAB2 binary)\n"
              "Options:\n"
              "  --demo               Synthetic moving-target test (default)\n"
              "  --video FILE         Process video file (requires OpenCV)\n"
              "  --camera INDEX       Read a capture-card or webcam feed (requires OpenCV)\n"
              "  --onnx FILE          ONNX detection model, YOLOv8/YOLO11 detection layout\n"
              "  --input-size N       Model input side, default 640\n"
              "  --frames N           Max frames in synthetic/video mode, default 120\n"
              "  --no-gui             Do not display OpenCV preview window\n"
              "  --enable-input       Opt-in OS pointer movement; dry-run otherwise\n"
              "  --help               Print this help\n";
}
struct Options {std::string video,onnx;int camera=-1,frames=120,input_size=640;bool gui=true,enable_input=false;};
Options parse(int argc,char**argv){
 Options opts;
 for(int i=1;i<argc;i++){
  std::string a=argv[i];
  auto next=[&]()->std::string {if(i+1>=argc)throw std::invalid_argument("Missing value for "+a);return argv[++i];};
  if(a=="--help") {help();std::exit(0);}
  else if(a=="--demo") {opts.video="";opts.camera=-1;}
  else if(a=="--video") {opts.video=next();opts.camera=-1;}
  else if(a=="--camera") {opts.camera=std::stoi(next());opts.video="";}
  else if(a=="--onnx")opts.onnx=next();
  else if(a=="--input-size")opts.input_size=std::stoi(next());
  else if(a=="--frames")opts.frames=std::stoi(next());
  else if(a=="--no-gui")opts.gui=false;
  else if(a=="--enable-input")opts.enable_input=true;
  else throw std::invalid_argument("Unknown option: "+a);
 }
 if(opts.frames<=0||opts.input_size<64||opts.input_size>2048)throw std::invalid_argument("Invalid numeric argument");
 return opts;
}
void print(int frame,Point center,Track track,Motion motion){
 std::cout<<frame<<","<<std::fixed<<std::setprecision(2)<<center.x<<","<<center.y<<","
          <<track.id<<","<<int(track.valid)<<","<<int(track.observed)<<","
          <<track.center.x<<","<<track.center.y<<","<<motion.dx<<","<<motion.dy<<"\n";
}
}
int main(int argc,char**argv){
 try {
  auto opts=parse(argc,argv);
  auto output=create_input_sink(opts.enable_input);
  SingleTargetTracker tracker;ProportionalController controller;
  std::cerr << "Input sink: " << output->name() << "\n";
  std::cout << "frame,origin_x,origin_y,target_id,valid,observed,target_x,target_y,delta_x,delta_y\n";
#ifdef SAB2_WITH_OPENCV
  if(opts.camera>=0 || !opts.video.empty()) {
    cv::VideoCapture source;
    if(opts.camera>=0)source.open(opts.camera);
    else source.open(opts.video);
    if(!source.isOpened())throw std::runtime_error("Cannot open video/camera source");
    VisionDetector detector(opts.onnx,opts.input_size);
    std::cerr<<"Detector: "<<(detector.is_onnx()?"ONNX (OpenCV DNN CPU)":"green marker threshold")<<"\n";
    auto previous=std::chrono::steady_clock::now();
    for(int f=0;f<opts.frames;f++){
      cv::Mat image;
      if(!source.read(image)||image.empty())break;
      auto now=std::chrono::steady_clock::now();
      double dt=std::chrono::duration<double>(now-previous).count();previous=now;
      if(f==0||dt<0.001)dt=1.0/60.0;
      const Point origin{image.cols/2.0,image.rows/2.0};
      auto objects=detector.detect(image);
      const auto track=tracker.update(objects,dt,origin);
      const auto motion=controller.update(origin,track);
      if(opts.enable_input && track.valid)output->move(motion);
      print(f,origin,track,motion);
      if(opts.gui){
        for(const auto& d:objects) {
          cv::Rect b(cvRound(d.center.x-d.width/2),cvRound(d.center.y-d.height/2),cvRound(d.width),cvRound(d.height));
          cv::rectangle(image,b,cv::Scalar(30,220,30),2);
        }
        cv::drawMarker(image,cv::Point(cvRound(origin.x),cvRound(origin.y)),cv::Scalar(255,255,255),cv::MARKER_CROSS,22,1);
        if(track.valid)cv::circle(image,cv::Point(cvRound(track.center.x),cvRound(track.center.y)),8,cv::Scalar(0,255,255),2);
        cv::imshow("SAB2 portable preview (ESC to exit)",image);
        if(cv::waitKey(1)==27)break;
      }
    }
    return 0;
  }
#else
  if(opts.camera>=0 || !opts.video.empty() || !opts.onnx.empty())
   throw std::runtime_error("Video/ONNX mode unavailable: rebuild with OpenCV development libraries");
#endif
  // Deterministic synthetic path used to test the capture-independent core on any platform.
  constexpr Point origin{320,240};
  for(int i=0;i<opts.frames;i++) {
   const double t=i/60.0;
   Detection d{{320+130*std::sin(0.9*t),240+70*std::cos(1.1*t)},35,70,0.95,0};
   std::vector<Detection> objects;
   if(i%53!=50)objects.push_back(d); // drop a frame to test hold behavior
   const auto track=tracker.update(objects,1.0/60.0,origin);
   const auto motion=controller.update(origin,track);
   if(opts.enable_input&&track.valid)output->move(motion);
   print(i,origin,track,motion);
  }
  return 0;
 }catch(const std::exception& e){std::cerr<<"ERROR: "<<e.what()<<"\n";return 2;}
}
