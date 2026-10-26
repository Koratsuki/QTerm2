# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "CMakeFiles/QTerm2_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/QTerm2_autogen.dir/ParseCache.txt"
  "CMakeFiles/qterm2_app_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/qterm2_app_autogen.dir/ParseCache.txt"
  "CMakeFiles/qterm2_archtest_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/qterm2_archtest_autogen.dir/ParseCache.txt"
  "CMakeFiles/qterm2_core_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/qterm2_core_autogen.dir/ParseCache.txt"
  "CMakeFiles/qterm2_smoketest_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/qterm2_smoketest_autogen.dir/ParseCache.txt"
  "CMakeFiles/qterm2_uitest_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/qterm2_uitest_autogen.dir/ParseCache.txt"
  "QTerm2_autogen"
  "qterm2_app_autogen"
  "qterm2_archtest_autogen"
  "qterm2_core_autogen"
  "qterm2_smoketest_autogen"
  "qterm2_uitest_autogen"
  )
endif()
