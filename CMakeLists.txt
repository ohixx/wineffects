cmake_minimum_required(VERSION 3.20)
project(wineffects VERSION 0.5.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

include(FetchContent)
FetchContent_Declare(imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG        v1.91.9b
  GIT_SHALLOW    TRUE)
# The next ones are header-only or plain C sources: fetch them without running their build files.
FetchContent_Declare(miniaudio
  GIT_REPOSITORY https://github.com/mackron/miniaudio.git
  GIT_TAG        0.11.24
  GIT_SHALLOW    TRUE
  SOURCE_SUBDIR  none)
FetchContent_Declare(signalsmith_stretch
  GIT_REPOSITORY https://github.com/Signalsmith-Audio/signalsmith-stretch.git
  GIT_TAG        1.4.0
  GIT_SHALLOW    TRUE
  SOURCE_SUBDIR  none)
FetchContent_Declare(signalsmith_linear
  GIT_REPOSITORY https://github.com/Signalsmith-Audio/linear.git
  GIT_TAG        0.6.4
  GIT_SHALLOW    TRUE
  SOURCE_SUBDIR  none)
FetchContent_MakeAvailable(imgui miniaudio signalsmith_stretch signalsmith_linear)

# Dear ImGui
add_library(imgui STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_compile_definitions(imgui PUBLIC IMGUI_DISABLE_DEMO_WINDOWS IMGUI_DISABLE_DEBUG_TOOLS)

# RNNoise 0.2 (BSD-3-Clause), vendored in third_party/rnnoise. The model weights are a single binary
# blob that is embedded into the executable (see src/rnnoise_blob.cpp).
set(RNNOISE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/rnnoise)
add_library(rnnoise STATIC
  ${RNNOISE_DIR}/src/denoise.c
  ${RNNOISE_DIR}/src/rnn.c
  ${RNNOISE_DIR}/src/rnnoise_data.c
  ${RNNOISE_DIR}/src/rnnoise_tables.c
  ${RNNOISE_DIR}/src/nnet.c
  ${RNNOISE_DIR}/src/nnet_default.c
  ${RNNOISE_DIR}/src/parse_lpcnet_weights.c
  ${RNNOISE_DIR}/src/pitch.c
  ${RNNOISE_DIR}/src/kiss_fft.c
  ${RNNOISE_DIR}/src/celt_lpc.c)
target_include_directories(rnnoise PUBLIC ${RNNOISE_DIR}/include PRIVATE ${RNNOISE_DIR}/src)
target_compile_definitions(rnnoise PRIVATE USE_WEIGHTS_FILE)
if(NOT MSVC)
  target_compile_options(rnnoise PRIVATE -O3 -w)
  target_link_libraries(rnnoise PRIVATE m)
endif()

add_executable(wineffects WIN32
  src/main.cpp
  src/app.cpp
  src/engine.cpp
  src/miniaudio_impl.cpp
  src/rnnoise_blob.cpp
  src/effects.cpp
  src/platform.cpp
  src/ui.cpp)
set_source_files_properties(src/rnnoise_blob.cpp PROPERTIES OBJECT_DEPENDS ${RNNOISE_DIR}/weights_blob.bin)
target_include_directories(wineffects PRIVATE
  ${miniaudio_SOURCE_DIR}
  ${signalsmith_stretch_SOURCE_DIR}/include
  ${signalsmith_linear_SOURCE_DIR}/include)
target_link_libraries(wineffects PRIVATE imgui rnnoise d3d11 dxgi d3dcompiler dwmapi ole32 shell32 advapi32 gdi32 winmm)
target_compile_definitions(wineffects PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN
  RNNOISE_BLOB_PATH="${RNNOISE_DIR}/weights_blob.bin"
  MA_NO_ENCODING MA_NO_DECODING MA_NO_GENERATION MA_NO_RESOURCE_MANAGER MA_NO_NODE_GRAPH MA_NO_ENGINE)

if(MINGW)
  target_link_options(wineffects PRIVATE -static -mwindows)
endif()
