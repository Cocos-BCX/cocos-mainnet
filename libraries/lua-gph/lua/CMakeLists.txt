include_directories(${PROJECT_SOURCE_DIR}/lib/inc )

add_executable( mylua lua.cpp )

target_link_libraries( mylua PRIVATE  lua  )


install( TARGETS
   mylua

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)


add_executable( lua_test lua_test.cpp )

target_link_libraries( lua_test PRIVATE  lua  )


install( TARGETS
   lua_test

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
