cmake_minimum_required(VERSION 3.16)
project(hand_tracking_cpu LANGUAGES C CXX)

# ============================================================
# C++ Standard
# ============================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================================
# Platform defines (CPU-only mode, matching original Bazel build)
# ============================================================
add_definitions(
    -DMEDIAPIPE_DISABLE_GPU=1
    -DNOMINMAX
    -DWIN32_LEAN_AND_MEAN
    -D_WIN32_WINNT=0x0A00
    -DGLOG_EXPORT=
    -DGLOG_DEPRECATED=
    -DGLOG_NO_ABBREVIATED_SEVERITIES
    -D_USE_MATH_DEFINES
)

# MSVC warnings - suppress noisy warnings from third-party code
if(MSVC)
    add_compile_options(
        /wd4018 /wd4244 /wd4251 /wd4267 /wd4305 /wd4309
        /wd4334 /wd4456 /wd4457 /wd4503 /wd4800 /wd4996
        /wd4005 /wd4065 /wd4100 /wd4127 /wd4146 /wd4189
        /wd4200 /wd4245 /wd4389 /wd4464 /wd4505 /wd4514
        /wd4571 /wd4624 /wd4626 /wd4668 /wd4702 /wd4710
        /wd4711 /wd4778 /wd4820 /wd4868 /wd5026 /wd5027
        /wd5045 /wd5219 /wd5246
    )
endif()

# ============================================================
# Third-party dependencies (shared via copy from face_mesh)
# ============================================================
set(THIRDPARTY_DIR "${CMAKE_SOURCE_DIR}/3rdparty")

# ============================================================
# OpenCV (bundled in 3rdparty/opencv/)
# ============================================================
set(OPENCV_INCLUDE_DIR "${THIRDPARTY_DIR}/opencv/include")
file(GLOB OPENCV_LIBS "${THIRDPARTY_DIR}/opencv/lib/opencv_world*.lib")
set(OPENCV_DLL "${THIRDPARTY_DIR}/opencv/bin/opencv_world3410.dll")
if(NOT OPENCV_LIBS)
    message(FATAL_ERROR "OpenCV lib not found in ${THIRDPARTY_DIR}/opencv/lib/")
endif()

# ============================================================
# Include directories (all in 3rdparty/ structure)
# ============================================================
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    # OpenCV
    ${OPENCV_INCLUDE_DIR}
    # Runtime: GLog
    ${THIRDPARTY_DIR}/glog/include
    # Runtime: GFlags (needed by GLog)
    ${THIRDPARTY_DIR}/gflags/include
    # Runtime: Abseil
    ${THIRDPARTY_DIR}/abseil/include
    # Runtime: Protobuf
    ${THIRDPARTY_DIR}/protobuf/include
    # Runtime: TFLite
    ${THIRDPARTY_DIR}/tflite/include
    # Runtime: FlatBuffers
    ${THIRDPARTY_DIR}/flatbuffers/include
    # Runtime: Eigen (TFLite expects third_party/eigen3 path)
    ${THIRDPARTY_DIR}/eigen/include
    # Symlink: third_party/eigen3/Eigen -> 3rdparty/eigen/include/Eigen
    ${CMAKE_BINARY_DIR}/generated
    # Runtime: XNNPACK
    ${THIRDPARTY_DIR}/XNNPACK/include
    ${THIRDPARTY_DIR}/XNNPACK/include/src
    # Runtime: cpuinfo
    ${THIRDPARTY_DIR}/cpuinfo/include
    ${THIRDPARTY_DIR}/cpuinfo
    # Runtime: pthreadpool
    ${THIRDPARTY_DIR}/pthreadpool/include
    # Runtime: farmhash
    ${THIRDPARTY_DIR}/farmhash/include
    # Runtime: ruy
    ${THIRDPARTY_DIR}/ruy/include
    ${THIRDPARTY_DIR}/ruy
    # Runtime: FP16
    ${THIRDPARTY_DIR}/FP16/include
    # Runtime: FXdiv
    ${THIRDPARTY_DIR}/FXdiv/include
    # Runtime: gemmlowp (fixedpoint headers)
    ${THIRDPARTY_DIR}/gemmlowp/include
    # Runtime: fft2d
    ${THIRDPARTY_DIR}/fft2d/include
    # Runtime: arm_neon_2_x86_sse
    ${THIRDPARTY_DIR}/arm_neon_2_x86_sse/include
    # Generated proto files
    ${CMAKE_BINARY_DIR}/generated
)

