
# Takes in a target and its files
function(mtgs_add_test target)
    qt_add_executable(${target} ${ARGN})

    target_link_libraries(${target}
        PRIVATE
            Qt6::Test
            ${PROJECT_NAME}
            ${PROJECT_NAME}Lib
    )

    add_test(NAME ${target} COMMAND ${target})
endfunction()

mtgs_add_test(test_channelmodel test_channelmodel.cpp)
mtgs_add_test(test_channelmetrics test_channelmetrics.cpp)
mtgs_add_test(test_framespersecond test_framespersecond.cpp)
mtgs_add_test(test_nameplatemodel test_nameplatemodel.cpp)

mtgs_add_test(test_nameplatdelegate test_nameplatdelegate.cpp)
set_tests_properties(test_nameplatdelegate PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

mtgs_add_test(test_cameramanager test_cameramanager.cpp)
mtgs_add_test(test_availablecamerasmodel test_availablecamerasmodel.cpp)
mtgs_add_test(test_cameracapture test_cameracapture.cpp)
mtgs_add_test(test_cardprocessor test_cardprocessor.cpp)
mtgs_add_test(test_carddetector test_carddetector.cpp)

mtgs_add_test(test_predictionoverlay test_predictionoverlay.cpp)
set_tests_properties(test_predictionoverlay PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

mtgs_add_test(test_outputwindow test_outputwindow.cpp)
set_tests_properties(test_outputwindow PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

mtgs_add_test(test_channel test_channel.cpp)
set_tests_properties(test_channel PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

mtgs_add_test(test_engine test_engine.cpp)
target_compile_definitions(test_engine PRIVATE NDEBUG)
set_tests_properties(test_engine PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
)

mtgs_add_test(test_image test_image.cpp)
