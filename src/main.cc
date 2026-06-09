#include <cstdio>
#include <cstdlib>
#include <string>
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/strings/str_cat.h"
#include "absl/time/time.h"
#include "mediapipe/framework/calculator_framework.h"
#include "mediapipe/framework/formats/image_frame.h"
#include "mediapipe/framework/formats/image_frame_opencv.h"
#include "mediapipe/framework/port/file_helpers.h"
#include "mediapipe/framework/port/opencv_highgui_inc.h"
#include "mediapipe/framework/port/opencv_imgproc_inc.h"
#include "mediapipe/framework/port/opencv_video_inc.h"
#include "mediapipe/framework/port/parse_text_proto.h"
#include "mediapipe/framework/port/status.h"
#include "mediapipe/util/ini_config.h"
#include "mediapipe/util/resource_util.h"

#ifdef _WIN32
#include <windows.h>
#endif

// Command-line flags (override INI config values)
ABSL_FLAG(std::string, config_file, "", "Path to hand_tracking_config.ini");
ABSL_FLAG(std::string, calculator_graph_config_file, "", "Graph config .pbtxt file");
ABSL_FLAG(std::string, input_video_path, "", "Video file (webcam if not set)");
ABSL_FLAG(std::string, output_video_path, "", "Output .mp4 (window if not set)");
// Forward-declare the resource_root_dir flag (defined in resource_util_windows.cc)
ABSL_DECLARE_FLAG(std::string, resource_root_dir);

// Get the directory of the current executable (hardcoded config path base)
static std::string GetExeDir() {
#ifdef _WIN32
  char buf[MAX_PATH];
  DWORD len = GetModuleFileNameA(nullptr, buf, MAX_PATH);
  if (len > 0) {
    std::string path(buf, len);
    size_t pos = path.find_last_of("\\/");
    if (pos != std::string::npos) {
      return path.substr(0, pos);
    }
  }
#endif
  return ".";
}

// Hardcoded config file path — loaded from exe directory by default.
static std::string GetDefaultConfigPath() {
  return GetExeDir() + "/hand_tracking_config.ini";
}

// Convert a relative path to absolute, using exe directory as base.
static std::string MakeAbsolute(const std::string& path) {
  if (path.empty()) return path;
#ifdef _WIN32
  if (path.size() >= 2 && path[1] == ':') return path;
  if (path.size() >= 2 && path[0] == '\\' && path[1] == '\\') return path;
#endif
  if (path[0] == '/') return path;
  return GetExeDir() + "/" + path;
}

// Load configuration from INI file, with command-line overrides.
static bool LoadConfiguration(std::string& graph_config,
                              std::string& input_video,
                              std::string& output_video,
                              std::string& resource_root,
                              int& num_threads,
                              bool& xnnpack_enable,
                              int& num_hands,
                              int& model_complexity) {
  mediapipe::IniConfig ini;

  // Determine INI file path
  std::string ini_path = absl::GetFlag(FLAGS_config_file);
  if (ini_path.empty()) {
    std::string exe_dir = GetExeDir();
    const char* candidates[] = {
        "/hand_tracking_config.ini",
        "/resource/hand_tracking_config.ini",
    };
    bool found = false;
    for (const char* suffix : candidates) {
      std::string test_path = exe_dir + suffix;
      FILE* test = fopen(test_path.c_str(), "r");
      if (test) {
        fclose(test);
        ini_path = test_path;
        found = true;
        break;
      }
    }
    if (!found) {
      ini_path = GetDefaultConfigPath();
    }
  }

  if (!ini.LoadFromFile(ini_path)) {
    fprintf(stderr, "WARNING: Could not load config from %s, using command-line flags only\n",
            ini_path.c_str());
  }

  // Apply config values, command-line flags take precedence
  resource_root = absl::GetFlag(FLAGS_resource_root_dir);
  if (resource_root.empty()) {
    resource_root = MakeAbsolute(ini.Get("paths", "resource_root_dir", "resource"));
  } else {
    resource_root = MakeAbsolute(resource_root);
  }

  graph_config = absl::GetFlag(FLAGS_calculator_graph_config_file);
  if (graph_config.empty()) {
    graph_config = ini.Get("paths", "graph_config",
                           "graphs/hand_tracking_desktop_live.pbtxt");
  }

  // Video section
  input_video = absl::GetFlag(FLAGS_input_video_path);
  if (input_video.empty()) {
    input_video = ini.Get("video", "input", "");
  }
  if (!input_video.empty()) {
    input_video = MakeAbsolute(input_video);
  }

  output_video = absl::GetFlag(FLAGS_output_video_path);
  if (output_video.empty()) {
    output_video = ini.Get("video", "output", "");
  }

  // Execution settings
  num_threads = ini.GetInt("execution", "num_threads", 2);
  xnnpack_enable = ini.GetBool("execution", "xnnpack_enable", true);

  // Detection settings
  num_hands = ini.GetInt("detection", "num_hands", 2);
  model_complexity = ini.GetInt("detection", "model_complexity", 1);

  // Print loaded configuration
  fprintf(stderr, "=== Hand Tracking Configuration ===\n");
  fprintf(stderr, "  config_file:      %s\n", ini_path.c_str());
  fprintf(stderr, "  resource_root_dir: %s\n", resource_root.c_str());
  fprintf(stderr, "  graph_config:      %s\n", graph_config.c_str());
  fprintf(stderr, "  input_video:       %s\n",
          input_video.empty() ? "(webcam)" : input_video.c_str());
  fprintf(stderr, "  output_video:      %s\n",
          output_video.empty() ? "(window)" : output_video.c_str());
  fprintf(stderr, "  num_hands:         %d\n", num_hands);
  fprintf(stderr, "  model_complexity:  %d (0=lite, 1=full)\n", model_complexity);
  fprintf(stderr, "  num_threads:       %d\n", num_threads);
  fprintf(stderr, "  xnnpack:           %s\n",
          xnnpack_enable ? "true" : "false");
  fprintf(stderr, "=====================================\n");

  return true;
}

