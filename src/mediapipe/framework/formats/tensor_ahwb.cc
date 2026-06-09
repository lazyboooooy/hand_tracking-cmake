// Stub for non-Android builds
#include "mediapipe/framework/formats/tensor.h"
namespace mediapipe {
void Tensor::MoveAhwbStuff(Tensor*) {}
void Tensor::ReleaseAhwbStuff() {}
void Tensor::TrackAhwbUsage(uint64_t) const {}
}  // namespace mediapipe
