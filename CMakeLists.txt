# ============================================================================
# CMakeLists.txt - WebView2 Hello World (MFC Dialog)
#
# Build:
#   1) mkdir build && cd build
#   2) cmake .. -G "Visual Studio 17 2022" -A x64
#   3) cmake --build . --config Release
# ============================================================================

cmake_minimum_required(VERSION 3.20)
project(WebView2HelloWorld LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================================
# MFC Configuration (Shared DLL)
# ============================================================================
set(CMAKE_MFC_FLAG 2)

# ============================================================================
# NuGet Packages Auto-Download
# ============================================================================
set(NUGET_PACKAGES_DIR "${CMAKE_BINARY_DIR}/packages")

# Download nuget.exe
set(NUGET_EXE "${CMAKE_BINARY_DIR}/nuget.exe")
if(NOT EXISTS "${NUGET_EXE}")
    message(STATUS "Downloading nuget.exe...")
    file(DOWNLOAD
        "https://dist.nuget.org/win-x86-commandline/latest/nuget.exe"
        "${NUGET_EXE}"
    )
endif()

# Microsoft.Web.WebView2 SDK
set(WEBVIEW2_VERSION "1.0.3065.39")
set(WEBVIEW2_PKG_DIR "${NUGET_PACKAGES_DIR}/Microsoft.Web.WebView2.${WEBVIEW2_VERSION}")
if(NOT EXISTS "${WEBVIEW2_PKG_DIR}")
    message(STATUS "Installing Microsoft.Web.WebView2 ${WEBVIEW2_VERSION}...")
    execute_process(
        COMMAND "${NUGET_EXE}" install Microsoft.Web.WebView2
                -Version ${WEBVIEW2_VERSION}
                -OutputDirectory "${NUGET_PACKAGES_DIR}"
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    )
endif()

# Microsoft.Windows.ImplementationLibrary (WIL)
set(WIL_VERSION "1.0.240803.1")
set(WIL_PKG_DIR "${NUGET_PACKAGES_DIR}/Microsoft.Windows.ImplementationLibrary.${WIL_VERSION}")
if(NOT EXISTS "${WIL_PKG_DIR}")
    message(STATUS "Installing Microsoft.Windows.ImplementationLibrary ${WIL_VERSION}...")
    execute_process(
        COMMAND "${NUGET_EXE}" install Microsoft.Windows.ImplementationLibrary
                -Version ${WIL_VERSION}
                -OutputDirectory "${NUGET_PACKAGES_DIR}"
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    )
endif()

# ============================================================================
# Source Files
# ============================================================================
set(SOURCES
    pch.cpp
    WebView2App.cpp
    WebView2.cpp
    WebView2Dlg.cpp
)

set(HEADERS
    pch.h
    targetver.h
    resource.h
    WebView2App.h
    WebView2.h
    WebView2Dlg.h
)

set(RESOURCES
    WebView2HelloWorld.rc
)

# ============================================================================
# Executable (WIN32 = /SUBSYSTEM:WINDOWS)
# ============================================================================
add_executable(WebView2HelloWorld WIN32
    ${SOURCES}
    ${HEADERS}
    ${RESOURCES}
)

# ============================================================================
# Precompiled Header
# ============================================================================
target_precompile_headers(WebView2HelloWorld PRIVATE pch.h)

# ============================================================================
# Include / Library paths
# ============================================================================
set(WEBVIEW2_INCLUDE "${WEBVIEW2_PKG_DIR}/build/native/include")
set(WEBVIEW2_LIB_DIR "${WEBVIEW2_PKG_DIR}/build/native/x64")
set(WIL_INCLUDE "${WIL_PKG_DIR}/include")

target_include_directories(WebView2HelloWorld PRIVATE
    "${CMAKE_SOURCE_DIR}"
    "${WEBVIEW2_INCLUDE}"
    "${WIL_INCLUDE}"
)

target_link_directories(WebView2HelloWorld PRIVATE
    "${WEBVIEW2_LIB_DIR}"
)

target_link_libraries(WebView2HelloWorld PRIVATE
    WebView2LoaderStatic.lib
)

# ============================================================================
# Compiler Settings
# ============================================================================
target_compile_definitions(WebView2HelloWorld PRIVATE
    _UNICODE
    UNICODE
    WIN32
    _WINDOWS
    _AFXDLL
)

# MSVC UTF-8 source encoding
if(MSVC)
    target_compile_options(WebView2HelloWorld PRIVATE /utf-8)
endif()
