#!/usr/bin/env python3
"""Generate subgraph .cc files from .pbtxt files for hand_tracking."""
import os, re

SRC_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "src", "mediapipe")
OUTPUT_DIR = os.path.join(SRC_DIR, "generated_subgraphs")

# CPU subgraphs needed by hand_tracking (in dependency order)
# (pbtxt_path_relative_to_src, type_name)
SUBRAPHS = [
    ("modules/palm_detection/palm_detection_model_loader.pbtxt", "PalmDetectionModelLoader"),
    ("modules/hand_landmark/hand_landmark_model_loader.pbtxt", "HandLandmarkModelLoader"),
    ("modules/palm_detection/palm_detection_cpu.pbtxt", "PalmDetectionCpu"),
    ("modules/hand_landmark/hand_landmark_cpu.pbtxt", "HandLandmarkCpu"),
    ("modules/hand_landmark/hand_landmark_landmarks_to_roi.pbtxt", "HandLandmarkLandmarksToRoi"),
    ("modules/hand_landmark/palm_detection_detection_to_roi.pbtxt", "PalmDetectionDetectionToRoi"),
    ("modules/hand_landmark/hand_landmark_tracking_cpu.pbtxt", "HandLandmarkTrackingCpu"),
    ("graphs/hand_tracking/subgraphs/hand_renderer_cpu.pbtxt", "HandRendererSubgraph"),
]

os.makedirs(OUTPUT_DIR, exist_ok=True)

for pbtxt_rel, type_name in SUBRAPHS:
    pbtxt_path = os.path.join(SRC_DIR, pbtxt_rel)
    if not os.path.exists(pbtxt_path):
        print(f"WARNING: {pbtxt_path} not found, skipping")
        continue

    with open(pbtxt_path, 'r', encoding='utf-8') as f:
        content = f.read()

    # Strip the type: line (it's set via class name / GetConfig)
    content = re.sub(r'^type:\s*"[^"]*"\s*\n', '', content, flags=re.MULTILINE)

    # Generate the .cc file
    cc_name = type_name + "_subgraph.cc"
    cc_path = os.path.join(OUTPUT_DIR, cc_name)

    with open(cc_path, 'w', encoding='utf-8') as f:
        f.write('#include "mediapipe/framework/calculator_framework.h"\n')
        f.write('#include "mediapipe/framework/subgraph.h"\n')
        f.write('#include "mediapipe/framework/port/parse_text_proto.h"\n')
        f.write('namespace mediapipe {\n')
        f.write('static const char* kCfg = R"pb(\n')
        f.write(content)
        if not content.endswith('\n'):
            f.write('\n')
        f.write(')pb";\n')
        f.write(f'class {type_name} : public Subgraph {{\n')
        f.write(' public:\n')
        f.write('  absl::StatusOr<CalculatorGraphConfig> GetConfig(const SubgraphOptions&) override {\n')
        f.write('    CalculatorGraphConfig config;\n')
        f.write(f'    if (!ParseTextProto(kCfg, &config)) return absl::InternalError("Bad subgraph: {type_name}");\n')
        f.write('    return config;\n')
        f.write('  }\n')
        f.write('};\n')
        f.write(f'REGISTER_MEDIAPIPE_GRAPH({type_name});\n')
        f.write('}  // namespace mediapipe\n')

    print(f"Generated: {cc_name}")

print(f"\nDone! Generated {len(SUBGRAPHS)} subgraph files to {OUTPUT_DIR}")
