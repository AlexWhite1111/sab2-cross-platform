#include "sab2/vision/opencv_pipeline.h"
#include <opencv2/imgproc.hpp>
#include <opencv2/dnn.hpp>
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace sab2 {
VisionDetector::VisionDetector(const std::string& onnx,int size):input_size_(size){
  if(!onnx.empty()) {
    net_=cv::dnn::readNetFromONNX(onnx);
    if(net_.empty())throw std::runtime_error("Could not load ONNX model: "+onnx);
    net_.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
    net_.setPreferableTarget(cv::dnn::DNN_TARGET_CPU); // portable baseline; no CoreML claim
    with_model_=true;
  }
}
std::vector<Detection> VisionDetector::detect(const cv::Mat& image,double score){
  if(image.empty())return {};
  if(with_model_)return onnx_detect(image,score);
  return green_marker_detect(image);
}
std::vector<Detection> VisionDetector::green_marker_detect(const cv::Mat& image){
  cv::Mat hsv,mask;cv::cvtColor(image,hsv,cv::COLOR_BGR2HSV);
  cv::inRange(hsv,cv::Scalar(35,90,90),cv::Scalar(85,255,255),mask);
  std::vector<std::vector<cv::Point>> contours;
  cv::findContours(mask,contours,cv::RETR_EXTERNAL,cv::CHAIN_APPROX_SIMPLE);
  std::vector<Detection> found;
  for(const auto& c:contours){
    if(cv::contourArea(c)<50)continue;
    const auto b=cv::boundingRect(c);
    found.push_back({{b.x+b.width/2.0,b.y+b.height/2.0},double(b.width),double(b.height),0.95,0});
  }
  return found;
}
std::vector<Detection> VisionDetector::onnx_detect(const cv::Mat& frame,double threshold){
  // Supports regular YOLOv8/YOLO11 detect ONNX output [1,C,N] or [1,N,C].
  // Other families/export variants MUST have their own decoder; do not assume equivalence.
  cv::Mat resized,blob;
  cv::resize(frame,resized,cv::Size(input_size_,input_size_));
  cv::dnn::blobFromImage(resized,blob,1.0/255.0,cv::Size(input_size_,input_size_),cv::Scalar(),true,false);
  net_.setInput(blob);
  const cv::Mat out=net_.forward();
  if(out.dims!=3||out.type()!=CV_32F||!out.isContinuous())
    throw std::runtime_error("Unsupported ONNX output: expected contiguous [1,C,N] or [1,N,C] float tensor");
  const int a=out.size[1],b=out.size[2];
  const bool channels_first=a<=256 && b>a;
  const int channels=channels_first?a:b;
  const int count=channels_first?b:a;
  if(channels<5||channels>256||count<=0)
    throw std::runtime_error("Unsupported ONNX tensor dimensions");
  const bool objectness=channels==85; // canonical YOLOv5 COCO only
  const int class_start=objectness?5:4;
  std::vector<cv::Rect> boxes;std::vector<float> scores;std::vector<int> labels;
  const float*data=out.ptr<float>();
  auto at=[&](int i,int c)->float{return channels_first?data[c*count+i]:data[i*channels+c];};
  for(int i=0;i<count;i++){
    const float obj=objectness?at(i,4):1.0f;
    int cls=-1;float best=0;
    for(int c=class_start;c<channels;c++){
      float s=at(i,c)*obj;
      if(s>best){best=s;cls=c-class_start;}
    }
    if(best<threshold||cls<0)continue;
    const float cx=at(i,0),cy=at(i,1),w=at(i,2),h=at(i,3);
    if(!std::isfinite(cx)||!std::isfinite(cy)||!std::isfinite(w)||!std::isfinite(h)||w<=0||h<=0)continue;
    const float sx=float(frame.cols)/input_size_,sy=float(frame.rows)/input_size_;
    cv::Rect r(cv::Point(cvRound((cx-w/2)*sx),cvRound((cy-h/2)*sy)),
               cv::Point(cvRound((cx+w/2)*sx),cvRound((cy+h/2)*sy)));
    r &= cv::Rect(0,0,frame.cols,frame.rows);
    if(r.area()<=0)continue;
    boxes.push_back(r);scores.push_back(best);labels.push_back(cls);
  }
  std::vector<int> keep;cv::dnn::NMSBoxes(boxes,scores,static_cast<float>(threshold),0.45f,keep);
  std::vector<Detection> result;
  for(int i:keep){const auto&r=boxes[i];result.push_back({{r.x+r.width/2.0,r.y+r.height/2.0},double(r.width),double(r.height),scores[i],labels[i]});}
  return result;
}
}
