add_executable(footprintconv)
target_sources(footprintconv
	PRIVATE
		footprintconv.cpp
)

if (WIN32)
	target_link_libraries(footprintconv ${ZLIB_LIBRARIES})
	target_include_directories(footprintconv PRIVATE ${ZLIB_INCLUDE_DIR})
endif()

install(TARGETS footprintconv RUNTIME DESTINATION bin)