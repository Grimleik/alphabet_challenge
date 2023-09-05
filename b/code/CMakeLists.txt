cmake_minimum_required(VERSION 3.22.0)
project(age VERSION 0.1.0)

include(CTest)
enable_testing()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(AUDIO_ENABLED)
    include_directories(../externals/fmod/inc)
    if("${CMAKE_SIZEOF_VOID_P}" STREQUAL "8")
        include_directories(../externals/fmod/core/lib/x64/)
    else()
        include_directories(../externals/fmod/core/lib/x86/)
    endif()
endif()
# Windows Build
if(MSVC)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS=1)
    add_compile_options(-MTd -nologo -fp:fast -GR -EHa -EHsc -Zo -W4 -FC -Z7 -wd4100 -wd5246 -wd4201)

    if("${CMAKE_SIZEOF_VOID_P}" STREQUAL "8")
        add_executable(w64exe w64_main.cpp w64_sw_render.cpp age.cpp)
    else()
        add_executable(w32exe w32_main.cpp)
    endif()

#TODO Add others
endif()

# DLL Build
add_library(game SHARED game_breakout.cpp age.cpp)
if(AUDIO_ENABLED)
    if("${CMAKE_SIZEOF_VOID_P}" STREQUAL "8")
        target_link_libraries(game ../externals/fmod/core/lib/x64/fmod_vc)
    else()
        target_link_libraries(game ../externals/fmod/core/lib/x86/fmod_vc)
    endif()
endif()
add_compile_options(game PUBLIC -DLL -MTd -nologo -fp:fast -GR -EHa -EHsc -Zo -W4 -FC -Z7 -wd4100 -wd5246 -wd4201)
set_property(
    TARGET game
    PROPERTY PDB_NAME "game-$([System.DateTime]::Now.ToString(\"HH_mm_ss_fff\"))"
)
target_link_options(game PUBLIC -EXPORT:GameInit -EXPORT:GameUpdate -EXPORT:GameShutdown -incremental:no)
add_custom_command(
        TARGET game
        PRE_BUILD
        COMMAND del Debug\\game-*.pdb > NUL 2> NUL)
add_custom_command(
    TARGET game
    PRE_BUILD
    COMMAND echo lock > Debug\\pdb.lock)
add_custom_command(
    TARGET game
    POST_BUILD
    COMMAND del Debug\\pdb.lock)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
