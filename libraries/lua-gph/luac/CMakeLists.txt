include_directories(${PROJECT_SOURCE_DIR}/lib/inc )

add_executable( myluac luac.cpp )

target_link_libraries( myluac PRIVATE  lua )

install( TARGETS
   myluac

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
