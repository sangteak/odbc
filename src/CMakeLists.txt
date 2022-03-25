# create .exe
add_executable(odbc
	main.cpp
)

target_include_directories(odbc
PUBLIC
	${CMAKE_SOURCE_DIR}/include
)

if(MSVC)	# Microsoft Visual C++ Compiler
	target_compile_options(odbc
	PUBLIC
		/std:c++latest	/W4	# MSVC 가 식별 가능한 옵션을 지정
	)	
endif()

if(MSVC)	# Microsoft Visual C++ Compiler
	target_compile_definitions(odbc
	PRIVATE
		NOMINMAX
		_CRT_SECURE_NO_WARNINGS
	)	
endif()