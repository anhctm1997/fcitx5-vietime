cmake_minimum_required(VERSION 3.20)
project(fcitx5-vietime LANGUAGES CXX)

include(GNUInstallDirs)
find_package(Fcitx5Core REQUIRED)
find_package(Fcitx5Config REQUIRED)

set(VIETIME_RUST_LIBRARY "${CMAKE_SOURCE_DIR}/../target/release/libvietime_ffi.a"
    CACHE FILEPATH "Path to the VietIME Rust static library")
if(NOT EXISTS "${VIETIME_RUST_LIBRARY}")
    message(FATAL_ERROR "Rust library not found: ${VIETIME_RUST_LIBRARY}. Build vietime-ffi first.")
endif()

add_library(vietime MODULE vietime.cpp vietime_config.cpp)
target_compile_features(vietime PRIVATE cxx_std_20)
target_include_directories(vietime PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(vietime PRIVATE Fcitx5::Core Fcitx5::Config "${VIETIME_RUST_LIBRARY}" dl m pthread)
set_target_properties(vietime PROPERTIES
    PREFIX ""
    OUTPUT_NAME "vietime"
    INSTALL_RPATH ""
)

install(TARGETS vietime DESTINATION "${CMAKE_INSTALL_LIBDIR}/fcitx5")
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/../data/vietime-addon.conf.in"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/fcitx5/addon"
        RENAME vietime.conf)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/../data/vietime.conf.in"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/fcitx5/inputmethod"
        RENAME vietime.conf)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../gnome-extension/vietime@vietime.invalid"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/gnome-shell/extensions")

option(VIETIME_BUILD_QT_EDITOR "Build the Qt configuration editor" ON)
if(VIETIME_BUILD_QT_EDITOR)
    find_package(Qt6 COMPONENTS Core DBus Widgets REQUIRED)
    find_package(Fcitx5Qt6WidgetsAddons REQUIRED)
    add_subdirectory(config-editor)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(vietime-config-test vietime_config_test.cpp vietime_config.cpp)
    target_compile_features(vietime-config-test PRIVATE cxx_std_20)
    target_link_libraries(vietime-config-test PRIVATE Fcitx5::Core Fcitx5::Config)
    add_test(NAME vietime-config-test COMMAND vietime-config-test)
endif()
