cmake_minimum_required (VERSION 3.11)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE INTERNAL "")

set(BOTCRAFT_COMPRESSION ON CACHE INTERNAL "")
set(BOTCRAFT_ENCRYPTION ON CACHE INTERNAL "")
set(BOTCRAFT_BUILD_TESTS OFF CACHE INTERNAL "")
set(BOTCRAFT_BUILD_EXAMPLES OFF CACHE INTERNAL "")
set(BOTCRAFT_USE_OPENGL_GUI ON CACHE INTERNAL "")
set(BOTCRAFT_USE_IMGUI ON CACHE INTERNAL "")

include(FetchContent)
# --- Fetch FTXUI --------------------------------------------------------------

#set(FETCHCONTENT_UPDATES_DISCONNECTED TRUE)
#FetchContent_Declare(ftxui
#  GIT_REPOSITORY https://github.com/ArthurSonzogni/ftxui
#  GIT_TAG v3.0.0
#  GIT_SHALLOW TRUE
#)

#FetchContent_GetProperties(ftxui)
#if(NOT ftxui_POPULATED)
#  FetchContent_Populate(ftxui)
#  add_subdirectory(${ftxui_SOURCE_DIR} ${ftxui_BINARY_DIR} EXCLUDE_FROM_ALL)
#endif()

# --- Fetch BotCraft --------------------------------------------------------------
set(FETCHCONTENT_UPDATES_DISCONNECTED TRUE)
FetchContent_Declare(botcraft
    #GIT_REPOSITORY https://github.com/adepierre/Botcraft
    GIT_REPOSITORY https://github.com/coldelectrons/BotcraftExperimental
    #GIT_TAG latest
  GIT_SHALLOW TRUE
)

FetchContent_GetProperties(botcraft)
if(NOT botcraft_POPULATED)
  FetchContent_Populate(botcraft)
  add_subdirectory(${botcraft_SOURCE_DIR} ${botcraft_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()
# ------------------------------------------------------------------------------

project(minion
  LANGUAGES CXX
  VERSION 1.0.0
)

add_executable(minion
    src/main.cpp
    src/ChatCommandClient.cpp
    src/MapCreationTasks.cpp
)
target_include_directories(minion PRIVATE src)

target_link_libraries(minion
    #PRIVATE ftxui::screen
    #PRIVATE ftxui::dom
    #PRIVATE ftxui::component # Not needed for this example.
  PRIVATE botcraft
)