# ============================================================
# Eigen3 path junction (TFLite expects third_party/eigen3/Eigen/Core)
# ============================================================
set(EIGEN3_DIR "${CMAKE_BINARY_DIR}/generated/third_party/eigen3")
if(NOT EXISTS "${EIGEN3_DIR}/Eigen")
    file(MAKE_DIRECTORY "${EIGEN3_DIR}")
    execute_process(
        COMMAND cmd /c "mklink /J \"${EIGEN3_DIR}/Eigen\" \"${THIRDPARTY_DIR}/eigen/include/Eigen\""
        RESULT_VARIABLE MKLINK_RESULT
    )
    if(NOT MKLINK_RESULT EQUAL 0)
        file(COPY "${THIRDPARTY_DIR}/eigen/include/Eigen" DESTINATION "${EIGEN3_DIR}")
        message(STATUS "Eigen3: copied (junction not available)")
    else()
        message(STATUS "Eigen3: directory junction created")
    endif()
endif()

# ============================================================
# Protobuf compiler
# ============================================================
set(PROTOC_EXECUTABLE "${CMAKE_SOURCE_DIR}/cmake/protoc.exe")

# ============================================================
# Generate all proto files (deduplicated by filename)
# ============================================================
file(GLOB_RECURSE PROTO_FILES_ALL ${CMAKE_SOURCE_DIR}/src/mediapipe/*.proto)

# Deduplicate proto files by basename
set(PROTO_FILES "")
set(PROTO_NAMES "")
foreach(PROTO_FILE ${PROTO_FILES_ALL})
    get_filename_component(PROTO_BASENAME ${PROTO_FILE} NAME)
    if(NOT PROTO_BASENAME IN_LIST PROTO_NAMES)
        list(APPEND PROTO_NAMES ${PROTO_BASENAME})
        list(APPEND PROTO_FILES ${PROTO_FILE})
    endif()
endforeach()

# Exclude protos for calculators NOT used by hand_tracking
list(FILTER PROTO_FILES EXCLUDE REGEX ".*combine_joints_calculator\\.proto$")
list(FILTER PROTO_FILES EXCLUDE REGEX ".*detection_label_id_to_text_calculator\\.proto$")
list(FILTER PROTO_FILES EXCLUDE REGEX ".*rect_to_render_scale_calculator\\.proto$")
list(FILTER PROTO_FILES EXCLUDE REGEX ".*filter_detections_calculator\\.proto$")
list(FILTER PROTO_FILES EXCLUDE REGEX ".*concatenate_vector_calculator\\.proto$")
list(FILTER PROTO_FILES EXCLUDE REGEX ".*packet_cloner_calculator\\.proto$")
list(FILTER PROTO_FILES EXCLUDE REGEX ".*packet_resampler_calculator\\.proto$")
list(FILTER PROTO_FILES EXCLUDE REGEX ".*image_cropping_calculator\\.proto$")
list(FILTER PROTO_FILES EXCLUDE REGEX ".*opencv_encoded_image_to_image_frame_calculator\\.proto$")
list(FILTER PROTO_FILES EXCLUDE REGEX ".*warp_affine_calculator\\.proto$")
# label_map.proto is needed by tensors_to_classification_calculator.proto — keep it

message(STATUS "Proto files: ${PROTO_FILES}")

# Proto include paths
set(PROTO_INCLUDE_ARGS
    "--proto_path=${CMAKE_SOURCE_DIR}/src"
    "--proto_path=${THIRDPARTY_DIR}/protobuf/include"
)

# Generate proto sources at configure time
set(GENERATED_PROTO_DIR "${CMAKE_BINARY_DIR}/generated")
file(MAKE_DIRECTORY ${GENERATED_PROTO_DIR})

message(STATUS "Generating proto files...")
execute_process(
    COMMAND ${PROTOC_EXECUTABLE}
        ${PROTO_INCLUDE_ARGS}
        --cpp_out=${GENERATED_PROTO_DIR}
        ${PROTO_FILES}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    RESULT_VARIABLE PROTOC_RESULT
    OUTPUT_VARIABLE PROTOC_OUT
    ERROR_VARIABLE PROTOC_ERR
)
if(NOT PROTOC_RESULT EQUAL 0)
    message(WARNING "protoc errors: ${PROTOC_ERR}")
endif()
message(STATUS "Proto generation complete")

# ============================================================
# Collect all MediaPipe source files
# Hand tracking needs nearly all calculators except face_mesh-specific ones
# ============================================================
file(GLOB_RECURSE MEDIAPIPE_SRCS
    ${CMAKE_SOURCE_DIR}/src/mediapipe/framework/*.cc
    ${CMAKE_SOURCE_DIR}/src/mediapipe/framework/formats/*.cc
    ${CMAKE_SOURCE_DIR}/src/mediapipe/framework/deps/*.cc
    ${CMAKE_SOURCE_DIR}/src/mediapipe/framework/tool/*.cc
    ${CMAKE_SOURCE_DIR}/src/mediapipe/framework/stream_handler/*.cc
    ${CMAKE_SOURCE_DIR}/src/mediapipe/framework/profiler/*.cc
    ${CMAKE_SOURCE_DIR}/src/mediapipe/framework/api2/*.cc
    ${CMAKE_SOURCE_DIR}/src/mediapipe/calculators/core/*.cc
    ${CMAKE_SOURCE_DIR}/src/mediapipe/calculators/tensor/*.cc
    ${CMAKE_SOURCE_DIR}/src/mediapipe/calculators/tflite/*.cc
    ${CMAKE_SOURCE_DIR}/src/mediapipe/calculators/util/*.cc
    ${CMAKE_SOURCE_DIR}/src/mediapipe/calculators/image/*.cc
    # Hand tracking specific calculators
    ${CMAKE_SOURCE_DIR}/src/mediapipe/modules/hand_landmark/calculators/*.cc
    # Framework utilities
    ${CMAKE_SOURCE_DIR}/src/mediapipe/util/*.cc
    ${CMAKE_SOURCE_DIR}/src/mediapipe/util/tflite/*.cc
    ${CMAKE_SOURCE_DIR}/src/mediapipe/util/tflite/operations/*.cc
    # Abseil log stubs
    ${CMAKE_SOURCE_DIR}/src/mediapipe/abseil_log_stubs.cc
    # Generated subgraph registrations (from .pbtxt files)
    ${CMAKE_SOURCE_DIR}/src/mediapipe/generated_subgraphs/*.cc
    # TFLite internal - compiled directly to resolve symbols
    ${CMAKE_SOURCE_DIR}/src/mediapipe/tflite_kernels/quantization_util.cc
    ${CMAKE_SOURCE_DIR}/src/mediapipe/tflite_kernels/telemetry_profiling.cc
    # GPU memory management (NOT OpenGL — used by unified Image type)
    ${CMAKE_SOURCE_DIR}/src/mediapipe/gpu/gpu_buffer.cc
    ${CMAKE_SOURCE_DIR}/src/mediapipe/gpu/gpu_buffer_format.cc
    ${CMAKE_SOURCE_DIR}/src/mediapipe/gpu/gpu_buffer_storage.cc
    ${CMAKE_SOURCE_DIR}/src/mediapipe/gpu/gpu_buffer_storage_image_frame.cc
)

# Generated proto sources
file(GLOB_RECURSE GENERATED_PROTO_SRCS_FINAL ${GENERATED_PROTO_DIR}/*.pb.cc)

# Remove test files and GPU-only profiler
list(FILTER MEDIAPIPE_SRCS EXCLUDE REGEX ".*_test\\.cc$")
list(FILTER MEDIAPIPE_SRCS EXCLUDE REGEX ".*gl_context_profiler\\.cc$")

# Remove ONLY calculators that are face_mesh-specific and NOT used by hand_tracking
# face_mesh graph calculators (face_landmarks_to_render_data)
list(FILTER MEDIAPIPE_SRCS EXCLUDE REGEX ".*face_landmarks_to_render_data_calculator\\.cc$")

# --- Unused core/ calculators (7) ---
list(FILTER MEDIAPIPE_SRCS EXCLUDE REGEX ".*concatenate_vector_calculator\\.cc$")
list(FILTER MEDIAPIPE_SRCS EXCLUDE REGEX ".*immediate_mux_calculator\\.cc$")
list(FILTER MEDIAPIPE_SRCS EXCLUDE REGEX ".*packet_cloner_calculator\\.cc$")
list(FILTER MEDIAPIPE_SRCS EXCLUDE REGEX ".*packet_inner_join_calculator\\.cc$")
list(FILTER MEDIAPIPE_SRCS EXCLUDE REGEX ".*packet_resampler_calculator\\.cc$")
list(FILTER MEDIAPIPE_SRCS EXCLUDE REGEX ".*pass_through_calculator\\.cc$")
list(FILTER MEDIAPIPE_SRCS EXCLUDE REGEX ".*split_proto_list_calculator\\.cc$")

# --- Unused util/ calculators (9) ---
list(FILTER MEDIAPIPE_SRCS EXCLUDE REGEX ".*combine_joints_calculator\\.cc$")
list(FILTER MEDIAPIPE_SRCS EXCLUDE REGEX ".*detection_label_id_to_text_calculator\\.cc$")
list(FILTER MEDIAPIPE_SRCS EXCLUDE REGEX ".*rect_to_render_scale_calculator\\.cc$")
list(FILTER MEDIAPIPE_SRCS EXCLUDE REGEX ".*detection_projection_calculator\\.cc$")
list(FILTER MEDIAPIPE_SRCS EXCLUDE REGEX ".*detection_transformation_calculator\\.cc$")
list(FILTER MEDIAPIPE_SRCS EXCLUDE REGEX ".*filter_detections_calculator\\.cc$")
list(FILTER MEDIAPIPE_SRCS EXCLUDE REGEX ".*landmarks_refinement_calculator\\.cc$")
list(FILTER MEDIAPIPE_SRCS EXCLUDE REGEX ".*landmarks_to_detection_calculator\\.cc$")
list(FILTER MEDIAPIPE_SRCS EXCLUDE REGEX ".*to_image_calculator\\.cc$")

# --- Unused image/ calculators (5) ---
list(FILTER MEDIAPIPE_SRCS EXCLUDE REGEX ".*affine_transformation_runner_opencv\\.cc$")
list(FILTER MEDIAPIPE_SRCS EXCLUDE REGEX ".*color_convert_calculator\\.cc$")
list(FILTER MEDIAPIPE_SRCS EXCLUDE REGEX ".*image_cropping_calculator\\.cc$")
list(FILTER MEDIAPIPE_SRCS EXCLUDE REGEX ".*opencv_encoded_image_to_image_frame_calculator\\.cc$")
list(FILTER MEDIAPIPE_SRCS EXCLUDE REGEX ".*warp_affine_calculator\\.cc$")

# ============================================================
# Create the main executable
# ============================================================
add_executable(hand_tracking_cpu
    ${CMAKE_SOURCE_DIR}/src/main.cc
    ${MEDIAPIPE_SRCS}
    ${GENERATED_PROTO_SRCS_FINAL}
)

# ============================================================
# Collect pre-built .lib files from 3rdparty/ (recursively)
# ============================================================
file(GLOB_RECURSE RUNTIME_LIBS ${THIRDPARTY_DIR}/*/*.lib)

