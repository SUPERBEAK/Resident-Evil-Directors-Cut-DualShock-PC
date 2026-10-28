cmake_minimum_required(VERSION 3.20)
project(ResidentEvilDualshockPort LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_definitions(
    -D_CRT_SECURE_NO_WARNINGS
    -DUNICODE
    -D_UNICODE
    -DWIN32_LEAN_AND_MEAN
    -DNOMINMAX
    -DRE1_PORT_WIN64
)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../dualshock/include
)

set(PORT_HAL_SOURCES
    src/hal/psx_memory.cpp
    src/hal/psx_gte.cpp
    src/hal/psx_gpu.cpp
    src/hal/psx_cd.cpp
    src/hal/psx_task.cpp
    src/hal/psx_mdec.cpp
    src/hal/psx_tim.cpp
    src/hal/psx_tmd.cpp
    src/hal/psx_mc.cpp
    src/hal/psx_str.cpp
)

set(PORT_INPUT_SOURCES
    src/input/psx_pad.cpp
)

set(PORT_RENDERER_SOURCES
    src/renderer/dx12_renderer.cpp
    src/renderer/ps1_pipeline.cpp
    src/renderer/background_loader.cpp
    src/renderer/dlss_hook.cpp
)

set(PORT_AUDIO_SOURCES
    src/audio/audio_engine.cpp
    src/audio/xas_decoder.cpp
    src/audio/psx_seq.cpp
    src/spu/psx_spu.cpp
)

set(PORT_CORE_SOURCES
    src/main.cpp
    src/stage_manager.cpp
    src/room_manager.cpp
    src/game/player_character.cpp
    src/game/enemy_zombie.cpp
    src/game/inventory_system.cpp
    src/game/scd_vm.cpp
    src/game/text_box.cpp
    src/game/door_transition.cpp
    src/game/save_manager.cpp
    src/game/item_box.cpp
    src/game/puzzle_manager.cpp
    src/game/title_screen.cpp
)

add_executable(re1_dualshock_port
    ${PORT_CORE_SOURCES}
    ${PORT_HAL_SOURCES}
    ${PORT_INPUT_SOURCES}
    ${PORT_RENDERER_SOURCES}
    ${PORT_AUDIO_SOURCES}
)

target_link_libraries(re1_dualshock_port PRIVATE
    d3d12
    dxgi
    d3dcompiler
    xaudio2
    xinput
)

add_executable(extract_music
    src/extract_music.cpp
    src/audio/psx_seq.cpp
)
