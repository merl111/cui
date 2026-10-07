cmake_minimum_required(VERSION 3.20)
enable_language(RC)
# WinUI is built as a C ABI DLL by MSBuild; CMake consumers remain ordinary C.
if(NOT MSVC)
    message(FATAL_ERROR "CUI's Windows backend is WinUI 3. Build cui.dll with Visual Studio 2022/MSVC; MinGW/Go/Zig consumers can link the resulting C ABI DLL.")
endif()
if(CUI_ENABLE_SANITIZERS OR CUI_BUILD_FUZZERS)
    message(FATAL_ERROR "CUI's WinUI build does not support these sanitizer/fuzzer options.")
endif()
if(NOT CUI_BUILD_SHARED)
    message(FATAL_ERROR "The WinUI backend requires CUI_BUILD_SHARED=ON. Windows consumers link cui.lib (a DLL import library), not a static CUI archive.")
endif()
find_program(CUI_NUGET nuget REQUIRED)
if(CMAKE_VS_MSBUILD_COMMAND)
    set(CUI_MSBUILD "${CMAKE_VS_MSBUILD_COMMAND}")
else()
    find_program(CUI_MSBUILD MSBuild REQUIRED)
endif()
set(CUI_WINUI_PACKAGES "${CMAKE_BINARY_DIR}/winui-packages" CACHE PATH "Pinned NuGet package cache (may be restored in advance)")
set(CUI_WINDOWS_SDK_VERSION "10.0" CACHE STRING "Installed Windows SDK version for the WinUI build")
if(CMAKE_VS_PLATFORM_NAME)
    set(CUI_WINUI_PLATFORM "${CMAKE_VS_PLATFORM_NAME}")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(CUI_WINUI_PLATFORM x64)
else()
    set(CUI_WINUI_PLATFORM Win32)
endif()
if(NOT CUI_WINUI_PLATFORM MATCHES "^(x64|ARM64|Win32)$")
    message(FATAL_ERROR "Unsupported WinUI architecture: ${CUI_WINUI_PLATFORM}")
endif()
set(CUI_WINUI_OUTPUT "${CMAKE_BINARY_DIR}")
set(CUI_WINUI_SOURCES
    src/cui_winui.cpp src/cui_winui_controls.cpp src/cui_winui_input.cpp
    src/cui_winui_tables.cpp src/cui_winui_draw.cpp src/cui_winui_desktop.cpp
    src/cui_winui_window.c src/cui_windows_assets.c src/cui_windows_text.c src/cui_win32_files.c)
set(CUI_WINUI_COMPILE_ITEMS "")
foreach(SOURCE IN LISTS CUI_CORE_SOURCES CUI_WINUI_SOURCES)
    string(APPEND CUI_WINUI_COMPILE_ITEMS "    <ClCompile Include=\"${PROJECT_SOURCE_DIR}/${SOURCE}\" />\n")
endforeach()
configure_file(windows/cui.vcxproj.in "${CMAKE_BINARY_DIR}/cui-winui.vcxproj" @ONLY)
set(CUI_WINUI_CONFIG "$<IF:$<BOOL:$<CONFIG>>,$<CONFIG>,Release>")
add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/winui-restore.stamp"
    COMMAND "${CUI_NUGET}" restore "${PROJECT_SOURCE_DIR}/windows/packages.config"
            -PackagesDirectory "${CUI_WINUI_PACKAGES}" -NonInteractive
    COMMAND ${CMAKE_COMMAND} -E touch "${CMAKE_BINARY_DIR}/winui-restore.stamp"
    DEPENDS windows/packages.config VERBATIM)
add_custom_target(cui_winui_build ALL
    COMMAND "${CUI_MSBUILD}" "${CMAKE_BINARY_DIR}/cui-winui.vcxproj" /m
            "/p:Configuration=${CUI_WINUI_CONFIG}" "/p:Platform=${CUI_WINUI_PLATFORM}"
    DEPENDS "${CMAKE_BINARY_DIR}/winui-restore.stamp"
    BYPRODUCTS "${CMAKE_BINARY_DIR}/${CUI_WINUI_CONFIG}/cui.dll" "${CMAKE_BINARY_DIR}/${CUI_WINUI_CONFIG}/cui.lib"
    VERBATIM)
