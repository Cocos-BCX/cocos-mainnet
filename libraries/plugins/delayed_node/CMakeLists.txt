file(GLOB HEADERS "include/graphene/delayed_node/*.hpp")

add_library( graphene_delayed_node 
             delayed_node_plugin.cpp
           )

target_link_libraries( graphene_delayed_node graphene_chain graphene_app )
target_include_directories( graphene_delayed_node
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

if(MSVC)
  set_source_files_properties( delayed_node_plugin.cpp PROPERTIES COMPILE_FLAGS "/bigobj" )
endif(MSVC)

install( TARGETS
   graphene_delayed_node

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
