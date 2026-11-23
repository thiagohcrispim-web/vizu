# CMakeLists.txt - Build system moderno para Vizu (substitui build.sh)
# Requer CMake 3.20+, MinGW-w64 no Linux ou MSVC no Windows
cmake_minimum_required(VERSION 3.20)
project(Vizu
    VERSION 9.14.4
    LANGUAGES CXX C RC
    DESCRIPTION "Vizu - Quick Look para Windows Explorer"
)

# ============================================================================
# Configurações globais
# ============================================================================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Políticas modernas
cmake_policy(SET CMP0077 NEW)  # option() honors normal variables
cmake_policy(SET CMP0091 NEW)  # FILE_SET
cmake_policy(SET CMP0108 NEW)  # try_compile() source file signature

# Configurações de build
set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
set(CMAKE_CONFIGURATION_TYPES "Release;Debug" CACHE STRING "" FORCE)

# Flags de compilador
if(MSVC)
    add_compile_options(
        /W4 /WX /permissive- /Zc:__cplusplus /Zc:inline /Zc:throwingNew
        /MP /O2 /Ob2 /Oi /Ot /Oy /GL
        /DWIN32_LEAN_AND_MEAN /DNOMINMAX
        /DNTDDI_VERSION=0x0A000003
        /D_CRT_SECURE_NO_WARNINGS /D_SCL_SECURE_NO_WARNINGS
    )
    add_link_options(/LTCG /OPT:REF /OPT:ICF /DYNAMICBASE /NXCOMPAT)
else()  # MinGW
    add_compile_options(
        -Wall -Wextra -Wpedantic -Werror -Wno-unused-parameter
        -std=c++20 -pipe -O2 -flto -fno-exceptions -fno-rtti
        -DWIN32_LEAN_AND_MEAN -DNOMINMAX
        -DNTDDI_VERSION=0x0A000003
    )
    add_link_options(-flto -Wl,--gc-sections -Wl,--strip-all -static-libgcc -static-libstdc++)
endif()

# Definições de versão
add_compile_definitions(
    VIZU_VERSION_MAJOR=9
    VIZU_VERSION_MINOR=14
    VIZU_VERSION_PATCH=4
    VIZU_VERSION_STRING="9.14.4"
    UNICODE
    _UNICODE
)

# ============================================================================
# Encontra dependências
# ============================================================================

# Windows SDK
find_package(WindowsSDK REQUIRED)

# Direct2D / DirectWrite / WIC
find_library(D2D1 d2d1)
find_library(DWRITE dwrite)
find_library(WINCODEC windowscodecs)
find_library(DWMAPI dwmapi)
find_library(SHLWAPI shlwapi)
find_library(VERSION version)
find_library(OLE32 ole32)
find_library(OLEAUT32 oleaut32)
find_library(UUID rpcrt4)
find_library(COMCTL32 comctl32)
find_library(GDI32 gdi32)
find_library(USER32 user32)
find_library(SHELL32 shell32)
find_library(ADVAPI32 advapi32)
find_library(PSAPI psapi)
find_library(PDH pdh)

# ============================================================================
# Dependências vendored (vcpkg ou FetchContent)
# ============================================================================

option(VIZU_USE_VCPKG "Use vcpkg for dependencies" ON)
option(VIZU_VENDOR_DEPS "Vendor dependencies locally" OFF)

if(VIZU_USE_VCPKG AND NOT VIZU_VENDOR_DEPS)
    # vcpkg toolchain
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")

    find_package(libwebp CONFIG REQUIRED)
    find_package(pdfium CONFIG REQUIRED)
    find_package(libcdr CONFIG REQUIRED)
    find_package(lcms2 CONFIG REQUIRED)
    find_package(zlib CONFIG REQUIRED)
    find_package(Boost CONFIG REQUIRED COMPONENTS system filesystem thread)
else()
    # Vendored dependencies (local)
    message(STATUS "Using vendored dependencies")
    add_subdirectory(vendor/libwebp)
    add_subdirectory(vendor/pdfium)
    add_subdirectory(vendor/libcdr)
    add_subdirectory(vendor/lcms2)
    add_subdirectory(vendor/zlib)
    add_subdirectory(vendor/boost)
endif()

# ============================================================================
# Fontes principais
# ============================================================================

set(VIZU_SOURCES
    # Core - monolithic Vizu.cpp contains all functionality including the installer fix
    Vizu.cpp
)

# Headers (para IDE)
set(VIZU_HEADERS
    Config.h
    ExplorerIntegration.h
    BackgroundLoader.h
    ThemeManager.h
    AltMenu.h
    Dialogs.h
    ConfigModern.h
    BackgroundLoaderOptimized.h
    AltMenuModern.h
    Resource.h
    CacheOptimized.h
    ModernCpp.h
    Formats.h
    Convert.h
    Ops.h
    PaletteFont.h
    VectorBundle.h
    VectorConvert.h
    CdrVector.h
    Jobs.h
    Clean.h
    Ocr.h
    Update.h
    InstallerUi.h
    ColorInfo.h
    Ui.h
    i18n.h
)

