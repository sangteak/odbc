# Root CMakeLists.txt
cmake_minimum_required(VERSION 3.10)

project(odbc)

list(APPEND	CMAKE_MODULE_PATH	${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(check-cxx-flag) # .cmake 생략하지 않을 경우 에러 발생

#if(cxx_latest)
#	target_compile_options(...)
#endif()

message(STATUS ${CMAKE_SOURCE_DIR})         # -- Root
message(STATUS ${CMAKE_CURRENT_SOURCE_DIR}) # -- Root

#if(MSVC)	# Microsoft Visual C++ Compiler
#	target_compile_options(my_exe
#	PUBLIC
#		/std:c++latest	/W4	# MSVC 가 식별 가능한 옵션을 지정
#	)	
#endif()

# add_subdirectory(include)
add_subdirectory(src)
