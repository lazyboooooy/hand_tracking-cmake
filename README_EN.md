# Hand Tracking CPU — Pure CPU Hand Tracking Decoupled from MediaPipe

[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Windows%20x64-lightgrey.svg)]()
[![C++](https://img.shields.io/badge/C%2B%2B-17-blue.svg)]()

A standalone hand tracking desktop application **decoupled from Google MediaPipe**. Pure CPU inference with zero GPU dependency — tracks 21 3D hand landmarks per hand in real time via webcam or video file, with multi-hand support (up to 4 hands) and left/right handedness detection.

![Hand Tracking Demo](demo/hand_tracking.gif)

> The GIF above shows Hand Tracking running on live webcam input: 21 landmarks per hand covering fingertips, knuckles, palm, and wrist, along with palm detection boxes and handedness labels.

> **Based on** Google [MediaPipe 0.10.10](https://github.com/google/mediapipe). Preserves the full hand tracking pipeline (palm detection + landmark localization + rendering), with Full/Lite dual-mode support.
>
> **中文文档**: [README.md](README.md)

---

## Features

- ✅ **Zero GPU dependency** — pure CPU inference with XNNPACK SIMD acceleration (SSE/AVX)
- ✅ **21 3D hand landmarks** — 4 fingertips + 5 knuckles + palm + wrist per hand, with x/y/z coordinates
- ✅ **Multi-hand tracking** — supports 1–4 hands simultaneously at 20–30 FPS
- ✅ **Handedness detection** — auto-labels left/right hand with confidence scores
- ✅ **Dual model support** — Full mode (high accuracy) / Lite mode (faster), hot-swappable via INI
- ✅ **Non-blocking pipeline** — FlowLimiterCalculator prevents frame backlog
- ✅ **Hot-reloadable configuration** — change hand count, complexity mode, thread count without recompiling
- ✅ **Video file or webcam input** — auto-detects and switches

---

## Pipeline Architecture

```
Camera Frame (OpenCV)
  → HandLandmarkTrackingCpu (palm detection + hand landmark tracking)
      ├── PalmDetectionCpu (192×192 full-image palm detection)
      │   └── Output: palm detection boxes + handedness labels
      ├── HandLandmarkCpu (224×224 hand landmark regression, 21 points x/y/z)
      │   └── Output: 21 3D landmarks + handedness confidence per hand
      └── HandLandmarkLandmarksToRoi (landmarks → ROI for next-frame tracking)
  → HandRendererSubgraph (landmarks + detection boxes + handedness overlay)
  → Output Frame (OpenCV imshow)
```

**Core design**: Palm Detection locates hands in the full frame, then Hand Landmark performs fine-grained 21-point regression within the detection ROI. This two-stage approach balances wide detection coverage with pinpoint landmark accuracy.

---

## Models

### Model Specifications

| Model | Size | Input | Output | Purpose |
|-------|------|-------|--------|---------|
| `palm_detection_full.tflite` | 2.3 MB | 192×192 RGB | Palm detection boxes + handedness | Stage 1: palm localization |
| `hand_landmark_full.tflite` | 5.5 MB | 224×224 RGB | **21 3D landmarks** + handedness scores | Stage 2: landmark regression |

### 21 Hand Landmarks

Right hand, back view (fingers pointing up, ● numbers = landmark indices):

```
                        ●8      ●12     ●16     ●20     ← Tips
                        │        │       │       │
                        ●7      ●11     ●15     ●19     ← DIP (distal interphalangeal)
                        │        │       │       │
      ●4 (Thumb tip)    ●6      ●10     ●14     ●18     ← PIP (proximal interphalangeal)
       │                │        │       │       │
      ●3 (Thumb IP)     ●5      ●9      ●13     ●17     ← MCP (metacarpophalangeal)
       │                │╲       │╲      │╲      │
      ●2 (Thumb MCP)    │ ╲      │ ╲     │ ╲     │
        ╲               │  ╲     │  ╲    │  ╲    │
         ●1 (Thumb CMC)─┘   ╲    │   ╲   │   ╲   │
           ╲                  ╲   │    ╲  │    ╲  │
            ╲                  ╲  │     ╲ │     ╲ │
             ╲                  ╲ │      ╲│      ╲│
              └──────●0──────────┸┸───────┸┘───────┘
                     Wrist

        Thumb          Index   Middle  Ring   Pinky
```

| Finger | Landmark Indices | Joint chain (palm → tip) |
|--------|-----------------|--------------------------|
| Wrist | **0** | Wrist center |
| Thumb | **1 → 2 → 3 → 4** | CMC (carpometacarpal) → MCP → IP (interphalangeal) → Tip |
| Index | **5 → 6 → 7 → 8** | MCP → PIP → DIP → Tip |
| Middle | **9 → 10 → 11 → 12** | MCP → PIP → DIP → Tip |
| Ring | **13 → 14 → 15 → 16** | MCP → PIP → DIP → Tip |
| Pinky | **17 → 18 → 19 → 20** | MCP → PIP → DIP → Tip |

> **Thumb note**: The thumb has only two phalanges (proximal and distal), resulting in a single interphalangeal (IP) joint, unlike the other four fingers which have both PIP and DIP joints. Each landmark outputs (x, y, z) 3D coordinates — the z-coordinate represents depth relative to the wrist, enabling gesture recognition and 3D hand modeling.

---

## Quick Start

### Prerequisites

- **Windows 10/11 x64**
- **Visual Studio 2022 or newer** with C++ CMake tools
- **CMake** 3.16+

### Build

```bash
cd hand_tracking
cmake -B build -S .
cmake --build build --config Release
```

Output: `build/Release/hand_tracking_cpu.exe`

### Run

```bash
cd build/Release
./hand_tracking_cpu.exe
```

Uses camera 0 by default. Press `ESC` to exit.

### Configuration

Edit `hand_tracking_config.ini` (auto-copied next to the exe at build time):

```ini
[paths]
resource_root_dir = resource
graph_config = graphs/hand_tracking_desktop_live.pbtxt

[video]
input =                         # Video file path (leave empty for webcam)
output =                        # Output MP4 (reserved)

[models]
palm_detection = mediapipe/modules/palm_detection/palm_detection_full.tflite
hand_landmark = mediapipe/modules/hand_landmark/hand_landmark_full.tflite

[detection]
num_hands = 2                   # Max hands to track (1–4)
model_complexity = 1            # 0 = Lite (faster), 1 = Full (more accurate)

[execution]
num_threads = 2                 # TFLite inference threads (0 = auto)
xnnpack_enable = true           # XNNPACK SIMD acceleration
```

### Recommended Settings

| Use Case | num_hands | complexity | threads | Why |
|----------|:---:|:---:|:---:|------|
| Single-hand gestures | 1 | Full | 2 | Max accuracy, one hand sufficient |
| Two-hand interaction | 2 | Full | 4 | High accuracy + multi-hand |
| Multi-person hands | 4 | Lite | 4 | More hands, prioritize FPS |
| Low-end laptop | 1 | Lite | 1 | Minimize compute |

### Command-line Overrides

```bash
./hand_tracking_cpu.exe --input_video_path=D:/video.mp4 --config_file=D:/custom.ini
```

---

## 8 Calculator Subgraphs

| Subgraph | Function |
|----------|----------|
| `HandLandmarkTrackingCpu` | Top-level scheduler: palm detection + landmark tracking + ROI loopback |
| `PalmDetectionCpu` | CPU palm detection wrapper |
| `PalmDetectionModelLoader` | Load palm detection model per complexity mode |
| `PalmDetectionDetectionToRoi` | Palm detection box → hand ROI |
| `HandLandmarkCpu` | Hand landmark detection (224×224, 21 points) |
| `HandLandmarkModelLoader` | Load hand landmark model per complexity mode |
| `HandLandmarkLandmarksToRoi` | Landmarks → ROI (for next-frame tracking) |
| `HandRendererSubgraph` | Landmarks + detection boxes + handedness overlay |

---

## 6 Custom TFLite Operators

The project uses the same 6 custom operators as face_mesh:

| Operator | Purpose |
|----------|---------|
| `MaxPoolingWithArgmax2D` | Max pooling with position recording |
| `MaxUnpooling2D` | Max unpooling |
| `Convolution2DTransposeBias` | Transposed convolution with bias |
| `TransformTensorBilinear` | Bilinear tensor transformation (ROI feature alignment) |
| `TransformLandmarks` | Landmark coordinate normalization + rotation transform |
| `Landmarks2TransformMatrix` | Landmark set → rigid transform matrix |

---

## Directory Structure

```
hand_tracking/
├── CMakeLists.txt                            # Build config (C++17, MSVC)
├── README.md（中文）
├── README_EN.md（English）
├── LICENSE                                   # Apache 2.0
├── demo/
│   └── hand_tracking.gif                     # Demo GIF
├── src/
│   ├── main.cc                               # Entry: graph loading, camera, display
│   ├── glog_config.h                         # GLog pre-include for MSVC
│   └── mediapipe/
│       ├── abseil_log/                       # Abseil log stubs
│       ├── calculators/                      # Core / image / tensor / TFLite / util calculators
│       ├── framework/                        # Graph engine, packets, formats
│       ├── generated_subgraphs/              # Pbtxt → C++ subgraphs (8 total)
│       ├── gpu/                              # GPU buffer management (Image type only, no OpenGL)
│       ├── graphs/hand_tracking/             # Hand renderer calculator
│       ├── tflite_kernels/                   # Custom TFLite op kernels
│       └── util/tflite/                      # OpResolver, custom op registration
├── resource/
│   ├── hand_tracking_config.ini              # Runtime configuration
│   ├── graphs/
│   │   ├── hand_tracking_desktop_live.pbtxt  # Top-level graph
│   │   └── hand_renderer_cpu.pbtxt           # Renderer subgraph
│   └── mediapipe/modules/
│       ├── palm_detection/
│       │   └── palm_detection_full.tflite    # Palm detection model (2.3 MB)
│       └── hand_landmark/
│           ├── hand_landmark_full.tflite     # Hand landmark model (5.5 MB)
│           └── handedness.txt                # Handedness label mapping
├── 3rdparty/                                 # Pre-built dependencies (~240 MB)
│   ├── tflite/  opencv/  protobuf/  abseil/
│   ├── XNNPACK/  eigen/  flatbuffers/
│   └── ...
└── cmake/
    ├── protoc.exe                            # Protocol Buffer compiler
    └── regenerate_subgraphs.py              # Pbtxt → C++ code generator
```

---

## Performance (i7-12700H)

| Metric | Value |
|--------|-------|
| Frame processing (Full, 1 hand) | 8–18 ms |
| Frame processing (Full, 2 hands) | 15–28 ms |
| Frame processing (Lite, 2 hands) | 10–20 ms |
| Real-time FPS | 20–30 |
| RAM usage | ~180 MB |
| EXE size | 6.8 MB |
| Total model size | 7.8 MB |

---

## Comparison with Other Approaches

| Solution | Landmarks | Hands | Inference | Model Size | CPU FPS |
|----------|-----------|-------|-----------|------------|---------|
| **This project** | 21 (3D) | 1–4 | CPU (XNNPACK) | 7.8 MB | 20–30 |
| MediaPipe original | 21 (3D) | 1–4 | CPU/GPU | 7.8 MB | 20–30 |
| OpenCV Handpose | 21 (2D) | 1 | CPU | ~2 MB | 10–20 |
| Google ML Kit | 21 (3D) | 1–2 | Mobile GPU | — | 30 |

---

## Relationship with face_mesh / face_detection / hair_segmentation

All four projects share the same origin (MediaPipe 0.10.10), 3rdparty dependency tree, and CMake build system:

| Project | Function | Models | Subgraphs | Custom Ops |
|---------|----------|--------|-----------|:---:|
| face_detection | Face detection + 6 pts | 1 (225 KB) | 5 | 4 |
| hair_segmentation | Hair segmentation + recolor | 1 (782 KB) | 0 | 0 |
| **hand_tracking (this project)** | Hand 21 pts + handedness | **2 (7.8 MB)** | **8** | **6** |
| face_mesh | Face 478 pts | 3 (3.8 MB) | 12 | 6 |

hand_tracking sits between hair_segmentation and face_mesh in complexity — 8 subgraphs and 6 custom ops, with a clear and well-structured pipeline.

---

## Limitations

- **Full-palm detection only**: 192×192 detection input — weak on very distant palms (< 30 px). Switch to Lite mode or substitute `palm_detection_lite.tflite`
- **Occlusion sensitivity**: Crossed fingers or closed fists may cause occluded landmark drift
- **Windows only**: Currently MSVC-only. Uses `GetModuleFileNameA` and DirectShow
- **No video output**: MP4 export is reserved in config but not yet implemented
- **Large 3rdparty footprint**: ~240 MB of pre-built libraries

---

## License

This project is open-sourced under the **Apache License 2.0**. See [LICENSE](LICENSE) for the full text.

The original MediaPipe project is Copyright Google LLC, also under Apache 2.0.

---

## Acknowledgements

- [Google MediaPipe](https://developers.google.com/mediapipe) — original framework and models
- [TensorFlow Lite](https://www.tensorflow.org/lite) — inference engine
- [XNNPACK](https://github.com/google/XNNPACK) — CPU SIMD acceleration
- [OpenCV](https://opencv.org/) — camera I/O and rendering
