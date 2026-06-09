#include "mediapipe/framework/calculator_framework.h"
#include "mediapipe/framework/subgraph.h"
#include "mediapipe/framework/port/parse_text_proto.h"
namespace mediapipe {
static const char* kCfg = R"pb(
# MediaPipe graph to calculate hand region of interest (ROI) from landmarks
# detected by "HandLandmarkCpu" or "HandLandmarkGpu".

# Normalized landmarks. (NormalizedLandmarkList)
input_stream: "LANDMARKS:landmarks"
# Image size (width & height). (std::pair<int, int>)
input_stream: "IMAGE_SIZE:image_size"

# ROI according to landmarks. (NormalizedRect)
output_stream: "ROI:roi"

# Converts the hand landmarks into a rectangle (normalized by image size)
# that encloses the hand.
node {
  calculator: "HandLandmarksToRectCalculator"
  input_stream: "NORM_LANDMARKS:landmarks"
  input_stream: "IMAGE_SIZE:image_size"
  output_stream: "NORM_RECT:hand_rect_from_landmarks"
}

# Expands the hand rectangle so that the box contains the entire hand and it's
# big enough so that it's likely to still contain the hand even with some motion
# in the next video frame .
node {
  calculator: "RectTransformationCalculator"
  input_stream: "NORM_RECT:hand_rect_from_landmarks"
  input_stream: "IMAGE_SIZE:image_size"
  output_stream: "roi"
  options: {
    [mediapipe.RectTransformationCalculatorOptions.ext] {
      scale_x: 2.0
      scale_y: 2.0
      shift_y: -0.1
      square_long: true
    }
  }
}
)pb";
class HandLandmarkLandmarksToRoi : public Subgraph {
 public:
  absl::StatusOr<CalculatorGraphConfig> GetConfig(const SubgraphOptions&) override {
    CalculatorGraphConfig config;
    if (!ParseTextProto(kCfg, &config)) return absl::InternalError("Bad subgraph: HandLandmarkLandmarksToRoi");
    return config;
  }
};
REGISTER_MEDIAPIPE_GRAPH(HandLandmarkLandmarksToRoi);
}  // namespace mediapipe
