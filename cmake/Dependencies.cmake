# Use an optional local dependency cache; otherwise the SDK fetches its pinned
# upstream versions. No multi-game checkout is required.
get_filename_component(SEGAGOLF_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
foreach(dep zydis tomlplusplus imgui sdl3 vma)
    string(TOUPPER "${dep}" upper)
    if(NOT DEFINED FETCHCONTENT_SOURCE_DIR_${upper} AND
       IS_DIRECTORY "${SEGAGOLF_ROOT}/third_party/${dep}-src")
        set(FETCHCONTENT_SOURCE_DIR_${upper}
            "${SEGAGOLF_ROOT}/third_party/${dep}-src" CACHE PATH "Cached ${dep}")
    endif()
endforeach()
if(DEFINED ENV{DXC_SOURCE} AND NOT FETCHCONTENT_SOURCE_DIR_DXC)
    set(FETCHCONTENT_SOURCE_DIR_DXC "$ENV{DXC_SOURCE}" CACHE PATH "Local DXC distribution")
endif()
