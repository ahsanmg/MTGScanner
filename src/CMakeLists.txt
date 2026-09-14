qt_add_library(${PROJECT_NAME}Lib STATIC
    camera/availablecamerasmodel.cpp
    camera/cameracapture.cpp
    camera/cameramanager.cpp
    core/frame.hpp
    engine/carddetector.cpp
    engine/channelraw.hpp
    engine/cardprocessor.cpp
    engine/channelmodel.cpp
    engine/engine.cpp
    output/nameplatdelegate.cpp
    output/nameplatemodel.cpp
    output/outputwindow.cpp
    output/outputwindow.cpp
)

target_include_directories(${PROJECT_NAME}Lib
    PUBLIC

    ${CMAKE_CURRENT_SOURCE_DIR}
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME}Lib
    PUBLIC

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
)