# ============================================================
# Link libraries — exclude:
#   Abseil log stubs (provided by .obj files)
#   FlatBuffers code generators (build-time only)
# ============================================================
set(FILTERED_RUNTIME_LIBS "")
foreach(LIB ${RUNTIME_LIBS})
    # Abseil log stubs (provided by our .obj files)
    if(LIB MATCHES "log_message.lib" OR LIB MATCHES "log_format.lib"
       OR LIB MATCHES "log_sink_set.lib" OR LIB MATCHES "globals.lib"
       OR LIB MATCHES "format.lib"
       OR LIB MATCHES "log_entry.lib" OR LIB MATCHES "log_severity.lib")
        continue()
    endif()
    # FlatBuffers code generators (build-time only)
    if(LIB MATCHES "code_generators.lib" OR LIB MATCHES "generate_fbs.lib")
        continue()
    endif()
    list(APPEND FILTERED_RUNTIME_LIBS ${LIB})
endforeach()

target_link_libraries(hand_tracking_cpu
    ${OPENCV_LIBS}
    ${FILTERED_RUNTIME_LIBS}
    ws2_32 advapi32 userenv dbghelp
)

# /WHOLEARCHIVE forces inclusion of all objects from thin Bazel archives
if(MSVC)
    foreach(LIB ${FILTERED_RUNTIME_LIBS})
        if(LIB MATCHES "tflite/" OR LIB MATCHES "abseil/")
            target_link_options(hand_tracking_cpu PRIVATE "/WHOLEARCHIVE:${LIB}")
        endif()
    endforeach()
