#ifndef MEDIAPIPE_GPU_GPU_SERVICE_H_
#define MEDIAPIPE_GPU_GPU_SERVICE_H_

#include <memory>
#include "mediapipe/framework/graph_service.h"

namespace mediapipe {

// Minimal GpuResources for CPU-only mode
class GpuResources {
 public:
  GpuResources() = default;
  ~GpuResources() = default;

  using StatusOrGpuResources = absl::StatusOr<std::shared_ptr<GpuResources>>;
  static StatusOrGpuResources Create() {
    return std::make_shared<GpuResources>();
  }
};

inline constexpr GraphService<GpuResources> kGpuService("kGpuService");

}  // namespace mediapipe

#endif  // MEDIAPIPE_GPU_GPU_SERVICE_H_
