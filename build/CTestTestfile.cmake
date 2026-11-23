# CMake generated Testfile for 
# Source directory: /home/ct/Documents/Resilio/Claude Projects/Yahtzee/yahtzee-gs
# Build directory: /home/ct/Documents/Resilio/Claude Projects/Yahtzee/yahtzee-gs/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(scoring "/home/ct/Documents/Resilio/Claude Projects/Yahtzee/yahtzee-gs/build/test_scoring")
set_tests_properties(scoring PROPERTIES  _BACKTRACE_TRIPLES "/home/ct/Documents/Resilio/Claude Projects/Yahtzee/yahtzee-gs/CMakeLists.txt;44;add_test;/home/ct/Documents/Resilio/Claude Projects/Yahtzee/yahtzee-gs/CMakeLists.txt;0;")
add_test(game "/home/ct/Documents/Resilio/Claude Projects/Yahtzee/yahtzee-gs/build/test_game")
set_tests_properties(game PROPERTIES  _BACKTRACE_TRIPLES "/home/ct/Documents/Resilio/Claude Projects/Yahtzee/yahtzee-gs/CMakeLists.txt;44;add_test;/home/ct/Documents/Resilio/Claude Projects/Yahtzee/yahtzee-gs/CMakeLists.txt;0;")
add_test(computer "/home/ct/Documents/Resilio/Claude Projects/Yahtzee/yahtzee-gs/build/test_computer" "/home/ct/Documents/Resilio/Claude Projects/Yahtzee/yahtzee-gs/tests/data/original_vectors.txt")
set_tests_properties(computer PROPERTIES  _BACKTRACE_TRIPLES "/home/ct/Documents/Resilio/Claude Projects/Yahtzee/yahtzee-gs/CMakeLists.txt;48;add_test;/home/ct/Documents/Resilio/Claude Projects/Yahtzee/yahtzee-gs/CMakeLists.txt;0;")
add_test(app "/home/ct/Documents/Resilio/Claude Projects/Yahtzee/yahtzee-gs/build/test_app")
set_tests_properties(app PROPERTIES  _BACKTRACE_TRIPLES "/home/ct/Documents/Resilio/Claude Projects/Yahtzee/yahtzee-gs/CMakeLists.txt;51;add_test;/home/ct/Documents/Resilio/Claude Projects/Yahtzee/yahtzee-gs/CMakeLists.txt;0;")