# ============================================================================
# Executável principal
# ============================================================================
add_executable(Vizu WIN32 ${VIZU_SOURCES} ${VIZU_HEADERS} Vizu.rc)

target_include_directories(Vizu PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(Vizu PRIVATE
    ${D2D1} ${DWRITE} ${WINCODEC} ${DWMAPI}
    ${SHLWAPI} ${VERSION} ${OLE32} ${OLEAUT32} ${UUID}
    ${COMCTL32} ${GDI32} ${USER32} ${SHELL32} ${ADVAPI32} ${PSAPI} ${PDH}
    WebP::webp WebP::webpdemux
    pdfium
    libcdr::libcdr
    lcms2::lcms2
    zlib::zlib
    Boost::system Boost::filesystem Boost::thread
)

# Recursos do Windows
target_link_libraries(Vizu PRIVATE ${CMAKE_RC_COMPILER})

# Propriedades do executável
set_target_properties(Vizu PROPERTIES
    OUTPUT_NAME "Vizu"
    WIN32_EXECUTABLE TRUE
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    LINKER_LANGUAGE CXX
)

# ============================================================================
# DLL de miniaturas (DesignThumbs.dll)
# ============================================================================
# Note: DesignThumbs.cpp includes Formats.h which has inline implementations
# For now, we build only DesignThumbs.cpp which pulls in all format handling
add_library(DesignThumbs SHARED
    DesignThumbs.cpp
)

target_include_directories(DesignThumbs PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(DesignThumbs PRIVATE
    ${D2D1} ${DWRITE} ${WINCODEC} ${DWMAPI}
    ${SHLWAPI} ${VERSION} ${OLE32} ${OLEAUT32} ${UUID}
    WebP::webp WebP::webpdemux
    pdfium
    libcdr::libcdr
    lcms2::lcms2
    zlib::zlib
    Boost::system Boost::filesystem Boost::thread
)

set_target_properties(DesignThumbs PROPERTIES
    OUTPUT_NAME "DesignThumbs"
    PREFIX ""
    SUFFIX ".dll"
    WINDOWS_EXPORT_ALL_SYMBOLS TRUE
)

# ============================================================================
# Testes
# ============================================================================
enable_testing()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

# ============================================================================
# Instalação
# ============================================================================
install(TARGETS Vizu DesignThumbs
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION bin
    ARCHIVE DESTINATION lib
)

install(FILES
    fonts/Inter-Regular.ttf
    fonts/Inter-Medium.ttf
    fonts/Inter-SemiBold.ttf
    fonts/Inter-Bold.ttf
    fonts/SpaceGrotesk-Regular.ttf
    fonts/SpaceGrotesk-Medium.ttf
    fonts/SpaceGrotesk-SemiBold.ttf
    fonts/SpaceGrotesk-Bold.ttf
    DESTINATION share/vizu/fonts
)

# ============================================================================
# Empacotamento (CPack)
# ============================================================================
set(CPACK_GENERATOR "NSIS")
set(CPACK_PACKAGE_NAME "Vizu")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VENDOR "Thiago Crispim")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Quick Look para Windows Explorer")
set(CPACK_NSIS_INSTALL_ROOT "$LOCALAPPDATA\\Programs\\DesignThumbs")
set(CPACK_NSIS_MODIFY_PATH OFF)
set(CPACK_NSIS_DISPLAY_NAME "Vizu ${PROJECT_VERSION}")
set(CPACK_NSIS_HELP_LINK "https://github.com/thiagohcrispim-web/vizu")
set(CPACK_NSIS_URL_INFO_ABOUT "https://github.com/thiagohcrispim-web/vizu")
set(CPACK_NSIS_CONTACT "thiago@crispim.dev")
set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/Vizu.ico")
set(CPACK_NSIS_MUI_UNIICON "${CMAKE_SOURCE_DIR}/Vizu.ico")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_ICON "${CMAKE_SOURCE_DIR}/Vizu.ico")

include(CPack)

# ============================================================================
# Targets customizados
# ============================================================================

# Installer preview
add_custom_target(installer-preview
    COMMAND ${CMAKE_COMMAND} -E echo "Building installer preview..."
    COMMAND Vizu /installer-preview
    DEPENDS Vizu
)

# Release package
add_custom_target(package-release
    COMMAND ${CMAKE_COMMAND} --build . --target package
    DEPENDS Vizu DesignThumbs
)

# ============================================================================
# Configuração para cross-compile Linux -> Windows
# ============================================================================
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    # Toolchain para MinGW
    set(CMAKE_SYSTEM_NAME Windows)
    set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
    set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
    set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)
    set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)
    set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
    set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
    set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
    set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
endif()