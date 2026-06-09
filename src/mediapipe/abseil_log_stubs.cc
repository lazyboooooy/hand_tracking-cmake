// Minimal stubs for remaining Abseil log symbols not covered by .obj files.
// These functions return safe defaults for basic logging functionality.

#include <string_view>

// Forward-declare only what we need
namespace absl {

enum class LogSeverityAtLeast : int { kInfo = 0, kWarning = 1, kError = 2, kFatal = 3 };

// Called by log_message - returns minimum severity to log
LogSeverityAtLeast MinLogLevel() {
  return LogSeverityAtLeast::kInfo;
}

// Called by make_unique<LogMessageData> - returns whether to prepend log prefix
bool ShouldPrependLogPrefix() {
  return true;
}

// Called by stderr log sink - returns stderr threshold
LogSeverityAtLeast StderrThreshold() {
  return LogSeverityAtLeast::kInfo;
}

namespace log_internal {

// Called by log_message - returns whether to log backtrace at a given location
// Note: must be in absl::log_internal namespace to match the declaration
// in log_message.obj (Bazel-compiled Abseil)
bool ShouldLogBacktraceAt(std::string_view, int) {
  return false;
}

}  // namespace log_internal

}  // namespace absl
