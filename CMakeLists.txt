
cmake_minimum_required (VERSION 3.4)
project(LabSoundDemo)

# Don't report that sample file installation up to date
set(CMAKE_INSTALL_MESSAGE LAZY)

set(LABSOUNDDEMO_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
configure_file("${LABSOUNDDEMO_ROOT}/LabSoundDemo.config.h" "${LABSOUNDDEMO_ROOT}/LabSoundDemo.h" @ONLY)

add_subdirectory(LabSound)

if (APPLE)
    set(PLATFORM_LIBS
        "-framework AudioToolbox"
        "-framework AudioUnit"
        "-framework Accelerate"
        "-framework Cocoa"
        "-framework CoreAudio"
        "-framework Metal"
        "-framework MetalKit"
        "-framework QuartzCore"
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

add_executable(LabSoundInteractive 
    LabSoundInteractive.cpp ImGuiGridSlider.cpp ImGuiGridSlider.h imgui-app/imgui_app.cpp)
target_link_libraries(LabSoundInteractive Lab::Sound ${PLATFORM_LIBS})
target_include_directories(LabSoundInteractive PRIVATE "${LABSOUNDDEMO_ROOT}")
if(WIN32)
    target_compile_definitions(LabSoundInteractive PRIVATE SOKOL_D3D11 SOKOL_WIN32_FORCE_MAIN)
elseif(APPLE)
    target_compile_definitions(LabSoundInteractive PRIVATE SOKOL_METAL)
    set_source_files_properties(imgui-app/imgui_app.cpp PROPERTIES
                            COMPILE_FLAGS "-x objective-c++")
elseif(UNIX)
    target_compile_definitions(LabSoundInteractive PRIVATE SOKOL_GLCORE33)
endif()
install(TARGETS LabSoundInteractive RUNTIME DESTINATION bin)
