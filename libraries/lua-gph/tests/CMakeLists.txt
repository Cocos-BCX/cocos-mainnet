include_directories(${PROJECT_SOURCE_DIR}/lib/inc )

find_package(Boost REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})


add_executable(tests
	main.cpp
	basic_readwrite.cpp
	advanced_readwrite.cpp
	execution.cpp
	functions_write.cpp
	functions_read.cpp
	custom_types.cpp
	movable.cpp
	metatables.cpp
	threads.cpp
)

target_link_libraries(tests lua pthread gtest gtest_main )
target_include_directories( tests  PUBLIC ${PROJECT_SOURCE_DIR}/lib/inc/wrapper )

install( TARGETS
   tests

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)

