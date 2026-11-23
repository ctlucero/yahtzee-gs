cmake_minimum_required(VERSION 3.16)
project(yahtzee-gs VERSION 1.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_QT_APP "Build the Qt 6 application" ON)
option(BUILD_TESTING "Build the tests" ON)

set(APP_ID io.github.yahtzee_gs.Yahtzee)

# Game logic: plain C++, no Qt dependency.
add_library(yahtzee_core STATIC
    src/core/scoring.cpp
    src/core/computer.cpp
    src/core/game.cpp
)
target_include_directories(yahtzee_core PUBLIC src/core)

# The IIgs screen and the original's event handling: plain C++ too.
add_library(yahtzee_screen STATIC
    src/screen/canvas.cpp
    src/screen/app.cpp
)
target_include_directories(yahtzee_screen PUBLIC src/screen)
target_link_libraries(yahtzee_screen PUBLIC yahtzee_core)

if(BUILD_QT_APP)
    find_package(Qt6 REQUIRED COMPONENTS Widgets)
    add_executable(yahtzee-gs src/qt/main.cpp)
    target_link_libraries(yahtzee-gs PRIVATE yahtzee_screen Qt6::Widgets)
    include(GNUInstallDirs)
    install(TARGETS yahtzee-gs RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    # Point the launcher entry at the installed program by full path, so it
    # works even when the install prefix's bin is not on the session's PATH.
    file(READ packaging/${APP_ID}.desktop _desktop)
    string(REGEX REPLACE "\nExec=[^\n]*" "\nExec=${CMAKE_INSTALL_FULL_BINDIR}/yahtzee-gs" _desktop "${_desktop}")
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/${APP_ID}.desktop "${_desktop}")
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${APP_ID}.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES packaging/${APP_ID}.metainfo.xml DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
    install(FILES packaging/${APP_ID}.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
endif()

if(BUILD_TESTING)
    enable_testing()
    foreach(t scoring game)
        add_executable(test_${t} tests/test_${t}.cpp)
        target_link_libraries(test_${t} PRIVATE yahtzee_core)
        add_test(NAME ${t} COMMAND test_${t})
    endforeach()
    add_executable(test_computer tests/test_computer.cpp)
    target_link_libraries(test_computer PRIVATE yahtzee_core)
    add_test(NAME computer COMMAND test_computer ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/original_vectors.txt)
    add_executable(test_app tests/test_app.cpp)
    target_link_libraries(test_app PRIVATE yahtzee_screen)
    add_test(NAME app COMMAND test_app)
endif()
