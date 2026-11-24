#pragma once

#include <string>
#include <string_view>

namespace fl {

enum class LogLevel { Debug, Info, Warn, Error };

void set_log_min_level(LogLevel level);
void log_write(LogLevel level, std::string_view message);

void log_debug(std::string_view message);
void log_info(std::string_view message);
void log_warn(std::string_view message);
void log_error(std::string_view message);

}  // namespace fl