add_library(cui SHARED IMPORTED GLOBAL)
set_target_properties(cui PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${PROJECT_SOURCE_DIR}/include"
    IMPORTED_IMPLIB "${CMAKE_BINARY_DIR}/Release/cui.lib"
    IMPORTED_LOCATION "${CMAKE_BINARY_DIR}/Release/cui.dll")
foreach(CONFIG Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${CONFIG}" UPPER_CONFIG)
    set_property(TARGET cui APPEND PROPERTY IMPORTED_CONFIGURATIONS "${CONFIG}")
    set_target_properties(cui PROPERTIES
        "IMPORTED_IMPLIB_${UPPER_CONFIG}" "${CMAKE_BINARY_DIR}/${CONFIG}/cui.lib"
        "IMPORTED_LOCATION_${UPPER_CONFIG}" "${CMAKE_BINARY_DIR}/${CONFIG}/cui.dll")
endforeach()
add_dependencies(cui cui_winui_build)
add_library(cui_shared ALIAS cui)
function(cui_windows_executable NAME)
    add_executable(${NAME} ${ARGN} examples/windows.rc)
    target_include_directories(${NAME} PRIVATE examples)
    target_compile_features(${NAME} PRIVATE c_std_11)
    target_compile_options(${NAME} PRIVATE /utf-8)
    # windows.rc already embeds manifest resource 1, including DPI settings.
    target_link_options(${NAME} PRIVATE /MANIFEST:NO)
    target_link_libraries(${NAME} PRIVATE cui)
    # Keep the CUI DLL and the SDK bootstrap DLL next to each executable.
    add_custom_command(TARGET ${NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:cui>" "$<TARGET_FILE_DIR:${NAME}>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_BINARY_DIR}/${CUI_WINUI_CONFIG}/Microsoft.WindowsAppRuntime.Bootstrap.dll" "$<TARGET_FILE_DIR:${NAME}>" VERBATIM)
endfunction()
if(CUI_BUILD_EXAMPLES)
    cui_windows_executable(cui_gallery examples/gallery.c examples/gallery_ui.c)
    cui_windows_executable(cui_settings examples/settings.c examples/settings_ui.c)
    cui_windows_executable(cui_showcase examples/showcase/main.c)
    cui_windows_executable(cui_desktop_gallery examples/desktop.c)
    cui_windows_executable(cui_chat_concepts examples/chat/main.c)
    set_target_properties(cui_gallery cui_settings cui_desktop_gallery
        PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
if(CUI_BUILD_TESTS)
    enable_testing()
    add_executable(cui_layout_test tests/layout.c src/cui_layout.c src/cui_layout_algorithms.c)
    target_include_directories(cui_layout_test PRIVATE include src)
    target_compile_features(cui_layout_test PRIVATE c_std_11)
    target_compile_options(cui_layout_test PRIVATE /UNDEBUG)
    add_test(NAME layout COMMAND cui_layout_test)
    cui_windows_executable(cui_chat_ux_test tests/chat_ux.c)
    target_include_directories(cui_chat_ux_test PRIVATE src)
    target_compile_options(cui_chat_ux_test PRIVATE /UNDEBUG)
    add_test(NAME chat_ux COMMAND cui_chat_ux_test)
    set_tests_properties(chat_ux PROPERTIES TIMEOUT 30)
    find_program(CUI_POWERSHELL NAMES powershell pwsh REQUIRED)
    add_test(NAME chat_ux_uia COMMAND ${CUI_POWERSHELL} -NoProfile -ExecutionPolicy Bypass
        -File ${PROJECT_SOURCE_DIR}/tests/chat_ux_windows.ps1 -Binary $<TARGET_FILE:cui_chat_ux_test>)
    set_tests_properties(chat_ux_uia PROPERTIES TIMEOUT 45)
    cui_windows_executable(cui_winui_test tests/winui.c)
    target_compile_options(cui_winui_test PRIVATE /UNDEBUG)
    add_test(NAME winui_contracts COMMAND cui_winui_test)
    set_tests_properties(winui_contracts PROPERTIES TIMEOUT 45)
endif()
install(FILES "$<TARGET_FILE:cui>" "${CMAKE_BINARY_DIR}/${CUI_WINUI_CONFIG}/Microsoft.WindowsAppRuntime.Bootstrap.dll" DESTINATION bin)
install(FILES "$<TARGET_LINKER_FILE:cui>" DESTINATION lib)
install(DIRECTORY include/ DESTINATION include FILES_MATCHING PATTERN "*.h")
