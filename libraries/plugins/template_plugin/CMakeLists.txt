file(GLOB HEADERS "include/graphene/template_plugin/*.hpp")

add_library( graphene_template_plugin
        template_plugin.cpp
           )

target_link_libraries( graphene_template_plugin graphene_chain graphene_app )
target_include_directories( graphene_template_plugin
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

if(MSVC)
  set_source_files_properties(template_plugin.cpp PROPERTIES COMPILE_FLAGS "/bigobj" )
endif(MSVC)

install( TARGETS
   graphene_template_plugin

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
INSTALL( FILES ${HEADERS} DESTINATION "include/graphene/template_plugin" )
