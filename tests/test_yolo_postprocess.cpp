#include "hdmi_test/yolo_detector.hpp"

#include <cassert>

int main() {
  const std::vector<hdmi_test::YoloDetection> input{{10, 10, 50, 50, 0.90F, 0},
                                                    {12, 12, 52, 52, 0.80F, 0},
                                                    {12, 12, 52, 52, 0.70F, 1}};
  const auto detections = hdmi_test::non_maximum_suppression(input, 0.45F);
  assert(detections.size() == 2U);
  assert(detections[0].confidence == 0.90F);
  assert(detections[1].class_id == 1);
}