endif()

# ============================================================
# Force-include GLog config header on MSVC
# ============================================================
if(MSVC)
    target_compile_options(hand_tracking_cpu PRIVATE
        /FI"${CMAKE_SOURCE_DIR}/src/glog_config.h"
    )
    # Abseil log symbols from real Bazel .obj files
    target_link_options(hand_tracking_cpu PRIVATE
        "${THIRDPARTY_DIR}/abseil_extra_obj/globals.obj"
        "${THIRDPARTY_DIR}/abseil_extra_obj/log_message.obj"
        "${THIRDPARTY_DIR}/abseil_extra_obj/log_format.obj"
        "${THIRDPARTY_DIR}/abseil_extra_obj/log_sink_set.obj"
    )
endif()

# ============================================================
# Copy resource files to output directory
# ============================================================
add_custom_command(TARGET hand_tracking_cpu POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/resource
        $<TARGET_FILE_DIR:hand_tracking_cpu>/resource
    COMMENT "Copying resource files to output directory"
)
# Copy config to exe directory
add_custom_command(TARGET hand_tracking_cpu POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/resource/hand_tracking_config.ini
        $<TARGET_FILE_DIR:hand_tracking_cpu>/hand_tracking_config.ini
    COMMENT "Copying hand_tracking_config.ini to exe directory"
)
# Copy OpenCV DLL to exe directory
add_custom_command(TARGET hand_tracking_cpu POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${OPENCV_DLL}
        $<TARGET_FILE_DIR:hand_tracking_cpu>/opencv_world3410.dll
    COMMENT "Copying OpenCV DLL to output directory"
)

# ============================================================
# Print configuration summary
# ============================================================
message(STATUS "=== hand_tracking_cpu Configuration ===")
message(STATUS "  OpenCV:  ${OPENCV_INCLUDE_DIR}")
message(STATUS "  Protoc:  ${PROTOC_EXECUTABLE}")
message(STATUS "  Protos:  ${PROTO_FILES}")