absl::Status RunMPPGraph() {
  // Load configuration
  std::string resource_root, graph_config, input_video, output_video;
  int num_threads, num_hands, model_complexity;
  bool xnnpack_enable;
  LoadConfiguration(graph_config, input_video, output_video, resource_root,
                    num_threads, xnnpack_enable, num_hands, model_complexity);

  // Apply resource_root_dir to the global flag (used by resource_util)
  if (!resource_root.empty()) {
    absl::SetFlag(&FLAGS_resource_root_dir, resource_root);
  }

  fprintf(stderr, "DEBUG: Loading graph config...\n");
  std::string config_content;

  // Resolve graph_config — always relative to resource_root_dir
  {
    std::string relative_config = graph_config;
    // Try stripping absolute root prefix
    std::string abs_root = resource_root + "/";
    if (relative_config.find(abs_root) == 0) {
      relative_config = relative_config.substr(abs_root.size());
    }
    // Try stripping just "resource/"
    if (relative_config.find("resource/") == 0) {
      relative_config = relative_config.substr(9);
    }
    MP_ASSIGN_OR_RETURN(std::string resolved_path,
                        mediapipe::PathToResourceAsFile(relative_config));
    fprintf(stderr, "DEBUG: Config path resolved: %s\n", resolved_path.c_str());
    MP_RETURN_IF_ERROR(mediapipe::file::GetContents(resolved_path, &config_content));
  }
  fprintf(stderr, "DEBUG: Config loaded (%d bytes)\n", (int)config_content.size());

  // Replace placeholders with values from INI config
  {
    auto replace = [&](const std::string& placeholder, int value) {
      size_t pos = 0;
      std::string val_str = std::to_string(value);
      while ((pos = config_content.find(placeholder, pos)) != std::string::npos) {
        config_content.replace(pos, placeholder.length(), val_str);
        pos += val_str.length();
      }
    };
    replace("{NUM_HANDS}", num_hands);
    replace("{MODEL_COMPLEXITY}", model_complexity);
  }
  fprintf(stderr, "DEBUG: Placeholders replaced: NUM_HANDS=%d, MODEL_COMPLEXITY=%d\n",
          num_hands, model_complexity);

  fprintf(stderr, "DEBUG: Parsing config proto...\n");
  mediapipe::CalculatorGraphConfig config =
      mediapipe::ParseTextProtoOrDie<mediapipe::CalculatorGraphConfig>(config_content);
  fprintf(stderr, "DEBUG: Config parsed OK\n");

  fprintf(stderr, "DEBUG: Creating graph...\n");
  mediapipe::CalculatorGraph graph;
  fprintf(stderr, "DEBUG: Initializing graph...\n");
  MP_RETURN_IF_ERROR(graph.Initialize(config));
  fprintf(stderr, "DEBUG: Graph initialized OK!\n");

  cv::VideoCapture capture;
  bool use_video = !input_video.empty();
  if (use_video) {
    FILE* test = fopen(input_video.c_str(), "rb");
    if (!test) {
      return absl::InternalError(
          absl::StrCat("Video file not found: ", input_video,
                       "\n  Specify a valid file in hand_tracking_config.ini [video] input=...\n"
                       "  Or use --input_video_path=<file> on the command line.\n"
                       "  Leave input empty to use webcam."));
    }
    fclose(test);
    capture.open(input_video);
  } else {
    fprintf(stderr, "INFO: No video file specified, trying webcam (camera 0)...\n");
    capture.open(0, cv::CAP_DSHOW);
    if (!capture.isOpened()) {
      // Fallback: try default backend
      capture.open(0);
    }
#if (CV_MAJOR_VERSION >= 3) && (CV_MINOR_VERSION >= 2)
    capture.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    capture.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
    capture.set(cv::CAP_PROP_FPS, 30);
#endif
  }
  if (!capture.isOpened()) {
    if (use_video) {
      return absl::InternalError(
          absl::StrCat("Cannot open video file: ", input_video,
                       "\n  The file exists but OpenCV could not decode it."));
    } else {
      return absl::InternalError(
          "Cannot open webcam (camera 0).\n"
          "  Make sure a camera is connected and accessible.\n"
          "  Or specify a video file in hand_tracking_config.ini [video] input=...");
    }
  }

  const char* kWindowName = "Hand Tracking";
  fprintf(stderr, "DEBUG: Starting graph run...\n");
  MP_ASSIGN_OR_RETURN(auto poller, graph.AddOutputStreamPoller("output_video"));
  MP_RETURN_IF_ERROR(graph.StartRun({}));

  bool running = true;
  int stall_count = 0;
  while (running) {
    cv::Mat frame_raw;
    capture >> frame_raw;
    if (frame_raw.empty()) {
      if (!use_video) {
        // Webcam: ignore empty frames
        continue;
      }
      // Video file: end of file
      break;
    }
    cv::Mat frame;
    cv::cvtColor(frame_raw, frame, cv::COLOR_BGR2RGB);
    if (!use_video) cv::flip(frame, frame, 1);  // mirror webcam

    auto input_frame = absl::make_unique<mediapipe::ImageFrame>(
        mediapipe::ImageFormat::SRGB, frame.cols, frame.rows,
        mediapipe::ImageFrame::kDefaultAlignmentBoundary);
    cv::Mat input_mat = mediapipe::formats::MatView(input_frame.get());
    frame.copyTo(input_mat);

    size_t ts = (double)cv::getTickCount() / cv::getTickFrequency() * 1e6;
    MP_RETURN_IF_ERROR(graph.AddPacketToInputStream(
        "input_video", mediapipe::Adopt(input_frame.release())
                           .At(mediapipe::Timestamp(ts))));

    // Poll with timeout — prevents indefinite hang if graph stalls
    mediapipe::Packet packet;
    while (true) {
      if (poller.Next(&packet)) {
        stall_count = 0;
        break;
      }
      // Check if user pressed a key to quit
      if (cv::waitKey(1) >= 0) {
        running = false;
        break;
      }
      // Timeout reached — graph may be backed up or stuck
      stall_count++;
      fprintf(stderr, "WARNING: Graph output stalled (%d), waiting...\n", stall_count);
      if (stall_count > 10) {
        fprintf(stderr, "ERROR: Graph stalled too long, shutting down.\n");
        running = false;
        break;
      }
    }
    if (!running) break;

    auto& output = packet.Get<mediapipe::ImageFrame>();
    cv::Mat output_mat = mediapipe::formats::MatView(&output);
    cv::cvtColor(output_mat, output_mat, cv::COLOR_RGB2BGR);
    cv::imshow(kWindowName, output_mat);
    if (cv::waitKey(5) >= 0) running = false;
  }

  fprintf(stderr, "DEBUG: Shutting down...\n");
  MP_RETURN_IF_ERROR(graph.CloseInputStream("input_video"));
  return graph.WaitUntilDone();
}

int main(int argc, char** argv) {
  google::InitGoogleLogging(argv[0]);
  absl::ParseCommandLine(argc, argv);
  absl::Status status = RunMPPGraph();
  if (!status.ok()) {
    fprintf(stderr, "ERROR: %s\n", status.ToString().c_str());
    return 1;
  }
  fprintf(stderr, "SUCCESS!\n");
  return 0;
}
