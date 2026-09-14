cmake_minimum_required(VERSION 3.16)

project(MTGScanner VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(UNIX AND NOT APPLE)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath,'$$ORIGIN'")
endif()

set(QT_VERSION "6.9")
find_package(Qt6 ${QT_VERSION} REQUIRED COMPONENTS Quick Multimedia QuickControls2 Widgets Svg)
find_package(OpenCV CONFIG REQUIRED)
find_package(TBB CONFIG REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)
find_package(Eigen3 CONFIG REQUIRED)

include(cmake/3rdParty/onnxruntime.cmake)
find_package(onnxruntime CONFIG REQUIRED)

qt_standard_project_setup(REQUIRES ${QT_VERSION})

qt_add_executable(${PROJECT_NAME}App
    src/main.cpp
)

qt_add_resources(${CMAKE_PROJECT_NAME}App "configuration"
    PREFIX "/"
    FILES
        qtquickcontrols2.conf
)

add_subdirectory(MTGScanner)
add_subdirectory(3rdParty/ByteTrack-cpp)
add_subdirectory(src)

set_target_properties(${PROJECT_NAME}App PROPERTIES
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(${PROJECT_NAME}App
    PRIVATE

    Qt6::Quick
    ${PROJECT_NAME}Lib
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
mtgs_copy_dlls(OrtDlls ${onnxruntime_ROOT})
add_dependencies(${PROJECT_NAME}App ModelAssets)
add_dependencies(${PROJECT_NAME}App OrtDlls)