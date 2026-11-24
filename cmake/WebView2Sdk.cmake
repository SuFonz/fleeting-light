# Make sure third_party/webview2 contains everything the build needs:
#   include/WebView2.h, include/WebView2EnvironmentOptions.h
#   include/WebView2UUIDs.h   (MinGW only, generated)
#   bin/x64/WebView2Loader.dll
#
# When something is missing, the Microsoft.Web.WebView2 NuGet package is downloaded,
# extracted into .cache/webview2 and the required files are copied out.
# Offline builds can pass -DFL_WEBVIEW2_LOCAL_PACKAGE=<path/to/*.nupkg>.
#
# Usage (from CMakeLists.txt):
#   include(cmake/WebView2Sdk.cmake)
#   fl_ensure_webview2_sdk()

set(FL_GEN_UUIDS_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/../tools/gen_webview2_uuids.cmake")

function(fl_ensure_webview2_sdk)
  set(sdk_dir "${FL_WEBVIEW2_DIR}")
  set(include_dir "${sdk_dir}/include")
  set(bin_dir "${sdk_dir}/bin/x64")
  set(header "${include_dir}/WebView2.h")
  set(uuids "${include_dir}/WebView2UUIDs.h")
  set(loader "${bin_dir}/WebView2Loader.dll")

  if(EXISTS "${header}" AND EXISTS "${uuids}" AND EXISTS "${loader}")
    return()
  endif()

  message(STATUS "WebView2 SDK: third_party/webview2 is incomplete, preparing (missing files are fetched automatically)")

  if(NOT EXISTS "${header}" OR NOT EXISTS "${loader}")
    set(cache_dir "${CMAKE_SOURCE_DIR}/.cache/webview2")
    file(MAKE_DIRECTORY "${cache_dir}")

    # ---- 1) get the nupkg: local package first, then cache, then download ----
    if(FL_WEBVIEW2_LOCAL_PACKAGE AND EXISTS "${FL_WEBVIEW2_LOCAL_PACKAGE}")
      set(package "${FL_WEBVIEW2_LOCAL_PACKAGE}")
      set(package_key "local")
      message(STATUS "WebView2 SDK: using local package ${package}")
    else()
      if(FL_WEBVIEW2_VERSION)
        set(package_key "${FL_WEBVIEW2_VERSION}")
        set(package "${cache_dir}/Microsoft.Web.WebView2.${FL_WEBVIEW2_VERSION}.nupkg")
        set(package_url "https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/${FL_WEBVIEW2_VERSION}")
      else()
        set(package_key "latest")
        set(package "${cache_dir}/Microsoft.Web.WebView2.latest.nupkg")
        set(package_url "https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/")
      endif()

      if(NOT EXISTS "${package}")
        message(STATUS "WebView2 SDK: downloading ${package_url}")
        file(DOWNLOAD "${package_url}" "${package}"
             STATUS download_status
             SHOW_PROGRESS)
        list(GET download_status 0 download_code)
        list(GET download_status 1 download_detail)
        if(NOT download_code EQUAL 0)
          file(REMOVE "${package}")
          message(FATAL_ERROR
            "Failed to download the WebView2 SDK: ${download_code} ${download_detail}\n"
            "Download it manually from https://www.nuget.org/packages/Microsoft.Web.WebView2/ and run:\n"
            "  cmake -S . -B build -DFL_WEBVIEW2_LOCAL_PACKAGE=<path to .nupkg>\n"
            "Or extract the nupkg, copy build/native/include/*.h to ${include_dir}\n"
            "and x64/WebView2Loader.dll to ${bin_dir}")
        endif()
      endif()
    endif()

    # ---- 2) extract into the cache (reused when already extracted) ----
    set(extract_dir "${cache_dir}/sdk-${package_key}")
    if(NOT EXISTS "${extract_dir}/build/native/include/WebView2.h")
      message(STATUS "WebView2 SDK: extracting to ${extract_dir}")
      file(REMOVE_RECURSE "${extract_dir}")
      file(ARCHIVE_EXTRACT INPUT "${package}" DESTINATION "${extract_dir}")
    endif()

    # ---- 3) copy the files we need ----
    if(NOT EXISTS "${extract_dir}/build/native/include/WebView2.h")
      message(FATAL_ERROR "Package has no build/native/include/WebView2.h, layout may have changed: ${package}")
    endif()
    if(NOT EXISTS "${extract_dir}/build/native/x64/WebView2Loader.dll")
      message(FATAL_ERROR "Package has no build/native/x64/WebView2Loader.dll: ${package}")
    endif()

    file(MAKE_DIRECTORY "${include_dir}" "${bin_dir}")
    file(COPY "${extract_dir}/build/native/include/WebView2.h" DESTINATION "${include_dir}")
    if(EXISTS "${extract_dir}/build/native/include/WebView2EnvironmentOptions.h")
      file(COPY "${extract_dir}/build/native/include/WebView2EnvironmentOptions.h" DESTINATION "${include_dir}")
    endif()
    file(COPY "${extract_dir}/build/native/x64/WebView2Loader.dll" DESTINATION "${bin_dir}")
    if(EXISTS "${extract_dir}/LICENSE.txt")
      file(COPY "${extract_dir}/LICENSE.txt" DESTINATION "${sdk_dir}")
    endif()
    message(STATUS "WebView2 SDK: extracted into ${sdk_dir}")
  endif()

  # ---- 4) generate the IID header MinGW needs ----
  if(NOT EXISTS "${uuids}")
    if(NOT EXISTS "${header}")
      message(FATAL_ERROR "Missing ${header}, cannot generate WebView2UUIDs.h")
    endif()
    message(STATUS "WebView2 SDK: generating WebView2UUIDs.h")
    execute_process(
      COMMAND "${CMAKE_COMMAND}"
              "-DHEADER=${header}"
              "-DOUTPUT=${uuids}"
              -P "${FL_GEN_UUIDS_SCRIPT}"
      RESULT_VARIABLE gen_result)
    if(NOT gen_result EQUAL 0 OR NOT EXISTS "${uuids}")
      message(FATAL_ERROR "Failed to generate ${uuids} (exit code ${gen_result})")
    endif()
  endif()
endfunction()
