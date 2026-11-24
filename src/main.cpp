// 入口：单实例、DPI、COM 初始化，然后交给平台层跑消息循环。
#include <objbase.h>

#include <string>

#include "core/log.h"
#include "core/platform_env.h"
#include "platform/win/win_app.h"
#include "platform/win/win_util.h"

namespace {

constexpr const wchar_t* kMutexName = L"Local\\FleetingLight.SingleInstance";
constexpr const wchar_t* kActivateMessage = L"FleetingLight.Activate";

struct LaunchOptions {
  bool open_settings = false;
  bool dev = false;
};

LaunchOptions parse_command_line(const std::wstring& command_line) {
  LaunchOptions options;
  const std::wstring lowered = [](std::wstring_view text) {
    std::wstring out(text);
    for (wchar_t& c : out) c = static_cast<wchar_t>(towlower(c));
    return out;
  }(command_line);
  if (lowered.find(L"--settings") != std::wstring::npos) options.open_settings = true;
  if (lowered.find(L"--dev") != std::wstring::npos) options.dev = true;
  return options;
}

bool has_environment_variable(const wchar_t* name) {
  wchar_t buffer[8] = {};
  return GetEnvironmentVariableW(name, buffer, static_cast<DWORD>(std::size(buffer))) > 0;
}

void ensure_dpi_awareness() {
  using SetDpiAwarenessContextFn = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);
  HMODULE user32 = GetModuleHandleW(L"user32.dll");
  auto set_context = user32 ? reinterpret_cast<SetDpiAwarenessContextFn>(
                                  GetProcAddress(user32, "SetProcessDpiAwarenessContext"))
                            : nullptr;
  // -4 == DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
  if (set_context && set_context(reinterpret_cast<DPI_AWARENESS_CONTEXT>(-4))) return;
  SetProcessDPIAware();
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR command_line, int) {
  ensure_dpi_awareness();

  HANDLE mutex = CreateMutexW(nullptr, TRUE, kMutexName);
  const bool already_running = mutex != nullptr && GetLastError() == ERROR_ALREADY_EXISTS;
  const UINT activate = RegisterWindowMessageW(kActivateMessage);
  if (already_running) {
    if (activate) PostMessageW(HWND_BROADCAST, activate, 0, 0);
    if (mutex) CloseHandle(mutex);
    return 0;
  }

  const LaunchOptions options = parse_command_line(command_line ? command_line : L"");
  const bool dev = options.dev || has_environment_variable(L"FL_DEBUG") || has_environment_variable(L"FL_DEV");
  fl::set_log_min_level(dev ? fl::LogLevel::Debug : fl::LogLevel::Info);
  fl::log_info("Fleeting Light 启动（version " + fl::env::app_version() + "）");

  const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
  if (FAILED(com)) {
    MessageBoxW(nullptr, L"COM 初始化失败", L"Fleeting Light", MB_ICONERROR);
    return 1;
  }

  bool first_run = !fl::env::file_exists(fl::env::join_path(fl::env::user_data_dir(), "config.json"));

  int exit_code = 0;
  {
    fl::win::WinApp app;
    exit_code = app.run(instance, options.open_settings || first_run);
  }

  CoUninitialize();
  if (mutex) CloseHandle(mutex);
  return exit_code;
}
