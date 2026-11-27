#pragma once

#include <atomic>
#include <format>
#include <iostream>
#include <memory>
#include <string_view>

namespace ai::logger {

enum class LogLevel {
  kLogLevelDebug = 0,
  kLogLevelInfo = 1,
  kLogLevelWarn = 2,
  kLogLevelError = 3
};

class Logger {
 public:
  virtual ~Logger() = default;

  // Core logging method
  virtual void log(LogLevel level, std::string_view message) = 0;

  // Convenience methods for each log level
  template <typename... Args>
  void debug(std::format_string<Args...> fmt, Args&&... args) {
    if (is_enabled(LogLevel::kLogLevelDebug)) {
      log(LogLevel::kLogLevelDebug,
          std::format(fmt, std::forward<Args>(args)...));
    }
  }

  template <typename... Args>
  void info(std::format_string<Args...> fmt, Args&&... args) {
    if (is_enabled(LogLevel::kLogLevelInfo)) {
      log(LogLevel::kLogLevelInfo,
          std::format(fmt, std::forward<Args>(args)...));
    }
  }

  template <typename... Args>
  void warn(std::format_string<Args...> fmt, Args&&... args) {
    if (is_enabled(LogLevel::kLogLevelWarn)) {
      log(LogLevel::kLogLevelWarn,
          std::format(fmt, std::forward<Args>(args)...));
    }
  }

  template <typename... Args>
  void error(std::format_string<Args...> fmt, Args&&... args) {
    if (is_enabled(LogLevel::kLogLevelError)) {
      log(LogLevel::kLogLevelError,
          std::format(fmt, std::forward<Args>(args)...));
    }
  }

  // Check if a log level is enabled
  virtual bool is_enabled(LogLevel level) const = 0;
};

// Null logger implementation - does nothing
class NullLogger final : public Logger {
 public:
  void log(LogLevel, std::string_view) override {}
  bool is_enabled(LogLevel) const override { return false; }
};

class ConsoleLogger final : public Logger {
 public:
  explicit ConsoleLogger(LogLevel min_level = LogLevel::kLogLevelInfo)
      : min_level_(min_level) {}

  void log(LogLevel level, std::string_view message) override {
    if (!is_enabled(level)) {
      return;
    }

    auto& stream = (level == LogLevel::kLogLevelError) ? std::cerr : std::cout;
    stream << std::format("[{}] {}\n", level_to_string(level), message);
    stream.flush();
  }

  bool is_enabled(LogLevel level) const override {
    return static_cast<int>(level) >= static_cast<int>(min_level_);
  }

  void set_min_level(LogLevel level) { min_level_ = level; }

 private:
  static constexpr std::string_view level_to_string(LogLevel level) {
    switch (level) {
      case LogLevel::kLogLevelDebug:
        return "DEBUG";
      case LogLevel::kLogLevelInfo:
        return "INFO";
      case LogLevel::kLogLevelWarn:
        return "WARN";
      case LogLevel::kLogLevelError:
        return "ERROR";
    }
    return "UNKNOWN";
  }

  LogLevel min_level_;
};

namespace detail {

#if defined(__cpp_lib_atomic_shared_ptr)

inline std::atomic<std::shared_ptr<Logger>>& logger_instance() {
  static std::atomic<std::shared_ptr<Logger>> instance(
      std::make_shared<ConsoleLogger>());
  return instance;
}

inline std::shared_ptr<Logger> load_logger() {
  return logger_instance().load();
}

inline void store_logger(std::shared_ptr<Logger> logger) {
  logger_instance().store(std::move(logger));
}

#else
// Fall back to the atomic shared_ptr free functions on standard libraries
// that have not implemented std::atomic<std::shared_ptr> (notably libc++).

inline std::shared_ptr<Logger>& logger_instance() {
  static std::shared_ptr<Logger> instance = std::make_shared<ConsoleLogger>();
  return instance;
}

inline std::shared_ptr<Logger> load_logger() {
  return std::atomic_load(&logger_instance());
}

inline void store_logger(std::shared_ptr<Logger> logger) {
  std::atomic_store(&logger_instance(), std::move(logger));
}

#endif

}  // namespace detail

inline void install_logger(std::shared_ptr<Logger> logger) {
  if (logger) {
    detail::store_logger(std::move(logger));
  }
}

/// Returns the currently installed logger. The reference stays valid until
/// the next logger()/log_* call on the same thread; do not cache it across
/// calls that may log.
inline Logger& logger() {
  thread_local std::shared_ptr<Logger> ptr;
  ptr = detail::load_logger();
  return *ptr;
}

template <typename... Args>
inline void log_debug(std::format_string<Args...> fmt, Args&&... args) {
  logger().debug(fmt, std::forward<Args>(args)...);
}

template <typename... Args>
inline void log_info(std::format_string<Args...> fmt, Args&&... args) {
  logger().info(fmt, std::forward<Args>(args)...);
}

template <typename... Args>
inline void log_warn(std::format_string<Args...> fmt, Args&&... args) {
  logger().warn(fmt, std::forward<Args>(args)...);
}

template <typename... Args>
inline void log_error(std::format_string<Args...> fmt, Args&&... args) {
  logger().error(fmt, std::forward<Args>(args)...);
}

}  // namespace ai::logger
