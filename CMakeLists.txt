
cmake_minimum_required (VERSION 3.4)
project(LabSoundDemo)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(LABSOUNDDEMO_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
add_subdirectory(LabSound)

add_executable(LabSoundDemo LabSoundDemo.cpp)
target_link_libraries(LabSoundDemo Lab::Sound)
target_include_directories(LabSoundDemo PRIVATE "${LABSOUNDDEMO_ROOT}")

configure_file("${LABSOUNDDEMO_ROOT}/LabSoundDemo.config.h" "${LABSOUNDDEMO_ROOT}/LabSoundDemo.h" @ONLY)

install(TARGETS LabSoundDemo
        RUNTIME DESTINATION bin)
