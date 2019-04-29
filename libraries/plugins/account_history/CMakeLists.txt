file(GLOB HEADERS "include/graphene/account_history/*.hpp")

add_library( graphene_account_history 
             account_history_plugin.cpp
           )

target_link_libraries( graphene_account_history graphene_chain graphene_app )
target_include_directories( graphene_account_history
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

if(MSVC)
  set_source_files_properties( account_history_plugin.cpp PROPERTIES COMPILE_FLAGS "/bigobj" )
endif(MSVC)

install( TARGETS
   graphene_account_history

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
INSTALL( FILES ${HEADERS} DESTINATION "include/graphene/account_history" )

