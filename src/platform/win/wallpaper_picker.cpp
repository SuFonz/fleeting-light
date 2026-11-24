#include "platform/win/wallpaper_picker.h"

#include <commdlg.h>
#include <objbase.h>
#include <shlobj.h>
#include <shobjidl.h>

#include <vector>

#include "core/log.h"

namespace fl::win {
namespace {

// 直接写死 GUID，避免依赖 MinGW 里 CLSID_FileOpenDialog / IID_IFileOpenDialog 的符号
// {DC1C5A9C-E88A-4DDE-A5A1-60F82A20AEF7}
const CLSID kFileOpenDialogClsid = {0xDC1C5A9C, 0xE88A, 0x4DDE, {0xA5, 0xA1, 0x60, 0xF8, 0x2A, 0x20, 0xAE, 0xF7}};
// {D57C7288-D4AD-4768-BE02-9D969532D960}
const IID kFileOpenDialogIid = {0xD57C7288, 0xD4AD, 0x4768, {0xBE, 0x02, 0x9D, 0x96, 0x95, 0x32, 0xD9, 0x60}};

std::wstring shell_item_path(IShellItem* item) {
  if (!item) return {};
  PWSTR raw = nullptr;
  std::wstring path;
  if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &raw)) && raw) {
    path.assign(raw);
    CoTaskMemFree(raw);
  }
  return path;
}

std::wstring pick_html_file_fallback(HWND owner) {
  wchar_t buffer[MAX_PATH * 2] = {};
  OPENFILENAMEW ofn{};
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = owner;
  ofn.lpstrFilter = L"网页文件 (*.html;*.htm)\0*.html;*.htm\0所有文件 (*.*)\0*.*\0";
  ofn.lpstrFile = buffer;
  ofn.nMaxFile = static_cast<DWORD>(std::size(buffer));
  ofn.lpstrTitle = L"选择壁纸 HTML 文件";
  ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER | OFN_NOCHANGEDIR;
  ofn.lpstrDefExt = L"html";
  if (!GetOpenFileNameW(&ofn)) return {};
  return buffer;
}

std::wstring pick_folder_fallback(HWND owner) {
  BROWSEINFOW info{};
  info.hwndOwner = owner;
  info.lpszTitle = L"选择壁纸文件夹（根目录需包含 index.html）";
  info.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
  PIDLIST_ABSOLUTE id = SHBrowseForFolderW(&info);
  if (!id) return {};
  wchar_t path[MAX_PATH * 2] = {};
  const bool ok = SHGetPathFromIDListW(id, path) != FALSE;
  CoTaskMemFree(id);
  return ok ? std::wstring(path) : std::wstring();
}

}  // namespace

std::string pick_html_file(HWND owner) {
  IFileOpenDialog* dialog = nullptr;
  const HRESULT hr = CoCreateInstance(kFileOpenDialogClsid, nullptr, CLSCTX_INPROC_SERVER, kFileOpenDialogIid,
                                      reinterpret_cast<void**>(&dialog));
  if (FAILED(hr) || !dialog) {
    log_debug("IFileOpenDialog 不可用，退回 GetOpenFileName");
    return to_utf8(pick_html_file_fallback(owner));
  }

  static const COMDLG_FILTERSPEC kFilters[] = {
      {L"网页文件 (*.html;*.htm)", L"*.html;*.htm"},
      {L"所有文件 (*.*)", L"*.*"},
  };

  DWORD options = 0;
  dialog->GetOptions(&options);
  dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_FILEMUSTEXIST | FOS_NOCHANGEDIR);
  dialog->SetFileTypes(static_cast<UINT>(std::size(kFilters)), kFilters);
  dialog->SetTitle(L"选择壁纸 HTML 文件（建议自包含：CSS/JS 内联）");

  std::wstring path;
  if (SUCCEEDED(dialog->Show(owner))) {
    IShellItem* item = nullptr;
    if (SUCCEEDED(dialog->GetResult(&item)) && item) {
      path = shell_item_path(item);
      item->Release();
    }
  }
  dialog->Release();
  return to_utf8(path);
}

std::string pick_folder(HWND owner) {
  IFileOpenDialog* dialog = nullptr;
  const HRESULT hr = CoCreateInstance(kFileOpenDialogClsid, nullptr, CLSCTX_INPROC_SERVER, kFileOpenDialogIid,
                                      reinterpret_cast<void**>(&dialog));
  if (FAILED(hr) || !dialog) {
    log_debug("IFileOpenDialog 不可用，退回 SHBrowseForFolder");
    return to_utf8(pick_folder_fallback(owner));
  }

  DWORD options = 0;
  dialog->GetOptions(&options);
  dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_PICKFOLDERS | FOS_NOCHANGEDIR);
  dialog->SetTitle(L"选择壁纸文件夹（根目录需包含 index.html）");

  std::wstring path;
  if (SUCCEEDED(dialog->Show(owner))) {
    IShellItem* item = nullptr;
    if (SUCCEEDED(dialog->GetResult(&item)) && item) {
      path = shell_item_path(item);
      item->Release();
    }
  }
  dialog->Release();
  return to_utf8(path);
}

}  // namespace fl::win
