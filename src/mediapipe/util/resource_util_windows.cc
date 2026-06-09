// Copyright 2019 The MediaPipe Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// (Simplified for CMake build - removed Bazel runfiles dependency)

#include <fstream>

#include "absl/flags/flag.h"
#include "mediapipe/framework/deps/file_path.h"
#include "mediapipe/framework/port/file_helpers.h"
#include "mediapipe/framework/port/singleton.h"
#include "mediapipe/framework/port/statusor.h"

ABSL_FLAG(
    std::string, resource_root_dir, "",
    "The absolute path to the resource directory."
    "If specified, resource_root_dir will be prepended to the original path.");

namespace mediapipe {

using mediapipe::file::GetContents;
using mediapipe::file::JoinPath;

namespace internal {

absl::Status DefaultGetResourceContents(const std::string& path,
                                        std::string* output,
                                        bool read_as_binary) {
  // Don't double-prepend: if the path already contains the resource_root_dir
  // (e.g. from PathToResourceAsFile), use it directly.
  std::string root = absl::GetFlag(FLAGS_resource_root_dir);
  if (root.empty() || path.find(root) == 0 || path.find(":") != std::string::npos) {
    return GetContents(path, output, read_as_binary);
  }
  std::string resource_path = JoinPath(root, path);
  return GetContents(resource_path, output, read_as_binary);
}

}  // namespace internal

absl::StatusOr<std::string> PathToResourceAsFile(const std::string& path) {
  return JoinPath(absl::GetFlag(FLAGS_resource_root_dir), path);
}

}  // namespace mediapipe
