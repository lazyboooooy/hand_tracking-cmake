// Pre-include header to define GLog macros before MediaPipe includes glog/logging.h
// This avoids CMake command-line issues with parentheses in macro definitions

#ifndef FACE_MESH_GLOG_CONFIG_H_
#define FACE_MESH_GLOG_CONFIG_H_

// GLOG_EXPORT: empty for static linking
#define GLOG_EXPORT

// GLOG_DEPRECATED: empty for now
#define GLOG_DEPRECATED

// GLOG_NO_ABBREVIATED_SEVERITIES: must be defined on Windows
#define GLOG_NO_ABBREVIATED_SEVERITIES

// GLOG_MSVC macros: use proper MSVC pragmas
#if defined(_MSC_VER)
#define GLOG_MSVC_PUSH_DISABLE_WARNING(n) \
  __pragma(warning(push)) __pragma(warning(disable : n))
#define GLOG_MSVC_POP_WARNING() __pragma(warning(pop))
#else
#define GLOG_MSVC_PUSH_DISABLE_WARNING(n)
#define GLOG_MSVC_POP_WARNING()
#endif

#endif  // FACE_MESH_GLOG_CONFIG_H_
