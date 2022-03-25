include(CheckCXXCompilerFlag)

message(STATUS "Compiler")
message(STATUS " - ID	\t: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS " - Version	\t: ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS " - Path	\t: ${CMAKE_CXX_COMPILER}")

if(MSVC)	# Microsoft Visual C++ Compiler
	check_cxx_compiler_flag(/std:c++latest	cxx_latest			)
	check_cxx_compiler_flag(/W4				high_warning_level	)
elseif(${CMAKE_CXX_COMPILER_ID} MATCHES Clang)	#Clang + AppleClang
	check_cxx_compiler_flag(-std=c++2a		cxx_latest			)
	check_cxx_compiler_flag(/Wall			high_warning_level	)
elseif(${CMAKE_CXX_COMPILER_ID} MATCHES GNU)	#GNU C Compiler
	check_cxx_compiler_flag(-std=gnu+++2a		cxx_latest			)
	check_cxx_compiler_flag(/Wextra				high_warning_level	)
else()	# etc
	# .. 
endif()
