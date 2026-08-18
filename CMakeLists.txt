cmake_minimum_required(VERSION 3.16)

project(MTGScanner VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Quick Multimedia QuickControls2 Widgets Svg)
find_package(OpenCV CONFIG REQUIRED)
find_package(TBB CONFIG REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)
find_package(Eigen3 CONFIG REQUIRED)

include(cmake/3rdParty/onnxruntime.cmake)
find_package(onnxruntime CONFIG REQUIRED)

qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(${PROJECT_NAME}App
    src/main.cpp

    src/camera/availablecamerasmodel.h
    src/camera/availablecamerasmodel.cpp
    src/camera/cameracapture.h
    src/camera/cameracapture.cpp
    src/camera/cameramanager.h
    src/camera/cameramanager.cpp
    src/core/frame.hpp
    src/core/prediction.hpp
    src/engine/carddetector.cpp
    src/engine/channelraw.hpp
    src/engine/cardprocessor.h
    src/engine/cardprocessor.cpp
    src/engine/channelmodel.h
    src/engine/channelmodel.cpp
    src/engine/engine.h
    src/engine/engine.cpp
    src/output/nameplatdelegate.h
    src/output/nameplatdelegate.cpp
    src/output/nameplatemodel.h
    src/output/nameplatemodel.cpp
    src/output/outputwindow.cpp
    src/output/outputwindow.cpp
)

qt_add_resources(${CMAKE_PROJECT_NAME}App "configuration"
    PREFIX "/"
    FILES
        qtquickcontrols2.conf
)

add_subdirectory(MTGScanner)
add_subdirectory(3rdParty/ByteTrack-cpp)

set_target_properties(${PROJECT_NAME}App PROPERTIES
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_include_directories(${PROJECT_NAME}App
    PRIVATE

    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME}App
    PRIVATE

    Qt6::Quick
    Qt6::Multimedia
    Qt6::Widgets
    Qt6::Svg

    TBB::tbb
    ${OpenCV_LIBS}
    onnxruntime::onnxruntime
    yaml-cpp::yaml-cpp
    bytetrack

    MTGScanner  # the static library, just for channeloptions.hpp
    MTGScannerplugin
    MTGScannerComponentsplugin
)

include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME}App
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Copy the compile_commands.json for tools like clangd
if (CMAKE_EXPORT_COMPILE_COMMANDS)
    message(STATUS "Copying compile_commands.json")
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E copy_if_different 
        "${CMAKE_BINARY_DIR}/compile_commands.json" 
        "${CMAKE_SOURCE_DIR}/compile_commands.json"
    )
endif()

include(cmake/helpers.cmake)
mtgs_copy_assets(ModelAssets "${CMAKE_CURRENT_SOURCE_DIR}/assets" "${CMAKE_CURRENT_BINARY_DIR}")
add_dependencies(${PROJECT_NAME}App ModelAssets)