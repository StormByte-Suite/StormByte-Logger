include(GNUInstallDirs)

file(GLOB_RECURSE STORMBYTE_LOGGER_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/*.cxx")

if(BUILD_SHARED_LIBS)
	set(_mode shared)
else()
	set(_mode static)
endif()

buildmaster_component(
	StormByte-Logger
	"StormByte Suite - Logger"
	"${STORMBYTE_LOGGER_SOURCES}"
	""
	${_mode}
	"StormByte-Logger"
	"BACKEND=host;ALIAS=StormByte::Logger"
)

if(BUILD_SHARED_LIBS)
	target_compile_definitions(StormByte-Logger PUBLIC STORMBYTE_LOGGER_SHARED)
endif()

set_target_properties(StormByte-Logger PROPERTIES
	LINKER_LANGUAGE CXX
	SOVERSION ${CMAKE_PROJECT_VERSION}
	VERSION ${CMAKE_PROJECT_VERSION}
	POSITION_INDEPENDENT_CODE ON
	C_VISIBILITY_PRESET hidden
	CXX_VISIBILITY_PRESET hidden
	VISIBILITY_INLINES_HIDDEN ON
)

if(MSVC)
	target_compile_options(StormByte-Logger PRIVATE
		/EHsc
		$<$<CONFIG:Release>:/O2>
		$<$<CONFIG:Debug>:/Od>
	)
else()
	target_compile_options(StormByte-Logger PRIVATE
		$<$<CONFIG:Debug>:-pipe -g -ggdb -Wall -Wextra -Wnon-virtual-dtor -pedantic -pedantic-errors -O0>
	)
endif()

target_include_directories(StormByte-Logger
	SYSTEM BEFORE PUBLIC
		"${CMAKE_CURRENT_LIST_DIR}/public"
		"${CMAKE_CURRENT_LIST_DIR}/private"
)
