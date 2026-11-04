cmake_minimum_required(VERSION 3.21)
project(kesShaderGraph VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets OpenGLWidgets)
qt_standard_project_setup()

add_library(sne_core STATIC
    src/Types.h
    src/NodeLibrary.h
    src/NodeLibrary.cpp
    src/GraphModel.h
    src/GraphModel.cpp
    src/SlangCodeGen.h
    src/SlangCodeGen.cpp
    src/ExampleGraphs.h
    src/ExampleGraphs.cpp
)
target_include_directories(sne_core PUBLIC src)
target_link_libraries(sne_core PUBLIC Qt6::Core Qt6::Gui)

qt_add_executable(kesShaderGraph
    src/main.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    src/GraphItems.h
    src/GraphItems.cpp
    src/NodeGraphScene.h
    src/NodeGraphScene.cpp
    src/NodeGraphView.h
    src/NodeGraphView.cpp
    src/NodeSearchPopup.h
    src/NodeSearchPopup.cpp
    src/InspectorWidget.h
    src/InspectorWidget.cpp
    src/SlangHighlighter.h
    src/SlangHighlighter.cpp
)
target_link_libraries(kesShaderGraph PRIVATE sne_core Qt6::Widgets Qt6::OpenGLWidgets)

# Runtime window icon (title bar, taskbar on Linux), available as :/icons/icon.png
qt_add_resources(kesShaderGraph "app_icon"
    PREFIX "/icons"
    BASE resource
    FILES resource/icon.png
)

if(WIN32)
    target_sources(kesShaderGraph PRIVATE resource/kesShaderGraph.rc)
    add_custom_command(TARGET kesShaderGraph POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:Qt6::OpenGL> $<TARGET_FILE:Qt6::OpenGLWidgets> $<TARGET_FILE_DIR:kesShaderGraph>
    )

    find_program(WINDEPLOYQT_EXECUTABLE windeployqt6
            HINTS "${Qt6_DIR}/../../../bin"
    )

    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET kesShaderGraph POST_BUILD
                COMMAND "${WINDEPLOYQT_EXECUTABLE}" "$<TARGET_FILE:kesShaderGraph>"
                COMMENT "Deploying Qt dependencies with windeployqt6"
        )
    else()
        message(WARNING "windeployqt6 not found; Qt deployment skipped")
    endif()
endif()
set_target_properties(kesShaderGraph PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
)

if(MSVC)
    target_compile_options(kesShaderGraph PRIVATE /W4)
    target_compile_options(sne_core PRIVATE /W4)
else()
    target_compile_options(kesShaderGraph PRIVATE -Wall -Wextra)
    target_compile_options(sne_core PRIVATE -Wall -Wextra)
endif()

install(TARGETS kesShaderGraph
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)

if(UNIX AND NOT APPLE)
    # Desktop entry and themed icon, used by launchers and Wayland compositors
    include(GNUInstallDirs)
    install(FILES resource/kesShaderGraph.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
    )
    install(FILES resource/icon.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps
        RENAME kesShaderGraph.png
    )
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(codegen_test tests/codegen_test.cpp)
    target_link_libraries(codegen_test PRIVATE sne_core)
    add_test(NAME codegen_test COMMAND codegen_test)
endif()
