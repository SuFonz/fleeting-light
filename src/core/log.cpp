#include "core/log.h"

#include <ctime>
#include <cstdio>
#include <mutex>

#include "core/platform_env.h"

namespace fl {
namespace {

LogLevel g_min_level = LogLevel::Info;
std::mutex g_mutex;

const char* level_name(LogLevel level) {
  switch (level) {
    case LogLevel::Debug: return "DEBUG";
    case LogLevel::Info: return "INFO ";
    case LogLevel::Warn: return "WARN ";
    case LogLevel::Error: return "ERROR";
  }
  return "INFO ";
}

bool log_to_file = true;

}  // namespace

void set_log_min_level(LogLevel level) { g_min_level = level; }

void log_write(LogLevel level, std::string_view message) {
  if (static_cast<int>(level) < static_cast<int>(g_min_level)) return;

  std::time_t now = std::time(nullptr);
  std::tm local{};
#if defined(_WIN32)
  localtime_s(&local, &now);
#else
  localtime_r(&now, &local);
#endif
  char stamp[32];
  std::snprintf(stamp, sizeof(stamp), "%02d:%02d:%02d", local.tm_hour, local.tm_min, local.tm_sec);

  std::string line;
  line.reserve(message.size() + 32);
  line += '[';
  line += stamp;
  line += "] ";
  line += level_name(level);
  line += " ";
  line.append(message.data(), message.size());
  line += "\n";

  std::lock_guard<std::mutex> lock(g_mutex);
#if defined(_WIN32)
  std::fputs(line.c_str(), stderr);
#endif
  if (log_to_file) {
    static bool dir_checked = false;
    if (!dir_checked) {
      dir_checked = true;
      env::ensure_dir(env::user_data_dir());
    }
    std::string err;
    env::write_file(env::join_path(env::user_data_dir(), "fleeting-light.log"), line, env::FileMode::Append, &err);
  }
}

void log_debug(std::string_view message) { log_write(LogLevel::Debug, message); }
void log_info(std::string_view message) { log_write(LogLevel::Info, message); }
void log_warn(std::string_view message) { log_write(LogLevel::Warn, message); }
void log_error(std::string_view message) { log_write(LogLevel::Error, message); }

}  // namespace fl
