function(set_compile_property target_name)
	set_target_properties(
			${target_name}
			PROPERTIES
			CXX_STANDARD 20
			CXX_STANDARD_REQUIRED TRUE
	)

	if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
		set_target_properties(
				${target_name}
				PROPERTIES
				COMPILE_OPTIONS "/W4;/WX"
		)
	elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
		set_target_properties(
				${target_name}
				PROPERTIES
				COMPILE_OPTIONS "-Wall;-Wextra;-Wpedantic;-Werror"
		)
	else()
		message("Unknown Compiler ID: ${CMAKE_CXX_COMPILER_ID}")
	endif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
endfunction(set_compile_property)
