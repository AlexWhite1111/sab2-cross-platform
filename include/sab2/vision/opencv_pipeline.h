#pragma once
#include "sab2/core/types.h"
#include <opencv2/core.hpp>
#include <opencv2/dnn.hpp>
#include <string>
#include <vector>
namespace sab2 {
class VisionDetector {
public:
  explicit VisionDetector(const std::string& onnx_file="",int input_size=640);
  std::vector<Detection> detect(const cv::Mat& bgr,double confidence=0.35) ;
  bool is_onnx()const{return with_model_;}
private:
  cv::dnn::Net net_;
  bool with_model_=false;
  int input_size_=640;
  std::vector<Detection> onnx_detect(const cv::Mat& frame,double threshold);
  std::vector<Detection> green_marker_detect(const cv::Mat& frame);
};
}
