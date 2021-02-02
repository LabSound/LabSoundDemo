
cmake_minimum_required (VERSION 3.4)
project(LabSoundDemo)

set(LABSOUNDDEMO_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
configure_file("${LABSOUNDDEMO_ROOT}/LabSoundDemo.config.h" "${LABSOUNDDEMO_ROOT}/LabSoundDemo.h" @ONLY)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
add_subdirectory(LabSound)

if (APPLE)
    set(PLATFORM_LIBS
        "-framework AudioToolbox"
        "-framework AudioUnit"
        "-framework Accelerate"
        "-framework Cocoa"
        "-framework CoreAudio"
#        "-framework Metal"
#        "-framework MetalKit"
#        "-framework QuartzCore"
        )
endif()

add_executable(LabSoundStarter LabSoundStarter.cpp)
target_link_libraries(LabSoundStarter Lab::Sound ${PLATFORM_LIBS})
target_include_directories(LabSoundStarter PRIVATE "${LABSOUNDDEMO_ROOT}")
install(TARGETS LabSoundStarter RUNTIME DESTINATION bin)

add_executable(LabSoundDemo LabSoundDemo.cpp)
target_link_libraries(LabSoundDemo Lab::Sound ${PLATFORM_LIBS})
target_include_directories(LabSoundDemo PRIVATE "${LABSOUNDDEMO_ROOT}")
install(TARGETS LabSoundDemo RUNTIME DESTINATION bin)
