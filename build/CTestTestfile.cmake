# CMake generated Testfile for 
# Source directory: /home/koratsuki/Desktop/QTerm2
# Build directory: /home/koratsuki/Desktop/QTerm2/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(smoketest "/home/koratsuki/Desktop/QTerm2/build/qterm2_smoketest")
set_tests_properties(smoketest PROPERTIES  ENVIRONMENT "QT_QPA_PLATFORM=offscreen" _BACKTRACE_TRIPLES "/home/koratsuki/Desktop/QTerm2/CMakeLists.txt;130;add_test;/home/koratsuki/Desktop/QTerm2/CMakeLists.txt;0;")
add_test(uitest "/home/koratsuki/Desktop/QTerm2/build/qterm2_uitest")
set_tests_properties(uitest PROPERTIES  ENVIRONMENT "QT_QPA_PLATFORM=offscreen" _BACKTRACE_TRIPLES "/home/koratsuki/Desktop/QTerm2/CMakeLists.txt;136;add_test;/home/koratsuki/Desktop/QTerm2/CMakeLists.txt;0;")
add_test(archtest "/home/koratsuki/Desktop/QTerm2/build/qterm2_archtest")
set_tests_properties(archtest PROPERTIES  ENVIRONMENT "QT_QPA_PLATFORM=offscreen" _BACKTRACE_TRIPLES "/home/koratsuki/Desktop/QTerm2/CMakeLists.txt;142;add_test;/home/koratsuki/Desktop/QTerm2/CMakeLists.txt;0;")
