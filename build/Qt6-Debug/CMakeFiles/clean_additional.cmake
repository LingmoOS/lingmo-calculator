# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "CMakeFiles/lingmo-calculator_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/lingmo-calculator_autogen.dir/ParseCache.txt"
  "lingmo-calculator_autogen"
  )
endif()
